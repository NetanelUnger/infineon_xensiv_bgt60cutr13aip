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
#ifndef SRC_COMMON_COMMON_H_
#define SRC_COMMON_COMMON_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "usecase.h"

#include "resource_map.h"


/* defines if the frame generation is enabled after power on reset */
#define START_FRAME_AFTER_POR true
#define MAX_VELOCITY_GRID_BINS 64

#define STRINGIFY_VALUE(X) STRINGIFY_TOKEN(X)
#define STRINGIFY_TOKEN(X) #X
#define PROJECT_NAME STRINGIFY_VALUE(IFX_PROJECT_NAME)
#define USECASE_NAME STRINGIFY_VALUE(IFX_USECASE_NAME)
#define FW_VERSION STRINGIFY_VALUE(IFX_BUILD_VERSION)
#define FW_BUILD_HASH STRINGIFY_VALUE(IFX_BUILD_HASH)
#define BOARD_NAME STRINGIFY_VALUE(IFX_BOARD_NAME)

/**
 * RTT up-channel used to stream raw ADC data frames.
 *
 * Up-channel 0 is reserved for terminal/printf output, which would otherwise be interleaved into the binary
 * frame stream. The host (ifxapp) learns the channel index from the firmware info response.
 */
#ifndef RTT_UP_CHANNEL_RAW_DATA_FRAMES
#define RTT_UP_CHANNEL_RAW_DATA_FRAMES 1
#endif

/**
 * Macro to enable memory report for production test
 *
 * This also requires the FreeRTOSConfig.h to be configured accordingly:
 * - configRECORD_STACK_HIGH_ADDRESS must be set to 1
 * - INCLUDE_uxTaskGetStackHighWaterMark2 must be set to 1
 * - INCLUDE_xTaskGetIdleTaskHandle must be set to 1
 */
//#define ENABLE_MEMORY_REPORT_PRODUCTION_TEST

/**
 * Macro to enable or disable debug print statements
 * If the macro DEBUG_PRINT_DISABLE is defined, then no code will be generated for the debug_print macro.
 * If the macro DEBUG_PRINT_ENABLE is defined, then printf and retarget io will be used  for printing debug messages.
 */
#define DEBUG_PRINT_DISABLE

/** @brief Task priorities
 * The higher the value the higher the priority*/
typedef enum
{
    TASK_PRIORITY_RADAR = configMAX_PRIORITIES - 1,
    TASK_PRIORITY_PROCESS = TASK_PRIORITY_RADAR - 1,
    TASK_PRIORITY_TERMINAL = TASK_PRIORITY_PROCESS - 1
} task_priorities;

/**
 * @brief Hardware Peripheral Priorities
 *
 * collection of the priorities for hardware interrupts
 */
typedef enum irq_priorities
{
    IRQ_PRIORITY_UART_RX = 4
} irq_priorities_t;

/* Process job types */
typedef enum
{
    PROCESS_JOB_TYPE_EMPTY = 0,
    PROCESS_JOB_TYPE_RADAR_FRAME = 1,
    PROCESS_JOB_TYPE_TERMINAL_COMMAND = 2,
    PROCESS_JOB_TYPE_TERMINAL_COMMAND_CUSTOM = 3,
    PROCESS_JOB_TYPE_MOTION_DETECTION = 4
} process_job_type;

/* A job that is posed to the process queue */
typedef struct
{
    process_job_type type;
    uint32_t param;
    void* payload;
    size_t size;
} process_job;

/**
 * @brief Message types for the radar message queue.
 *
 * Used for asynchronous radar operations and event notifications.
 */
typedef enum
{
    RADAR_MSG_NONE = 0,        /**< No message */
    RADAR_MSG_GET_REGISTERS,   /**< Request to get radar registers */
    RADAR_MSG_SET_REGISTERS,   /**< Request to set radar registers */
    RADAR_MSG_RADAR_DATA_READY /**< Radar data ready event */
} radar_msg_type_t;

/**
 * @brief Message structure for the radar message queue.
 *
 * Used to pass radar commands and events between threads via FreeRTOS queues.
 */
typedef struct
{
    radar_msg_type_t type;   /**< Message type */
    void* payload;           /**< Pointer to message-specific data */
    size_t payload_size;     /**< Size of payload in bytes */
    TaskHandle_t reply_task; /**< Task to notify on completion (optional) */
} radar_msg_t;

/* Terminal command types */
typedef enum
{
    TERMINAL_CMD_TYPE_EMPTY = 0,

    /**
     * @brief Get version information about the firmware. Legacy command replaced by TERMINAL_CMD_TYPE_GET_INFO.
     */
    TERMINAL_CMD_TYPE_GET_VERSION = 1,
    /**
     * @brief Get information about the system. Firmware version, sensor information, general system information.
     */
    TERMINAL_CMD_TYPE_GET_INFO = 1,
    /**
     * @brief Apply (use-case) algorithm configuration. Payload holds the configuration structure.
     */
    TERMINAL_CMD_TYPE_APPLY_CONFIGURATION = 2,
    /**
     * @brief Apply algorithm user-configuration. Payload holds the user-configuration structure.
     */
    TERMINAL_CMD_TYPE_APPLY_ALGO_USER_CONFIGURATION = 3,
    /**
     * @brief Switch input source. Payload is one byte that indicates the source.
     * Payload
     *   0x00: Radar
     *   0x01: Terminal
     * @see TERMINAL_CMD_TYPE_PROCESS_FRAME
     */
    TERMINAL_CMD_TYPE_SWITCH_SOURCE = 4,
    /**
     * @brief Process a radar frame. Payload holds the radar frame.
     */
    TERMINAL_CMD_TYPE_PROCESS_FRAME = 5,
    /**
     * @brief Query current algorithm configuration.
     */
    TERMINAL_CMD_TYPE_GET_CONFIGURATION = 6,
    /**
     * @brief Query current algorithm user-configuration.
     */
    TERMINAL_CMD_TYPE_GET_ALGO_USER_CONFIGURATION = 7,
    /**
     * @brief Get list of all radar registers.
     */
    TERMINAL_CMD_TYPE_GET_REGISTER_LIST = 8,
    /**
     * @brief Set list of radar registers.
     */
    TERMINAL_CMD_TYPE_SET_REGISTER_LIST = 9
} terminal_cmd_type;


/* A terminal command. */
typedef struct
{
    uint8_t id; /* see terminal_cmd_type */
    uint16_t length;
    char* payload;
    uint16_t checksum;
} terminal_command;

typedef struct
{
    uint16_t* frame;
    uint32_t frames_dropped_since_last_frame;
    uint32_t sample_count;
    uint32_t captured_ms;
} radar_frame;

typedef struct
{
    uint32_t address;
    uint32_t value;
} register_entry_t;

typedef struct
{
    bool enabled;
    float range_resolution_m;
    float rd_velocity_grid_mps[MAX_VELOCITY_GRID_BINS];
    uint32_t num_rd_velocity_bins;
} meta_sequence_settings_t;

typedef struct
{
    float rd_calibration_address_rx0;
    float rd_calibration_address_rx1;
    float rd_calibration_address_rx2;
} meta_calib_t;

typedef struct
{
    uint16_t number_of_chirps;
    uint16_t rx_antennas;
    uint16_t samples_per_chirp;
    uint32_t fifo_threshold;
} meta_raw_t;

typedef struct
{
    meta_sequence_settings_t ulp;
    meta_sequence_settings_t rd;
    meta_calib_t calib;
    bool enable_clustering;
    bool enable_clutter_detection;
    meta_raw_t raw;
} meta_t;

typedef struct
{
    meta_t meta;
    register_entry_t* entries;
    uint32_t length;
} register_list_t;

typedef struct
{
    const char* name;
    const char* model;
    const char* driver_version;
    uint32_t xtal_frequency_hz;
    bool is_configurable;
} sensor_info_t;

/* Task creation functions */
void radar_task_init(void);
void process_task_init(void);


/* Radar on/off control function */
void radar_enable(bool enable);

void process_radar(ifx_usecase_handle handle, bool reset, const uint16_t* radar_frame_data, uint32_t frames_dropped);

void terminal_task_init(void);

/* Write message to UART */
void terminal_write(const char* str);
void terminal_printf(const char* fmt, ...);
void debug_print(const char* fmt, ...);
void process_terminal_command(terminal_command* cmd, ifx_usecase_handle usecase_handle, bool* reset,
                              bool* input_source_attached_radar);

/** initialize the UART hardware peripheral*/
void uart_init(void);

/* Get system clock frequence in Hz. */
uint32_t ifx_get_system_clock_frequency(void);

/* Get CPU type such as "Cortex-M4" */
const char* ifx_get_system_cpu_type(void);

/* Process job queue */
extern QueueHandle_t process_job_queue;

/* Radar command queue */
extern QueueHandle_t radar_cmd_queue;

/* Helpers to convert float values to the nearest signed/unsigned integer */
int16_t float_to_int16(float value);
uint16_t float_to_uint16(float value);
uint8_t float_to_uint8(float value);

#ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
#ifndef configRECORD_STACK_HIGH_ADDRESS
#error "Please add to FreeRTOSConfig: #define configRECORD_STACK_HIGH_ADDRESS 1"
#elif  (configRECORD_STACK_HIGH_ADDRESS == 0)
#error "Please modify configRECORD_STACK_HIGH_ADDRESS to 1 in FreeRTOSConfig"
#endif

#ifndef INCLUDE_uxTaskGetStackHighWaterMark2
#error "Please add to FreeRTOSConfig: #define INCLUDE_uxTaskGetStackHighWaterMark2 1"
#elif  (INCLUDE_uxTaskGetStackHighWaterMark2 == 0)
#error "Please modify INCLUDE_uxTaskGetStackHighWaterMark2 to 1 in FreeRTOSConfig"
#endif

#ifndef INCLUDE_xTaskGetIdleTaskHandle
#error "Please add to FreeRTOSConfig: #define INCLUDE_xTaskGetIdleTaskHandle 1"
#elif  (INCLUDE_xTaskGetIdleTaskHandle == 0)
#error "Please modify INCLUDE_xTaskGetIdleTaskHandle to 1 in FreeRTOSConfig"
#endif

typedef struct
{
    uint32_t system_stack_size;
    uint32_t system_stack_used;
    uint32_t heap_size;
    uint32_t bss_size;
    uint32_t max_heap_usage;
    uint32_t idle_heap_usage;
    uint32_t runtime_additional_heap;
    uint32_t radar_task_heap_usage;
    uint32_t process_task_heap_usage;
    uint32_t process_queue_heap_usage;
    uint32_t terminal_task_heap_usage;
    uint32_t terminal_timers_heap_usage;
    uint32_t timer_daemon_heap_usage;
    uint32_t system_minimum_heap_usage;
    uint32_t processing_stack_size;
    uint32_t processing_stack_usage;
    uint32_t radar_task_stack_size;
    uint32_t radar_task_stack_usage;
    uint32_t terminal_task_stack_size;
    uint32_t terminal_task_stack_usage;
    uint32_t idle_task_stack_usage;
} memory_report_data_t;

extern memory_report_data_t memory_report_data;

uint32_t get_used_heap_size(void);
void get_radar_task_memory_report(memory_report_data_t* report);
void get_process_task_memory_report(memory_report_data_t* report);
#endif /* ENABLE_MEMORY_REPORT_PRODUCTION_TEST */

#endif /* SRC_COMMON_COMMON_H_ */
