#include "cyhal_system.h"
#include "cy_syslib.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include "common.h"

#ifdef ENABLE_MEMORY_REPORT_PRODUCTION_TEST
#include <malloc.h>

memory_report_data_t memory_report_data = { 0 };

uint32_t get_used_heap_size(void)
{
    struct mallinfo mi = mallinfo();
    return mi.uordblks;
}


#endif /* ENABLE_MEMORY_REPORT_PRODUCTION_TEST */

uint32_t ifx_get_system_clock_frequency(void)
{
    #if defined(COMPONENT_CM4)
    return SystemCoreClock;
    #else
    //#error "SystemCoreClock not defined"
    return 0;
    #endif
}


const char* ifx_get_system_cpu_type(void)
{
    #if defined(COMPONENT_CM4)
    return "Cortex-M4";
    #else
    //TODO: error case appears on qemu
    //#error "CPU type not defined."
    return "Undefined";
    #endif
}


int16_t float_to_int16(float value)
{
    int32_t int_value = (int32_t)roundf(value);
    if (int_value > INT16_MAX)
    {
        int_value = INT16_MAX;                        // clamp to max value
    }
    if (int_value < INT16_MIN)
    {
        int_value = INT16_MIN;                        // clamp to min value
    }
    return int_value;
}


uint16_t float_to_uint16(float value)
{
    int32_t uint_value = (int32_t)roundf(value);
    if (uint_value > UINT16_MAX)
    {
        uint_value = UINT16_MAX;                          // clamp to max value
    }
    if (uint_value < 0)
    {
        uint_value = 0;                 // clamp to min value
    }
    return uint_value;
}


uint8_t float_to_uint8(float value)
{
    int32_t uint_value = (int32_t)roundf(value);
    if (uint_value > UINT8_MAX)
    {
        uint_value = UINT8_MAX;                         // clamp to max value
    }
    if (uint_value < 0)
    {
        uint_value = 0;                 // clamp to min value
    }
    return uint_value;
}


void debug_print(const char* fmt, ...)
{
    #ifdef DEBUG_PRINT_ENABLE
    if (!fmt)
    {
        return;
    }

    char buffer[512];
    va_list args;

    // Format the incoming message
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    va_end(args);

    if (len < 0)
    {
        return;           // Formatting failed
    }
    buffer[len] = '\0';

    // Output as JSON
    terminal_printf("{\"debug\": \"%s\"}\n", buffer);
    #endif // ifdef DEBUG_PRINT_ENABLE
}
