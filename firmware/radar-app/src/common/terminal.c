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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "radar_ble.h"
#include "research_capture.h"
#include <stdlib.h>
#include <errno.h>

#include "empty_register_list_macro.h"
#include "json.h"
#include "usecase_settings_serialize.h"
#include "xensiv_bgt60cutrxx_mtb.h"

#include "cyhal.h"
#include "cybsp.h"
#include "common.h"

#include "SEGGER_RTT.h"

#define CMD_SYNC_BYTE (0xD9)
#define CMD_HEADER_LENGTH (6)

#ifdef RADAR_MOTION_DETECTION
static void terminal_print_regs_as_json(register_list_t* regs);
static void parse_json_to_register_list(const char* json_str, register_list_t* regs);
static register_entry_t register_entries_buffer[] = EMPTY_REGISTER_LIST_INIT;
#endif

typedef struct
{
    uint8_t* buffer;
    uint32_t data_count;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
} circular_buffer_t;

static circular_buffer_t _uart_data_cbuff;
static uint8_t _uart_data_buf[40000];
static TaskHandle_t terminal_task_handle;
static SemaphoreHandle_t terminal_mutex = NULL;


static void circular_buffer_init(circular_buffer_t* cbuff, uint8_t* buffer_array, uint32_t size)
{
    cbuff->buffer = buffer_array;
    cbuff->data_count = 0;
    cbuff->capacity = size;
    cbuff->head = 0;
    cbuff->tail = 0;
    return;
}


static uint32_t circular_buffer_add(circular_buffer_t* cbuff, uint8_t* src_data, uint32_t src_data_len)
{
    /* check if circular buffer has enough space */
    if ((cbuff->data_count + src_data_len) > cbuff->capacity)
    {
        return (0);
    }
    /* copy data */
    for (int i = 0; i < src_data_len; i++)
    {
        cbuff->buffer[cbuff->tail++] = src_data[i];
        /* roll over */
        if (cbuff->tail >= cbuff->capacity)
        {
            cbuff->tail = 0;
        }
    }
    cbuff->data_count += src_data_len;
    return (src_data_len);
}


static uint32_t circular_buffer_get(circular_buffer_t* cbuff, uint8_t* dest_data, uint32_t dest_data_len)
{
    /* no data in the circular buffer */
    if (cbuff->data_count == 0)
    {
        return (0);
    }
    /* adjust write length */
    if (dest_data_len > cbuff->data_count)
    {
        dest_data_len = cbuff->data_count;
    }
    /* copy data */
    for (int i = 0; i < dest_data_len; i++)
    {
        dest_data[i] = cbuff->buffer[cbuff->head++];
        /* roll over */
        if (cbuff->head >= cbuff->capacity)
        {
            cbuff->head = 0;
        }
    }
    cbuff->data_count -= dest_data_len;
    return (dest_data_len);
}


/* IRQ handler for UART RX data ready */
static void uart_rx_irq(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t numToCopy = Cy_SCB_GetNumInRxFifo(UART_DBG_HW);
    uint8_t read_data;
    debug_pin_toggle(DEBUG_PIN_0);
    /* Copy UART data to circular buffer */
    while (numToCopy-- > 0)
    {
        read_data = Cy_SCB_ReadRxFifo(UART_DBG_HW);
        circular_buffer_add(&_uart_data_cbuff, (void*)&read_data, sizeof(uint8_t));
    }
    Cy_SCB_ClearRxInterrupt(UART_DBG_HW, CY_SCB_RX_INTR_NOT_EMPTY);
    /* Notify terminal task UART data received */
    vTaskNotifyGiveFromISR(terminal_task_handle, &xHigherPriorityTaskWoken);
}


void uart_init(void)
{
    static const cy_stc_sysint_t UART_DEBUG_IRQ_cfg =
    {
        .intrSrc      = (IRQn_Type)UART_DBG_IRQ,
        .intrPriority = IRQ_PRIORITY_UART_RX
    };
    static cy_stc_scb_uart_context_t UART_DEBUG_context;
    /* init circular buffer for UART RX buffer */
    circular_buffer_init(&_uart_data_cbuff, (void*)_uart_data_buf, sizeof(_uart_data_buf));
    /* init UART with RX interrupt trigger */
    Cy_SCB_UART_Init(UART_DBG_HW, &UART_DBG_config, &UART_DEBUG_context);
    Cy_SCB_UART_Enable(UART_DBG_HW);
    Cy_SysInt_Init(&UART_DEBUG_IRQ_cfg, &uart_rx_irq);
    NVIC_EnableIRQ((IRQn_Type)UART_DEBUG_IRQ_cfg.intrSrc);
}


static void uart_buff_reinit(void)
{
    circular_buffer_init(&_uart_data_cbuff, (void*)_uart_data_buf, sizeof(_uart_data_buf));
}


static uint32_t uart_port_rxdatalen_get(void)
{
    return (_uart_data_cbuff.data_count);
}


static uint32_t uart_port_read_buff(uint8_t* data, uint32_t len)
{
    return (circular_buffer_get(&_uart_data_cbuff, data, len));
}


static uint16_t crc16_generate(uint8_t* src_data, uint32_t src_data_len, uint16_t* last_crc)
{
    /* CRC-16/CCITT-FALSE */
    uint16_t crc = (last_crc == NULL) ? (0xFFFF) : (*last_crc);
    for (int i = 0; i < src_data_len; i++)
    {
        crc = ((uint8_t)(crc >> 8) | (crc << 8)) ^ src_data[i];
        crc ^= (uint8_t)(crc & 0xFF) >> 4;
        crc ^= (crc << 12);
        crc ^= ((crc & 0xFF) << 5);
    }
    return crc;
}


static uint32_t port_data_get(void* data, uint32_t num_bytes)
{
    /* check UART data available */
    portENTER_CRITICAL();
    if (uart_port_rxdatalen_get() > 0)
    {
        num_bytes = uart_port_read_buff(data, num_bytes);
    }
    else
    {
        num_bytes = 0;
    }
    portEXIT_CRITICAL();
    return num_bytes;
}


bool terminal_submit_ble(uint8_t id, const uint8_t *payload, uint16_t length)
{
    if (!process_job_queue || !((id == 1 || id == 7 || id == 21 || id == 23) ? length == 0 :
        ((id == 3 || id == 20 || id == 22) && length > 1 && length <= 2048 && payload[0] == '{'))) return false;
    terminal_command *cmd = pvPortMalloc(sizeof(*cmd));
    if (!cmd) return false;
    memset(cmd, 0, sizeof(*cmd)); cmd->id = id; cmd->length = length;
    if (length) {
        cmd->payload = pvPortMalloc(length + 1);
        if (!cmd->payload) { vPortFree(cmd); return false; }
        memcpy(cmd->payload, payload, length); cmd->payload[length] = 0;
    }
    process_job job = {.type=PROCESS_JOB_TYPE_TERMINAL_COMMAND, .payload=(uint8_t*)cmd, .size=sizeof(*cmd)};
    if (xQueueSend(process_job_queue, &job, 0) == pdTRUE) return true;
    vPortFree(cmd->payload); vPortFree(cmd); return false;
}

static void bin_cmd_handle(terminal_command* cmd_message)
{
    /* Create copy of command */
    terminal_command* cmd = pvPortMalloc(sizeof(terminal_command));
    if (!cmd)
    {
        return;  /* Memory allocation failed, cannot process command */
    }
    cmd->id = cmd_message->id;
    cmd->length = cmd_message->length;
    cmd->payload = NULL;
    cmd->checksum = cmd_message->checksum;
    if (cmd_message->length)
    {
        cmd->payload = pvPortMalloc(cmd_message->length + 1);  // +1 for null terminator
        if (!cmd->payload)
        {
            CY_ASSERT(0);
            vPortFree(cmd);  /* Free cmd before returning */
            return;  /* Memory allocation failed */
        }
        memcpy(cmd->payload, cmd_message->payload, cmd_message->length);
        cmd->payload[cmd_message->length] = '\0';  // Null terminate for JSON parsing
    }
    process_job job =
    {
        .type    = PROCESS_JOB_TYPE_TERMINAL_COMMAND,
        .payload = (uint8_t*)cmd,
        .size    = sizeof(*cmd_message)
    };

    /* Send command to job queue */
    if (xQueueSend(process_job_queue, &job, 0) != pdTRUE)
    {
        terminal_printf("{"
                        "msg"
                        ": "
                        "Terminal command %d dropped"
                        "}",
                        cmd->id);
        /* Free memory as it was not queued. */
        vPortFree(cmd->payload);
        vPortFree(cmd);
    }
}


static uint32_t bin_cmd_consume_buffer(uint8_t* src_data,
                                       uint32_t src_data_len)
{
    terminal_command cmd_message;
    uint32_t read_ptr = 0;
    /* check sync byte */
    if (src_data[read_ptr++] == CMD_SYNC_BYTE)
    {
        cmd_message.id = src_data[read_ptr++];
        cmd_message.length = src_data[read_ptr++];
        cmd_message.length += ((uint32_t)src_data[read_ptr++]) << 8;
        cmd_message.payload = (char*)&src_data[read_ptr];
        read_ptr += cmd_message.length;
        /* not enough payload, wait for more data */
        if (src_data_len < (read_ptr + sizeof(uint16_t)))
        {
            return 0;
        }
        /* check checksum */
        cmd_message.checksum = src_data[read_ptr++];
        cmd_message.checksum += ((uint32_t)src_data[read_ptr++]) << 8;
        /* checksum incorrect, consume 1 byte */
        if (cmd_message.checksum != crc16_generate(src_data, read_ptr - sizeof(uint16_t), NULL))
        {
            return 1;
        }
        /* process command */
        bin_cmd_handle(&cmd_message);
        return read_ptr;
    }
    /* not starting with sync byte, consume 1 byte */
    else
    {
        return 1;
    }
}


static void bin_cmd_process(void)
{
    static uint8_t rx_buff[40000];
    static uint32_t read_start_ptr = 0;
    static uint32_t read_end_ptr = 0;
    static uint32_t last_recv_time = 0;
    uint32_t rx_buff_remain = sizeof(rx_buff) - read_end_ptr;
    uint32_t read_length, consume_length;
    /* get data from port */
    read_length = port_data_get(&rx_buff[read_end_ptr], rx_buff_remain);
    if (read_length > 0)
    {
        read_end_ptr += read_length;
        last_recv_time = xTaskGetTickCount();
        rx_buff_remain = sizeof(rx_buff) - read_end_ptr;
        read_length = read_end_ptr - read_start_ptr;
        while (read_length >= CMD_HEADER_LENGTH)
        {
            consume_length = bin_cmd_consume_buffer(&rx_buff[read_start_ptr], read_length);
            /* cannot consume bytes when buffer is full, force to consume 1 byte */
            if ((read_end_ptr == sizeof(rx_buff)) && (consume_length == 0))
            {
                consume_length = 1;
            }
            else if (consume_length == 0)
            {
                break;
            }
            read_start_ptr += consume_length;
            read_length = read_end_ptr - read_start_ptr;
        }
        /* reallocate buffer memory */
        if ((read_start_ptr > 0) && (read_end_ptr > read_start_ptr))
        {
            memmove(rx_buff, &rx_buff[read_start_ptr], read_end_ptr - read_start_ptr);
            read_end_ptr -= read_start_ptr;
            read_start_ptr = 0;
        }
        else if ((read_end_ptr > 0) && (read_end_ptr == read_start_ptr))
        {
            read_end_ptr = 0;
            read_start_ptr = 0;
        }
    }
    else
    {
        /* timeout if waiting extra data for more than 2 sec */
        TickType_t current_tick = xTaskGetTickCount();
        if ((read_end_ptr > 0) && (current_tick - last_recv_time > 2000))
        {
            read_end_ptr = 0;
            read_start_ptr = 0;
            uart_buff_reinit();
        }
    }
}


/* Deviation from MISRA C-2012 Rule 17.11: FreeRTOS task functions run in infinite loop and never return */
static _Noreturn void terminal_task(void* pvParameters)
{
    uint32_t check_timeout = 0;


    for (;;)
    {
        /* Wait for UART RX data received */
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        /* Loop for 3 sec if UART data received, to prevent any missing data */
        while (check_timeout < 300)
        {
            /* Process terminal input */
            bin_cmd_process();
            vTaskDelay(10);
            check_timeout++;
        }
        check_timeout = 0;
    }
}


void terminal_write(const char* str)
{
    /* Send string to UART, skipping newline characters. The GUI protocol is
     * newline-delimited JSON, so embedded newlines (e.g. in pretty-printed
     * schema strings from the algo library) must not reach the wire. */
    CY_ASSERT(terminal_mutex);
    if (xSemaphoreTake(terminal_mutex, portMAX_DELAY))
    {
        const char* chunk_start = str;
        const char* p = str;
        while (*p != '\0')
        {
            if (*p == '\n')
            {
                uint32_t chunk_len = (uint32_t)(p - chunk_start);
                if (chunk_len > 0U)
                {
                    /* coverity[misra_c_2012_rule_11_8_violation] : Third-party API void* lacks const */
                    Cy_SCB_UART_PutArrayBlocking(UART_DBG_HW, (void*)chunk_start, chunk_len);
                    radar_ble_terminal_bytes(chunk_start, chunk_len);
                }
                chunk_start = p + 1;
            }
            p++;
        }
        uint32_t chunk_len = (uint32_t)(p - chunk_start);
        if (chunk_len > 0U)
        {
            /* coverity[misra_c_2012_rule_11_8_violation] : Third-party API void* lacks const */
            Cy_SCB_UART_PutArrayBlocking(UART_DBG_HW, (void*)chunk_start, chunk_len);
            radar_ble_terminal_bytes(chunk_start, chunk_len);
        }
        xSemaphoreGive(terminal_mutex);
    }
}


void terminal_printf(const char* fmt, ...)
{
    CY_ASSERT(terminal_mutex);

    va_list args;

    /* Open variable parameter list */
    va_start(args, fmt);

    /* A shared buffer under the terminal lock avoids consuming the Bluetooth
     * callback task's small stack. All formatting/output is serialized. */
    if (!xSemaphoreTake(terminal_mutex, portMAX_DELAY)) { va_end(args); return; }
    static char buffer[4096];
    int length = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (length < 0)
    {
        xSemaphoreGive(terminal_mutex);
        return;
    }

    /* End parameter list */
    if ((size_t)length >= sizeof(buffer)) length = sizeof(buffer) - 1;

    /* Take the mutex */
    if (length >= 0)
    {
        debug_pin_toggle(DEBUG_PIN_0);
        /* Send the string via UART */
        Cy_SCB_UART_PutArrayBlocking(UART_DBG_HW, buffer, length);
        radar_ble_terminal_bytes(buffer, length);

        /* Release mutex */
        xSemaphoreGive(terminal_mutex);
    }
}


void terminal_task_init(void)
{
    /* Init terminal mutex */
    terminal_mutex = xSemaphoreCreateBinary();
    CY_ASSERT(terminal_mutex);

    /* Release mutex */
    xSemaphoreGive(terminal_mutex);

    /* Create command handle task */
    if (xTaskCreate(&terminal_task, "terminal", 256, NULL,
                    TASK_PRIORITY_TERMINAL, &terminal_task_handle) != pdPASS)
    {
        CY_ASSERT(0);
    }
}


void process_terminal_command(terminal_command* cmd, ifx_usecase_handle usecase_handle, bool* reset,
                              bool* input_source_attached_radar)
{
    /* Allocate buffer for sending data via json format to the terminal */
    #ifndef RADAR_MOTION_DETECTION
    static char json_buffer[2048] = { 0 };
    #endif

    if (cmd->id >= 20 && cmd->id <= 23) {
        bool ok = research_command(cmd->id, cmd->payload, cmd->length);
        terminal_printf("{\"cmd\":{\"id\":%u,\"state\":%u}}\n", cmd->id, ok);
        return;
    }
    bool processed = false;
    const terminal_cmd_type cmd_id = (terminal_cmd_type)cmd->id;

    switch (cmd_id)
    {
        case TERMINAL_CMD_TYPE_GET_INFO:
        {
            sensor_info_t sensor_info = { .driver_version    = NULL,
                                          .name              = NULL,
                                          .is_configurable   = false,
                                          .xtal_frequency_hz = 0 };
            sensor_info.driver_version = "1.0.8";
            sensor_info.name = "BGT60CUTR13";
            sensor_info.is_configurable = true;
            sensor_info.xtal_frequency_hz = 80000000;

            uint32_t cpu_freq = ifx_get_system_clock_frequency();
            const char* cpu_type = ifx_get_system_cpu_type();
            terminal_printf(
                "{\"version\": \"%s\", \"name\": \"%s\", \"board\": \"%s\", \"build\": \"%s\", \"cpu_freq\": %lu, \"cpu\": \"%s\"",
                FW_VERSION,
                FW_NAME,
                BOARD_NAME,
                FW_BUILD_HASH,
                cpu_freq,
                cpu_type);

            #ifndef RADAR_MOTION_DETECTION
            {
                const char* sem_ver = NULL;
                const char* name = NULL;
                const char* settings = NULL;
                const char* build = NULL;
                ifx_usecase_get_version(&sem_ver, &name, &settings, &build);
                (void)settings;  /* output param not needed; schema loaded via ifx_usecase_settings_schema_json */
                (void)build;     /* output param not needed; build info comes from FW_BUILD_HASH */
                const char* schema = ifx_usecase_settings_schema_json();
                sensor_info.is_configurable = false;
                /* Send algo_info prefix and schema separately to avoid overflowing terminal_printf's
                * internal vsprintf buffer (2048 bytes): the schema JSON alone exceeds that limit. */
                terminal_printf(
                    ",\"algo_info\":{\"version\": \"%s\", \"name\": \"%s\", \"is_configurable\": %s, \"user_configuration_schema\": ",
                    sem_ver,
                    name,
                    "true");
                terminal_write(schema ? schema : "null");
                terminal_write("}");
            }
            terminal_printf(
                ",\"event_info\":{\"detection_type_mapping\":{\"1\":\"CW Macro\", \"2\":\"CW Micro\", \"3\":\"CW Static\"}}");
            #else // ifndef RADAR_MOTION_DETECTION
            terminal_printf(
                ",\"event_info\":{\"types\":{\"azimuth_clutter\":\"zones\",\"elevation_clutter\":\"zones\"}, \"detection_type_mapping\":{\"1\":\"ULP\", \"2\":\"RD\", \"3\":\"ULP Clustered\"}}");
            #endif // ifndef RADAR_MOTION_DETECTION

            // Guarantee a hex-prefixed pointer output like: ,"rtt":{"addr":"0x20001234","channel":1}
            terminal_printf(",\"rtt\":{\"addr\":\"0x%" PRIxPTR "\",\"channel\":%u}", (uintptr_t)&_SEGGER_RTT,
                            (unsigned int)RTT_UP_CHANNEL_RAW_DATA_FRAMES);
            terminal_printf(
                ",\"sensor_info\":{\"driver_version\":\"%s\",\"name\": \"%s\", \"is_configurable\": %s, \"xtal_mhz\": %.2f}}\n",
                sensor_info.driver_version,
                sensor_info.name,
                sensor_info.is_configurable ? "true" : "false",
                sensor_info.xtal_frequency_hz / 1000000.0);
            processed = true;
            break;
        }

        #ifndef RADAR_MOTION_DETECTION
        case TERMINAL_CMD_TYPE_APPLY_CONFIGURATION:
        {
            /* new configuration to apply */
            ifx_usecase_config configuration = { 0 };
            if (ifx_usecase_config_deserialize(cmd->payload, &configuration))
            {
                /* restart use-case */
                ifx_usecase_deinit(&usecase_handle);
                usecase_handle = ifx_usecase_init(&configuration);

                terminal_printf("{\"info\": \"Reset use-case with configuration.\"}\n");

                *reset = true;
            }
            else
            {
                terminal_printf("{\"warning\": \"Could not parse configuration.\"}\n");
            }
            processed = true;
            break;
        }

        case TERMINAL_CMD_TYPE_APPLY_ALGO_USER_CONFIGURATION:
        {
            ifx_usecase_config configuration = { 0 };
            ifx_usecase_get_configuration(usecase_handle, &configuration);
            if (ifx_usecase_settings_deserialize(cmd->payload, &configuration))
            {
                ifx_usecase_set_configuration(usecase_handle, &configuration);

                terminal_printf("{\"info\": \"Applied user_configuration.\"}\n");
            }
            else
            {
                terminal_printf("{\"warning\": \"Could not parse user_configuration.\"}\n");
            }
            processed = true;
            break;
        }
        #endif // ifndef RADAR_MOTION_DETECTION

        case TERMINAL_CMD_TYPE_SWITCH_SOURCE:
        {
            if (cmd->length)
            {
                uint8_t input_source = cmd->payload[0];
                const char* source_changed_msg = input_source == 0x00 ? "radar" : "terminal";

                if (input_source == 0)
                {
                    radar_enable(true);
                }
                else
                {
                    radar_enable(false);
                }
                terminal_printf("{\"debug\": \"Input source changes to %s.\"}\n", source_changed_msg);

                *input_source_attached_radar = input_source == 0x00 ? true : false;
                *reset = true; /* make sure that we reset the use-case with the next frame */
                processed = true;
            }
            else
            {
                terminal_printf("{\"warning\": \"Malformated message id = %d\"}\n", cmd->id);
            }
            break;
        }

        case TERMINAL_CMD_TYPE_PROCESS_FRAME:
        {
            if (!*input_source_attached_radar)
            {
                /* through terminal, no frames are lost */
                const uint32_t frames_lost_through_terminal = 0;

                process_radar(usecase_handle, *reset, (uint16_t*)cmd->payload, frames_lost_through_terminal);
                processed = true;
                *reset = false;
            }
            else
            {
                terminal_printf("{\"warning\": \"Input source is not set to terminal.\"}\n");
            }
            break;
        }

        #ifndef RADAR_MOTION_DETECTION
        case TERMINAL_CMD_TYPE_GET_CONFIGURATION:
        {
            ifx_usecase_config configuration = { 0 };
            ifx_usecase_get_configuration(usecase_handle, &configuration);
            ifx_usecase_config_serialize(&configuration, json_buffer, sizeof(json_buffer));
            processed = true;
            /* Canonical name: algo_configuration (legacy JSON key kept for client compatibility). */
            terminal_printf("{\"configuration\": %s}\n", json_buffer);
            break;
        }

        case TERMINAL_CMD_TYPE_GET_ALGO_USER_CONFIGURATION:
        {
            ifx_usecase_config configuration = { 0 };
            ifx_usecase_get_configuration(usecase_handle, &configuration);
            ifx_usecase_settings_serialize(&configuration, json_buffer, sizeof(json_buffer));
            processed = true;
            /* Canonical name: algo_user_configuration (legacy JSON key kept for client compatibility). */
            terminal_printf("{\"settings\": %s}\n", json_buffer);
            break;
        }

        #else // ifndef RADAR_MOTION_DETECTION
        case TERMINAL_CMD_TYPE_GET_REGISTER_LIST:
        {
            register_list_t regs =
                { .entries = register_entries_buffer, .length = ARRAY_SIZE(register_entries_buffer) };
            TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
            radar_msg_t msg =
            {
                .type         = RADAR_MSG_GET_REGISTERS,
                .payload      = &regs,
                .payload_size = sizeof(regs),
                .reply_task   = current_task
            };
            xQueueSend(radar_cmd_queue, &msg, portMAX_DELAY);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            terminal_print_regs_as_json(&regs);
            processed = true;
            break;
        }

        case TERMINAL_CMD_TYPE_SET_REGISTER_LIST:
        {
            terminal_printf("{\"info\": \"Set register list.\"}\n");
            register_list_t regs;
            parse_json_to_register_list(cmd->payload, &regs);
            TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
            radar_msg_t msg =
            {
                .type         = RADAR_MSG_SET_REGISTERS,
                .payload      = &regs,
                .payload_size = sizeof(regs),
                .reply_task   = current_task
            };
            xQueueSend(radar_cmd_queue, &msg, portMAX_DELAY);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            processed = true;

            break;
        }
        #endif // ifndef RADAR_MOTION_DETECTION

        default:
            /* No action required for unknown command types */
            break;
    } /* end case cmd_id */

    /* Acknowledge command execution. */
    terminal_printf("{\"cmd\": { \"id\": %d, \"state\": %d }}\n", cmd->id, processed);
}


#ifdef RADAR_MOTION_DETECTION
static void terminal_print_regs_as_json(register_list_t* regs)
{
    char json_buf[256];
    terminal_printf("{\"register_list\": [");
    for (size_t i = 0; i < regs->length; i++)
    {
        int result = snprintf(json_buf, sizeof(json_buf),
                              "{\"address\":\"0x%04" PRIx16 "\",\"value\":\"0x%08" PRIx32 "\"}%c",
                              (uint16_t)regs->entries[i].address, regs->entries[i].value,
                              i < (regs->length - 1) ? ',' : ']');

        if (result >= 0)
        {
            terminal_printf(json_buf);
        }
    }

    terminal_printf(", \"meta\": {");
    terminal_printf("\"ulp\": {");
    terminal_printf("\"enabled\": %s, ", regs->meta.ulp.enabled ? "true" : "false");
    terminal_printf("\"range_resolution_m\": %.3f, ", regs->meta.ulp.range_resolution_m);
    terminal_printf("\"velocity_grid_mps\": [");
    for (size_t i = 0; i < regs->meta.ulp.num_rd_velocity_bins; ++i)
    {
        terminal_printf("%.3f%c", regs->meta.ulp.rd_velocity_grid_mps[i],
                        i < (regs->meta.ulp.num_rd_velocity_bins - 1) ? ',' : ' ');
    }
    terminal_printf("]}, "); // end of ulp
    terminal_printf("\"rd\": {");
    terminal_printf("\"enabled\": %s, ", regs->meta.rd.enabled ? "true" : "false");
    terminal_printf("\"range_resolution_m\": %.3f, ", regs->meta.rd.range_resolution_m);
    terminal_printf("\"velocity_grid_mps\": [");
    for (size_t i = 0; i < regs->meta.rd.num_rd_velocity_bins; ++i)
    {
        terminal_printf("%.3f%c", regs->meta.rd.rd_velocity_grid_mps[i],
                        i < (regs->meta.rd.num_rd_velocity_bins - 1) ? ',' : ' ');
    }
    terminal_printf("]}, "); // end of rd
    terminal_printf("\"enable_clustering\": %s,", regs->meta.enable_clustering ? "true" : "false");
    terminal_printf("\"enable_clutter_detection\": %s,", regs->meta.enable_clutter_detection ? "true" : "false");
    terminal_printf("\"raw\": {");
    terminal_printf("\"number_of_chirps\": %u,", regs->meta.raw.number_of_chirps);
    terminal_printf("\"samples_per_chirp\": %u,", regs->meta.raw.samples_per_chirp);
    terminal_printf("\"rx_antennas\": %u,", regs->meta.raw.rx_antennas);
    terminal_printf("\"fifo_threshold\": %" PRIu32, regs->meta.raw.fifo_threshold);
    terminal_printf("}");  // end of raw
    terminal_printf("}}\n");
}


static void parse_sequence_settings(struct json seq_obj, meta_sequence_settings_t* settings)
{
    if (json_type(seq_obj) != JSON_OBJECT)
    {
        return;
    }

    // Parse enabled flag
    struct json enabled = json_object_get(seq_obj, "enabled");
    settings->enabled = json_bool(enabled);
    if (!settings->enabled)
    {
        settings->enabled = false;
        settings->num_rd_velocity_bins = 0;
        settings->range_resolution_m = 0.0f;
        return;
    }

    // Parse range resolution
    struct json range_res = json_object_get(seq_obj, "range_resolution_m");
    settings->range_resolution_m = (float)json_double(range_res);


    // Parse velocity grid
    struct json vel_grid = json_object_get(seq_obj, "velocity_grid_mps");
    size_t vel_grid_arr_size = ARRAY_SIZE(settings->rd_velocity_grid_mps);
    if (json_type(vel_grid) == JSON_ARRAY)
    {
        size_t vel_count = json_array_count(vel_grid);
        if (vel_grid_arr_size < vel_count)
        {
            terminal_printf("{\"error\": \"Velocity grid size exceeds maximum size.\"}\n");
        }

        settings->num_rd_velocity_bins = vel_count < vel_grid_arr_size ? vel_count : vel_grid_arr_size;
        for (size_t i = 0; i < settings->num_rd_velocity_bins; ++i)
        {
            struct json vel = json_array_get(vel_grid, i);
            settings->rd_velocity_grid_mps[i] = (float)json_double(vel);
        }
    }
}


static void parse_json_to_register_list(const char* json_str, register_list_t* regs)
{
    struct json root = json_parse(json_str);
    struct json reg_list = json_object_get(root, "register_list");
    if (json_type(reg_list) != JSON_ARRAY)
    {
        return;
    }

    size_t count = json_array_count(reg_list);

    if (count > ARRAY_SIZE(register_entries_buffer))
    {
        terminal_printf("{\"error\": \"Register list too long.\"}\n");
        return;
    }


    for (size_t i = 0; i < count; ++i)
    {
        struct json reg = json_array_get(reg_list, i);
        struct json addr = json_object_get(reg, "address");
        struct json val = json_object_get(reg, "value");
        char addr_buf[16], val_buf[16];
        json_string_copy(addr, addr_buf, sizeof(addr_buf));
        json_string_copy(val, val_buf, sizeof(val_buf));

        errno = 0;  /* MISRA C-2012 Rule 22.8: Reset errno before calling strtoul */
        register_entries_buffer[i].address = (uint16_t)strtoul(addr_buf, NULL, 0);
        /* MISRA C-2012 Rule 22.9: Test errno after calling strtoul */
        if (errno != 0)
        {
            return;
        }

        errno = 0;  /* MISRA C-2012 Rule 22.8: Reset errno before calling strtoul */
        register_entries_buffer[i].value = (uint32_t)strtoul(val_buf, NULL, 0);
        /* MISRA C-2012 Rule 22.9: Test errno after calling strtoul */
        if (errno != 0)
        {
            return;
        }
    }
    regs->length = count;
    regs->entries = register_entries_buffer;

    // Parse meta data
    regs->meta = (meta_t) { 0 };

    struct json meta = json_object_get(root, "meta");
    if (json_type(meta) == JSON_OBJECT)
    {
        // Parse ULP sequence settings
        struct json ulp = json_object_get(meta, "ulp");
        parse_sequence_settings(ulp, &regs->meta.ulp);

        // Parse RD sequence settings
        struct json rd = json_object_get(meta, "rd");
        parse_sequence_settings(rd, &regs->meta.rd);

        // Parse calibration data
        if (!regs->meta.rd.enabled)
        {
            // If RD sequence is disabled, clear calibration data
            regs->meta.calib = (meta_calib_t) { 0 };
        }

        struct json calib = json_object_get(meta, "calib");
        if (json_type(calib) == JSON_OBJECT)
        {
            struct json cal_rx0 = json_object_get(calib, "calibration_address_rx0");
            regs->meta.calib.rd_calibration_address_rx0 = (float)json_double(cal_rx0);
            struct json cal_rx1 = json_object_get(calib, "calibration_address_rx1");
            regs->meta.calib.rd_calibration_address_rx1 = (float)json_double(cal_rx1);
            struct json cal_rx2 = json_object_get(calib, "calibration_address_rx2");
            regs->meta.calib.rd_calibration_address_rx2 = (float)json_double(cal_rx2);
        }

        struct json clustering = json_object_get(meta, "enable_clustering");
        regs->meta.enable_clustering = json_type(clustering) == JSON_TRUE;

        struct json clutter_detection = json_object_get(meta, "enable_clutter_detection");
        regs->meta.enable_clutter_detection = json_type(clutter_detection) == JSON_TRUE;

        struct json raw = json_object_get(meta, "raw");
        if (json_type(raw) == JSON_OBJECT)
        {
            regs->meta.raw.number_of_chirps = (uint16_t)json_double(json_object_get(raw, "number_of_chirps"));
            regs->meta.raw.samples_per_chirp = (uint16_t)json_double(json_object_get(raw, "samples_per_chirp"));
            regs->meta.raw.rx_antennas = (uint16_t)json_double(json_object_get(raw, "rx_antennas"));
            regs->meta.raw.fifo_threshold = (uint32_t)json_double(json_object_get(raw, "fifo_threshold"));
        }
    }
}


#endif // ifdef RADAR_MOTION_DETECTION
