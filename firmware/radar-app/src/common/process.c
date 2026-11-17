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

#include "research_capture.h"
#include <stdio.h>
#include <string.h>
#include "cyhal.h"
#include "cybsp.h"
#include "common.h"
#include <stdlib.h>

#include "motion_detection.h"

#define PROCESS_TASK_STACK_DEPTH_WORDS 32500  /* number of words according to configSTACK_DEPTH_TYPE */


static TaskHandle_t process_task_handle;
QueueHandle_t process_job_queue = NULL;

void process_radar(ifx_usecase_handle handle, bool reset, const uint16_t* radar_frame_data, uint32_t frames_dropped)
{
    TickType_t start_time = xTaskGetTickCount() / portTICK_PERIOD_MS;
    static TickType_t max_elapsed_time = 0;
    static uint32_t frame_count = 0;
    frame_count++;

    if (reset)
    {
        max_elapsed_time = 0;
        ifx_usecase_config cfg = { 0 };
        ifx_usecase_get_configuration(handle, &cfg);
    }

    /* run usecase with radar frame */

    ifx_event_flags events_flags = IFX_EVENT_FLAG_NONE;
   #ifndef ALGO_PROCESSING_DISABLE
    ifx_usecase_process(handle, reset, radar_frame_data, &events_flags);
   #else
    events_flags = IFX_EVENT_FLAG_PRESENCE;
   #endif
    debug_pin_toggle(DEBUG_PIN_1);
    TickType_t end_time = xTaskGetTickCount() / portTICK_PERIOD_MS;
    TickType_t elapsed_time = end_time - start_time;
    if (elapsed_time > max_elapsed_time)
    {
        max_elapsed_time = elapsed_time;
        terminal_printf("{\"debug\": \"Max processing time: %d ms\"}\n", max_elapsed_time);
    }

    if ((frame_count % 1) == 0)
    {
        static char ifx_event_buf[2048];
        int used_size = ifx_print_event(ifx_event_buf, sizeof(ifx_event_buf),
                                        handle, events_flags,
                                        (uint32_t)start_time);

        if (used_size < 0)
        {
            terminal_printf("{\"debug\": \"ifx_print_event exceeded output buffer size\"}\n");
        }
        else
        {
            static char outputbuffer[2176];
            char* closing = strrchr(ifx_event_buf, '}');
            if (closing != NULL)
            {
                int sn_result = snprintf(outputbuffer, sizeof(outputbuffer),
                                         "%.*s,\"elapsed_ms\":%u,\"frames_dropped\":%u}\n",
                                         (int)(closing - ifx_event_buf), ifx_event_buf,
                                         (unsigned int)elapsed_time, (unsigned int)frames_dropped);
                if ((sn_result > 0) && ((size_t)sn_result < sizeof(outputbuffer)))
                {
                    terminal_printf("%s", outputbuffer);
                }
            }
            else
            {
                (void)memcpy(outputbuffer, ifx_event_buf, sizeof(ifx_event_buf));
                terminal_printf("%s", outputbuffer);
            }
        }
    }
}


/* Deviation from MISRA C-2012 Rule 17.11: FreeRTOS task functions run in infinite loop and never return */
static _Noreturn void process_task(void* pvParameters)
{
    (void)pvParameters;

    /* Init process queue */
    process_job job = { 0 };

    /* Input source */
    bool input_source_attached_radar = true;
    bool reset = true;

    /* assign use-case algorithm */
    ifx_usecase_handle usecase_handle = (ifx_usecase_handle)pvParameters;

    for (;;)
    {
        /* Wait for any new job (either radar frame or terminal input) */
        if (xQueueReceive(process_job_queue, (void*)&job, portMAX_DELAY))
        {
            switch (job.type)
            {
                #ifdef RADAR_MOTION_DETECTION
                case PROCESS_JOB_TYPE_MOTION_DETECTION:
                {
                    if (input_source_attached_radar)
                    {
                        print_motion_detection_results((motion_detection_t*)job.payload);
                    }

                    break;
                }

                #endif // ifdef RADAR_MOTION_DETECTION
                case PROCESS_JOB_TYPE_RADAR_FRAME:
                {
                    radar_frame* cmd = (radar_frame*)job.payload;

                    if (input_source_attached_radar)
                    {
                        #ifndef RADAR_MOTION_DETECTION
                        research_frame(cmd->frame, cmd->sample_count, cmd->captured_ms, cmd->frames_dropped_since_last_frame);
                        process_radar(usecase_handle, reset, cmd->frame, cmd->frames_dropped_since_last_frame);
                        #endif
                        reset = false;
                    }

                    /* Free buffer */
                    vPortFree(cmd->frame);
                    //vPortFree(cmd);
                    break;
                }

                case PROCESS_JOB_TYPE_TERMINAL_COMMAND:
                {
                    terminal_command* cmd = (terminal_command*)job.payload;

                    process_terminal_command(cmd, usecase_handle, &reset, &input_source_attached_radar);

                    /* Free command buffer */
                    vPortFree(cmd->payload);
                    break;
                } /* end case PROCESS_JOB_TYPE_TERMINAL_COMMAND*/

                default:
                    /* No action required for unknown job types */
                    break;
            } /*end switch job.type*/
            /* Free up job payload buffer */
            vPortFree(job.payload);
        }/* end if process_queue*/
    } /* end for loop*/
}


void process_task_init(void)
{
    #ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
    const uint32_t heap_before_queue_creation = get_used_heap_size();
    #endif

    /* create a queue to exchange data between tasks*/
    const UBaseType_t QUEUE_LENGTH = 16;
    process_job_queue = xQueueCreate(QUEUE_LENGTH, sizeof(process_job));

    #ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
    const uint32_t heap_after_queue_cration = get_used_heap_size();
    memory_report_data.process_queue_heap_usage = heap_after_queue_cration - heap_before_queue_creation;
    #endif

    /* Init use-case algorithm */
    ifx_usecase_handle const usecase_handle = ifx_usecase_init(NULL);

    /* Optionally adjust user settings (e.g. sensor mounting, fov, angle correction) before init() from usecasesetting.h
       file */
//    ifx_usecase_settings settings;
//    int status = ifx_usecase_get_settings(usecase_handle, &settings);

//    settings.sensor_mounting.mounting_height_m = 1.5f;
//    settings.sensor_mounting.tilt_angle_deg = 0.0f;

//    settings.fov.angle_deg.min = -90.0f;
//    settings.fov.angle_deg.max = +90.0f;

//    // ifx_usecase_settings_angle_correction_t settings
//    settings.angle_correction.edge_handling = IFX_USECASE_SETTINGS_ANGLE_CORRECTION_EDGE_HANDLING_CLIP;
//    settings.angle_correction.outside_policy = IFX_USECASE_SETTINGS_ANGLE_CORRECTION_OUTSIDE_POLICY_CLAMP;

//    settings.angle_correction.enable = false;
//    settings.angle_correction.measured_deg.measured_at_m70 = -70;
//    settings.angle_correction.measured_deg.measured_at_m60 = -60;
//    settings.angle_correction.measured_deg.measured_at_m50 = -50;
//    settings.angle_correction.measured_deg.measured_at_m40 = -40;
//    settings.angle_correction.measured_deg.measured_at_m30 = -30;
//    settings.angle_correction.measured_deg.measured_at_m20 = -20;
//    settings.angle_correction.measured_deg.measured_at_m10 = -10;
//    settings.angle_correction.measured_deg.measured_at_0 = 0;
//    settings.angle_correction.measured_deg.measured_at_p10 = 10;
//    settings.angle_correction.measured_deg.measured_at_p20 = 20;
//    settings.angle_correction.measured_deg.measured_at_p30 = 30;
//    settings.angle_correction.measured_deg.measured_at_p40 = 40;
//    settings.angle_correction.measured_deg.measured_at_p50 = 50;
//    settings.angle_correction.measured_deg.measured_at_p60 = 60;
//    settings.angle_correction.measured_deg.measured_at_p70 = 70;

//    // ifx_usecase_settings_tracker_t settings
//    settings.tracker.macro_sensitivity = 1;
//    settings.tracker.micro_sensitivity = 1;
//    settings.tracker.safe_mode_as_default = true;
//    settings.tracker.room_size_detection = true;

//    status = ifx_usecase_set_settings(usecase_handle, &settings);
//    if (status != 0)
//    {
//       CY_ASSERT(0);
//    }

    /* Actual stack size can be taken from memory report.
     * The value defined here is the number of words. */
    const configSTACK_DEPTH_TYPE STACK_DEPTH_WORDS = PROCESS_TASK_STACK_DEPTH_WORDS;
    /* Init radar task */
    if (xTaskCreate(&process_task, "process", STACK_DEPTH_WORDS, usecase_handle,
                    TASK_PRIORITY_PROCESS, &process_task_handle) != pdPASS)
    {
        CY_ASSERT(0);
    }

    #ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
    memory_report_data.process_task_heap_usage = get_used_heap_size() - heap_after_queue_cration;
    #endif
}


#ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
void get_process_task_memory_report(memory_report_data_t* report)
{
    if (report == NULL)
    {
        return;
    }

    report->processing_stack_size = PROCESS_TASK_STACK_DEPTH_WORDS;  // * sizeof(StackType_t);
    report->processing_stack_usage = uxTaskGetStackHighWaterMark2(process_task_handle);
}


#endif // ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
