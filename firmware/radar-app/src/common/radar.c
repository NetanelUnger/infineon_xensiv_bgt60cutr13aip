/* ===========================================================================
** Copyright (C) 2022-2023 Infineon Technologies AG
** All rights reserved.
** ===========================================================================
**
** ===========================================================================
** This document contains proprietary information of Infineon Technologies AG.
** Passing on and copying of this document, and communication of its contents
** is not permitted without Infineon's prior written authorization.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
** ===========================================================================
*/

#include <stdbool.h>
#include <string.h>
#include "cyhal.h"
#include "cybsp.h"
#include "common.h"
#include "portable.h"
#include "radar_motion_detection.h"
#include "usecase.h"
#include "resource_map.h"

#include "xensiv_bgt60cutrxx_mtb.h"
#include "xensiv_bgt60cutrxx_regs.h"

#include "motion_detection.h"

#include "radar_config.h"
#include "SEGGER_RTT.h"

#define RADAR_TASK_STACK_DEPTH_WORDS 2048   /* number of words according to configSTACK_DEPTH_TYPE */

QueueHandle_t radar_cmd_queue = NULL;
extern xensiv_bgt60cutrxx_mtb_iface_config_t mcu_iface_config;

/**
 * calculates the number of elements of a two dimensional array
 */

static xensiv_bgt60cutrxx_mtb_t sensor = { 0 };

static bool radar_enable_state = false;
static bool motion_detector_enabled = false;
static TaskHandle_t radar_task_handle;
// The FIFO_THRESHOLD level for (FIFO_CTRL) will be also sent by the host as `meta.raw` info when setting a new reg
// list.
static uint32_t current_fifo_threshold = FIFO_THRESHOLD;

static void handle_radar_data_ready(void);
static void handle_radar_msg_get_registers(register_list_t* regs);
static void handle_radar_msg_set_registers(const register_list_t* regs);


#define MAGIC_STR            "[START]"
#define MAGIC_LEN            7u
#define FRAME_NUM_LEN        4u  // uint32_t frame number

/* Sized to hold about nine ADC frames, so host read delays do not immediately drop frames. */
#define RTT_BUFFER_SIZE_RAW_DATA_FRAMES (64u * 1024u)

static uint8_t rtt_raw_data_frame_buffer[RTT_BUFFER_SIZE_RAW_DATA_FRAMES];

#ifndef IFX_USECASE_RADAR_FRAME_CHIRPS
#define IFX_USECASE_RADAR_FRAME_CHIRPS 17
#endif

#ifndef IFX_USECASE_RADAR_FRAME_SAMPLES
#define IFX_USECASE_RADAR_FRAME_SAMPLES 64
#endif

#ifndef IFX_USECASE_RADAR_FRAME_RX_ANTENNAS
#define IFX_USECASE_RADAR_FRAME_RX_ANTENNAS 3
#endif


uint32_t current_frame_number = 0;

static meta_raw_t raw_frame_shape =
{
    .number_of_chirps  = IFX_USECASE_RADAR_FRAME_CHIRPS,
    .samples_per_chirp = IFX_USECASE_RADAR_FRAME_SAMPLES,
    .rx_antennas       = IFX_USECASE_RADAR_FRAME_RX_ANTENNAS
};


/* Interrupt handler to react on sensor indicating the availability of new data */
static void xensiv_bgt60trxx_mtb_interrupt_handler(void* args, cyhal_gpio_event_t event)
{
    CY_UNUSED_PARAMETER(args);
    CY_UNUSED_PARAMETER(event);

    /* Safety check: ensure queue is initialized */
    if (!radar_cmd_queue)
    {
        return;  // Queue not ready yet, discard this interrupt
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    radar_msg_t msg = { .type         = RADAR_MSG_RADAR_DATA_READY,
                        .payload      = NULL,
                        .payload_size = 0,
                        .reply_task   = NULL };

    xQueueSendFromISR(radar_cmd_queue, &msg, &xHigherPriorityTaskWoken);
    /* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/* Deviation from MISRA C-2012 Rule 17.11: FreeRTOS task functions run in infinite loop and never return */
static void _Noreturn radar_task(void* pvParameters)
{
    #ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
    /* FreeRTOS will start execution from this point and create the timer daemon */
    const uint32_t heap_before_timer_daemon_creation = get_used_heap_size();
    #endif

    (void)pvParameters;
    /* Create radar command queue FIRST, before any ISR can fire */
    if (!radar_cmd_queue)
    {
        radar_cmd_queue = xQueueCreate(8, sizeof(radar_msg_t));
        if (!radar_cmd_queue)
        {
            while (true)
            {
                CY_ASSERT(0);  // Cannot continue without a queue
            }
        }
    }

    cy_rslt_t result;
    result = xensiv_bgt60cutrxx_platform_open(&sensor.iface, &mcu_iface_config);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        while (true)
        {
            CY_ASSERT(0);  // Cannot continue when platform open fails
        }
    }

    result = xensiv_bgt60cutrxx_platform_init(&sensor, &mcu_iface_config, register_list, ARRAY_SIZE(
                                                  register_list), false, false);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        while (true)
        {
            CY_ASSERT(0);  // Cannot continue when initialization fails
        }
    }
    result = xensiv_bgt60cutrxx_platform_irq_init(&sensor, &mcu_iface_config, &xensiv_bgt60trxx_mtb_interrupt_handler,
                                                  NULL);
    #ifdef RADAR_MOTION_DETECTION
    result = motion_detection_init(&sensor.dev);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != result)
    {
        while (true)
        {
            CY_ASSERT(0);  // Cannot continue when initialization fails
        }
    }
    #endif

    #ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
    /* FreeRTOS task will start execution from this point */
    const uint32_t heap_at_system_start = get_used_heap_size();
    memory_report_data.timer_daemon_heap_usage = heap_at_system_start - heap_before_timer_daemon_creation;
    memory_report_data.system_minimum_heap_usage = heap_at_system_start;
    #endif

    /* start acquisition */
    radar_enable(START_FRAME_AFTER_POR);

    for (;;)
    {
        radar_msg_t cmd_msg = { 0 };
        BaseType_t got_cmd = xQueueReceive(radar_cmd_queue, &cmd_msg, portMAX_DELAY);
        /* Wait for the GPIO interrupt to indicate that radar raw data available */
        if (got_cmd == pdFALSE)
        {
            continue;
        }

        switch (cmd_msg.type)
        {
            case RADAR_MSG_RADAR_DATA_READY:
                current_frame_number++;
                handle_radar_data_ready();
                break;

            case RADAR_MSG_GET_REGISTERS:
                handle_radar_msg_get_registers((register_list_t*)cmd_msg.payload);
                break;

            case RADAR_MSG_SET_REGISTERS:
                handle_radar_msg_set_registers((const register_list_t*)cmd_msg.payload);
                break;

            default:
                /* No action required for unknown message types */
                break;
        }

        if (cmd_msg.reply_task)
        {
            xTaskNotifyGive(cmd_msg.reply_task);
        }
    }
}


bool send_frame_via_rtt(uint16_t* frame_data, uint32_t frame_number, uint32_t frame_length)
{
    // Header: MAGIC_STR + frame_number (4 bytes)
    static uint8_t header[MAGIC_LEN + FRAME_NUM_LEN] = MAGIC_STR;

    uint32_t frame_length_bytes = frame_length * sizeof(uint16_t);
    uint32_t message_length = MAGIC_LEN + FRAME_NUM_LEN + frame_length_bytes;

    /* The ring buffer can hold at most one byte less than its size. */
    if (message_length >= sizeof(rtt_raw_data_frame_buffer))
    {
        debug_print("Frame size exceeds RTT buffer size, cannot send frame via RTT");
        return false;
    }

    /* Header and payload are two writes in skip mode, so reserve space for both to never emit a partial frame. */
    if (SEGGER_RTT_GetAvailWriteSpace(RTT_UP_CHANNEL_RAW_DATA_FRAMES) < message_length)
    {
        return false;
    }

    // "frame_number" in little-endian byte order.
    header[MAGIC_LEN + 0] = (frame_number >> 0) & 0xFF;
    header[MAGIC_LEN + 1] = (frame_number >> 8) & 0xFF;
    header[MAGIC_LEN + 2] = (frame_number >> 16) & 0xFF;
    header[MAGIC_LEN + 3] = (frame_number >> 24) & 0xFF;
    uint32_t bytes_written = SEGGER_RTT_Write(RTT_UP_CHANNEL_RAW_DATA_FRAMES, header, MAGIC_LEN + FRAME_NUM_LEN);

    bytes_written += SEGGER_RTT_Write(RTT_UP_CHANNEL_RAW_DATA_FRAMES, frame_data, frame_length_bytes);
    bool success = (bytes_written == message_length);

    return success;
}


void radar_enable(bool enable)
{
    /* Enable back radar */
    if (enable != radar_enable_state)
    {
        if (!enable)
        {
            xensiv_bgt60cutrxx_soft_reset(&(sensor.dev), RESET_FSM);
        }
        else
        {
            if (!motion_detector_enabled)
            {
                xensiv_bgt60cutrxx_trigger_sequence0(&(sensor.dev)); // from sequence 0
            }
        }
        radar_enable_state = enable;
    }
}


void radar_task_init(void)
{
    #ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
    const uint32_t heap_before_task_creation = get_used_heap_size();
    #endif

    /* Only up-buffer 0 is pre-initialized by the SEGGER library, so the raw data channel needs an explicit buffer. */
    if (SEGGER_RTT_ConfigUpBuffer(RTT_UP_CHANNEL_RAW_DATA_FRAMES, "RawDataFrames", rtt_raw_data_frame_buffer,
                                  sizeof(rtt_raw_data_frame_buffer), SEGGER_RTT_MODE_NO_BLOCK_SKIP) < 0)
    {
        CY_ASSERT(0);
    }

    /* Init radar task */
    if (xTaskCreate(&radar_task, "radar", RADAR_TASK_STACK_DEPTH_WORDS, NULL,
                    TASK_PRIORITY_RADAR, &radar_task_handle) != pdPASS)
    {
        CY_ASSERT(0);
    }

    #ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
    memory_report_data.radar_task_heap_usage = get_used_heap_size() - heap_before_task_creation;
    #endif
}


#ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
void get_radar_task_memory_report(memory_report_data_t* report)
{
    if (report == NULL)
    {
        return;
    }

    report->radar_task_stack_size = RADAR_TASK_STACK_DEPTH_WORDS;  // * sizeof(StackType_t);
    report->radar_task_stack_usage = uxTaskGetStackHighWaterMark2(radar_task_handle);
}


#endif // ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST

void handle_fifo_threshold_interrupt(void)
{
    /* Holds the number of dropped frames since last processed frame */
    static uint32_t frames_dropped = 0;
    bool frame_dropped = true;
    uint32_t frame_size = raw_frame_shape.number_of_chirps * raw_frame_shape.samples_per_chirp *
                          raw_frame_shape.rx_antennas * sizeof(uint16_t);
    radar_frame* radar_job = (radar_frame*)pvPortMalloc(sizeof(radar_frame));
    // The allocated memory will be freed in the `process.c` task.
    uint16_t* frame_payload = (uint16_t*)pvPortMalloc(frame_size);
    if (radar_job && frame_payload)
    {
        radar_job->frames_dropped_since_last_frame = frames_dropped;
        radar_job->frame = frame_payload;
        radar_job->sample_count = frame_size / sizeof(uint16_t);
        radar_job->captured_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (xensiv_bgt60cutrxx_get_fifo(&(sensor.dev), (fifo_entry_t*)frame_payload, current_fifo_threshold+1) != XENSIV_BGT60CUTRXX_STATUS_OK) {
            vPortFree(frame_payload); vPortFree(radar_job); ++frames_dropped; return;
        }
        xensiv_bgt60cutrxx_set_irq_clear(&(sensor.dev), BGT60CUTR13B_IR_CLEAR_THRESHOLD_MSK);

        if (!send_frame_via_rtt(frame_payload, current_frame_number, frame_size / sizeof(uint16_t)))
        {
            debug_print("Failed to send radar frame via RTT");
        }

        /* Push job to process task */
        process_job job = { .type = PROCESS_JOB_TYPE_RADAR_FRAME, .payload = radar_job, .size = sizeof(radar_frame) };
        /* Use small timeout (10ms) instead of 0 to allow queue operation to complete */
        if (xQueueSend(process_job_queue, &job, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            frame_dropped = false;
        }
        else
        {
            debug_print("Failed to send radar frame to process queue");
        }
    }
    else
    {
        debug_print("Failed to allocate memory for radar frame");
    }

    /* Increase drop count if radar frame cannot pass to job queue */
    if (frame_dropped)
    {
        if (frame_payload)
        {
            vPortFree(frame_payload);
        }
        if (radar_job)
        {
            vPortFree(radar_job);
        }

        frames_dropped++;
    }
    else
    {
        frames_dropped = 0;
    }
}


static void handle_radar_data_ready(void)
{
    uint32_t irq_status = 0;
    int32_t ret = XENSIV_BGT60CUTRXX_STATUS_OK;

    ret = xensiv_bgt60cutrxx_get_irq_status(&(sensor.dev), &irq_status);
    if (XENSIV_BGT60CUTRXX_STATUS_OK != ret)
    {
        debug_pin_toggle(DEBUG_PIN_1);
    }

    #ifdef RADAR_MOTION_DETECTION
    if (irq_status &
        (BGT60CUTR13B_IR_STAT_ULP_EVENT_MSK |
         BGT60CUTR13B_IR_STAT_RD_EVENT_MSK |
         BGT60CUTR13B_IR_STAT_SEQ1_DONE_MSK |
         BGT60CUTR13B_IR_STAT_SEQ2_DONE_MSK))
    {
        handle_motion_detector_interrupt();
        debug_pin_toggle(DEBUG_PIN_1);
        motion_detection_t* md_results = pvPortMalloc(sizeof(motion_detection_t));

        if (md_results)
        {
            int result = motion_detection_get_results(md_results);
            if (result != 0)
            {
                debug_print("motion_detection_get_results failed with error code %d", result);
                vPortFree(md_results);
                return;
            }

            /* Push job to process task */
            process_job job =
                { .type   = PROCESS_JOB_TYPE_MOTION_DETECTION, .payload = md_results,
                  .size   = sizeof(motion_detection_t) };
            /* Use small timeout (10ms) instead of 0 to allow queue operation to complete */
            if (xQueueSend(process_job_queue, &job, pdMS_TO_TICKS(10)) != pdTRUE)
            {
                debug_print("Failed to send motion detection results to process queue");
                vPortFree(md_results);
            }
        }
        else
        {
            debug_print("Failed to allocate memory for motion detection results");
        }
    }
    else if (irq_status & BGT60CUTR13B_IR_STAT_FIFO_TH_MSK)
    {
        handle_fifo_threshold_interrupt();
        debug_pin_toggle(DEBUG_PIN_1);
    }
    #else // ifdef RADAR_MOTION_DETECTION
    if (irq_status & BGT60CUTR13B_IR_STAT_FIFO_TH_MSK)
    {
        handle_fifo_threshold_interrupt();
        debug_pin_toggle(DEBUG_PIN_1);
    }
    #endif // ifdef RADAR_MOTION_DETECTION
}


static void handle_radar_msg_get_registers(register_list_t* regs)
{
    bool enable_state = radar_enable_state;
    radar_enable(false);
    for (uint32_t i = 0; i < regs->length; i++)
    {
        uint16_t address = regs->entries[i].address;
        uint32_t* value_ptr = &regs->entries[i].value;
        int32_t status =
            xensiv_bgt60cutrxx_get_reg(&(sensor.dev), address, value_ptr);
        if (XENSIV_BGT60CUTRXX_STATUS_OK != status)
        {
            CY_ASSERT(0);
        }
    }
    #ifdef RADAR_MOTION_DETECTION
    motion_detection_get_sequence_settings(&regs->meta.ulp, &regs->meta.rd);
    #endif
    regs->meta.raw.fifo_threshold = current_fifo_threshold;
    radar_enable(enable_state);
}


static void handle_radar_msg_set_registers(const register_list_t* regs)
{
    radar_enable(false);

    /* Update FIFO threshold if provided in metadata */
    if (regs->meta.raw.fifo_threshold > 0)
    {
        current_fifo_threshold = regs->meta.raw.fifo_threshold;
    }

    int32_t result = xensiv_bgt60cutrxx_platform_init(&sensor, &mcu_iface_config, (uint32_t(
                                                                                       *)[2])regs->entries,
                                                      regs->length, false, false);

    if (result != 0)
    {
        debug_print("BGT60CUTRXX init failed with error code %d", result);
    }
    #ifdef RADAR_MOTION_DETECTION
    motion_detection_set_sequence_settings(&regs->meta.ulp, &regs->meta.rd);
    motion_detection_clustering_enable(regs->meta.enable_clustering);
    motion_detection_clutter_detection_enable(regs->meta.enable_clutter_detection);

    if (regs->meta.ulp.enabled || regs->meta.rd.enabled)
    {
        motion_detector_enabled = true;
        result = motion_detection_init(&sensor.dev);

        if (result != 0)
        {
            debug_print("motion_detection_init failed with error code %d", result);
        }
    }
    else
    {
        motion_detector_enabled = false;
        raw_frame_shape.number_of_chirps = regs->meta.raw.number_of_chirps;
        raw_frame_shape.samples_per_chirp = regs->meta.raw.samples_per_chirp;
        raw_frame_shape.rx_antennas = regs->meta.raw.rx_antennas;
    }
    #endif // ifdef RADAR_MOTION_DETECTION

    radar_enable(true);
}
