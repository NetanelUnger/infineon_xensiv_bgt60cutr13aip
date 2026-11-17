/*
 * resource_map.h
 *
 *
 ********************************************************************************
 * Copyright (c) (2016-2022), Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#ifndef RESOURCE_MAP_H_
#define RESOURCE_MAP_H_

/**
 * \defgroup resource_map Resource Map
 * @{
 */

#include "cybsp.h"

#define XENSIV_BGT60CUTRXX_SPI_FREQUENCY (18750000UL)
/** LED pins for radar shield feather adapter board*/
#define SHIELD_XENSIV_RADAR_LED_RED           CYBSP_GPIOA0
/** LED state definitions need to be inverted */
#define SHIELD_XENSIV_RADAR_LED_RED_ON        CYBSP_LED_STATE_OFF
/** LED state definitions need to be inverted */
#define SHIELD_XENSIV_RADAR_LED_RED_OFF       CYBSP_LED_STATE_ON

#define SHIELD_XENSIV_RADAR_LED_GREEN         CYBSP_GPIOA1
/** LED state definitions need to be inverted */
#define SHIELD_XENSIV_RADAR_LED_GREEN_ON     CYBSP_LED_STATE_OFF
/** LED state definitions need to be inverted */
#define SHIELD_XENSIV_RADAR_LED_GREEN_OFF    CYBSP_LED_STATE_ON

#define SHIELD_XENSIV_USER_SWITCH             CYBSP_GPIO6
#define SHIELD_XENSIV_DIR4_EN                 CYBSP_GPIOA2

#define LEVEL_SHIFTER_DIR4_EN               CYBSP_GPIOA2
#define CYBSP_SMARTAR_DIO5 CYBSP_GPIO12
#define LED_RED                             CYBSP_GPIOA0


#define PIN_XENSIV_BGT60CUTRXX_SPI_SCLK       CYBSP_SPI_CLK
#define PIN_XENSIV_BGT60CUTRXX_SPI_MOSI       CYBSP_SPI_MOSI
#define PIN_XENSIV_BGT60CUTRXX_SPI_MISO       CYBSP_SPI_MISO
#define PIN_XENSIV_BGT60CUTRXX_SPI_CSN        CYBSP_SPI_CS
#define PIN_XENSIV_BGT60CUTRXX_IRQ            CYBSP_GPIO10
#define PIN_XENSIV_BGT60CUTRXX_RSTN           CYBSP_GPIOA5
#define PIN_XENSIV_BGT60CUTRXX_LDO_EN         CYBSP_GPIO5

#define PIN_XENSIV_BGT60CUTRXX_TARGET_DETECT  CYBSP_GPIO13
#define PIN_XENSIV_BGT60CUTRXX_DIO2           PIN_XENSIV_BGT60CUTRXX_TARGET_DETECT
#define PIN_XENSIV_BGT60CUTRXX_DIO3           CYBSP_GPIO11

#define DEBUG_PIN_0 P10_0
#define DEBUG_PIN_1 P10_1
#define DEBUG_PIN_2 P10_2
#define debug_pins_init() \
    do { \
        cyhal_gpio_init(DEBUG_PIN_0, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1U); \
        cyhal_gpio_init(DEBUG_PIN_1, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1U); \
        cyhal_gpio_init(DEBUG_PIN_2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1U); \
    } while (0)

#define debug_pin_toggle(PIN) cyhal_gpio_toggle(PIN)


/**@}*/
#endif /* RESOURCE_MAP_H_ */
