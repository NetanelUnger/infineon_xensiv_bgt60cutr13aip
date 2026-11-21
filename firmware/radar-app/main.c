/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the Radar Presence Tracking Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2021-2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cyhal.h"
#include "cybsp.h"
#include "common.h"

#include "xensiv_bgt60cutrxx_mtb.h"
#include "resource_map.h"
#include "SEGGER_RTT.h"
#include "radar_ble.h"
#include "radar_wifi.h"


/******************************************************************************
 * Macros
 *****************************************************************************/


/*******************************************************************************
* Global Variables
*******************************************************************************/

/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
volatile UBaseType_t uxTopUsedPriority;

/** Platform interface configuration */
xensiv_bgt60cutrxx_mtb_iface_config_t mcu_iface_config =
{
    .selpin       = PIN_XENSIV_BGT60CUTRXX_SPI_CSN,
    .rstpin       = PIN_XENSIV_BGT60CUTRXX_RSTN,
    .irqpin       = PIN_XENSIV_BGT60CUTRXX_IRQ,
    .irq_priority = CYHAL_ISR_PRIORITY_DEFAULT,
    .spi_mosi     = CYBSP_SPI_MOSI,
    .spi_miso     = CYBSP_SPI_MISO,
    .spi_clk      = CYBSP_SPI_CLK,
    .spi_speed_hz = XENSIV_BGT60CUTRXX_SPI_FREQUENCY,
};

/*******************************************************************************
* Function Prototypes
*******************************************************************************/

static int32_t xensiv_bgt60cutrxx_board_init(void);


/**Hook for the idle tasks
 * This function called by the idle task.
 * The function might be used to measure the CPU load or to set the system into power saving modes.
 * The hook must be enabled in the by setting configUSE_IDLE_HOOK to '1'*/
void vApplicationIdleHook(void);

/** Helper function to initialize board LEDs */
static int32_t initialize_board_leds(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    result |= cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT,
                              CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    result |= cyhal_gpio_init(SHIELD_XENSIV_RADAR_LED_RED, CYHAL_GPIO_DIR_OUTPUT,
                              CYHAL_GPIO_DRIVE_STRONG, SHIELD_XENSIV_RADAR_LED_RED_OFF);
    result |= cyhal_gpio_init(SHIELD_XENSIV_RADAR_LED_GREEN, CYHAL_GPIO_DIR_OUTPUT,
                              CYHAL_GPIO_DRIVE_STRONG, SHIELD_XENSIV_RADAR_LED_GREEN_OFF);

    if (result != CY_RSLT_SUCCESS)
    {
        /* LED GPIO initialization failed. Stop program execution */
        CY_ASSERT(0);
    }

    return XENSIV_BGT60CUTRXX_STATUS_OK;
}


/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: main
*********************************************************************************
* Summary:
* This is the main function for CPU. It...
*    1.
*    2.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    /* This enables RTOS aware debugging in OpenOCD. */
    uxTopUsedPriority = configMAX_PRIORITIES - 1;

    #if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
    #endif

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    result |= initialize_board_leds();
    SEGGER_RTT_Init();
    /* Divider configuration for 2MBaud */
    cy_en_divider_types_t divider_type = CY_SYSCLK_DIV_16_5_BIT;
    Cy_SysClk_PeriphDisableDivider(divider_type, 0U);
    Cy_SysClk_PeriphSetFracDivider(divider_type, 0U, 3U, 22U);
    Cy_SysClk_PeriphEnableDivider(divider_type, 0U);

    debug_pins_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Init UART interface */
    uart_init();

    /* Initialize board */
    result = xensiv_bgt60cutrxx_board_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /*
        Create the process tasks

        PRIORITY_MAX - 2     PRIORITY_MAX - 3      PRIORITY_MAX - 1
        process_task_init()  terminal_task_init()  radar_task_init()
     */
    /* Init radar task which will start radar acquisition */
    radar_task_init();
    process_task_init();
    terminal_task_init();
    radar_ble_init();
    radar_wifi_init();

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    CY_ASSERT(0);
}


/**
 * @brief Initializes the XENSIV BGT60CUTRXX platform GPIO pins.
 *
 * This function initializes the GPIO pins required for the XENSIV BGT60CUTRXX sensor,
 * including the LDO enable pin, SPI chip select pin, reset pin, and other necessary
 * GPIO pins based on the target platform.
 *
 * @return int32_t Returns XENSIV_BGT60CUTRXX_STATUS_OK (0) on success, or a negative
 *                 error code on failure (-1 to -6).
 */
static int32_t xensiv_bgt60cutrxx_board_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Enable LDO */
    result |= cyhal_gpio_init(PIN_XENSIV_BGT60CUTRXX_LDO_EN, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1);

    /* Set specific pins to disable BGT60CUTRXX bootloader */
    result |= cyhal_gpio_init(PIN_XENSIV_BGT60CUTRXX_DIO2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1);
    result |= cyhal_gpio_init(PIN_XENSIV_BGT60CUTRXX_DIO3, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1);

    /* Set level shifter direction */
    result |= cyhal_gpio_init(SHIELD_XENSIV_DIR4_EN, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1);

    if (result != CY_RSLT_SUCCESS)
    {
        /* Board specific settings failed. Stop program execution */
        CY_ASSERT(0);
    }

    return XENSIV_BGT60CUTRXX_STATUS_OK;
}


void vApplicationIdleHook(void)
{
}


/* [] END OF FILE */
