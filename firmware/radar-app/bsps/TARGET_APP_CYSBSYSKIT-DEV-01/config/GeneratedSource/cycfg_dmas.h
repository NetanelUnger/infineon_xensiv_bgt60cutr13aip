/*******************************************************************************
 * File Name: cycfg_dmas.h
 *
 * Description:
 * DMA configuration
 * This file was automatically generated and should not be modified.
 * Configurator Backend 3.90.0
 * device-db 4.41.0.11271
 * mtb-pdl-cat1 3.25.0.47604
 *
 *******************************************************************************
 * Copyright 2026, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
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
 ******************************************************************************/

#if !defined(CYCFG_DMAS_H)
#define CYCFG_DMAS_H

#include "cycfg_notices.h"
#include "cy_dma.h"

#if defined (CY_USING_HAL)
#include "cyhal_hwmgr.h"
#endif /* defined (CY_USING_HAL) */

#if defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE)
#include "cyhal_dma.h"
#endif /* defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE) */

#if defined (COMPONENT_MTB_HAL)
#include "mtb_hal.h"
#endif /* defined (COMPONENT_MTB_HAL) */

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#define DMA_SPITX_ENABLED 1U
#define DMA_SPITX_HW DW0
#define DMA_SPITX_CHANNEL 26U
#define DMA_SPITX_IRQ cpuss_interrupts_dw0_26_IRQn
#define DMA_SPIRX_ENABLED 1U
#define DMA_SPIRX_HW DW0
#define DMA_SPIRX_CHANNEL 27U
#define DMA_SPIRX_IRQ cpuss_interrupts_dw0_27_IRQn

extern cy_stc_dma_descriptor_config_t DMA_SPITX_Descriptor_0_config;
extern cy_stc_dma_descriptor_config_t DMA_SPITX_Descriptor_1_config;
extern cy_stc_dma_descriptor_t DMA_SPITX_Descriptor_0;
extern cy_stc_dma_descriptor_t DMA_SPITX_Descriptor_1;
extern cy_stc_dma_channel_config_t DMA_SPITX_channelConfig;
extern cy_stc_dma_crc_config_t DMA_SPITX_crcConfig;

#if defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE)
extern const cyhal_resource_inst_t DMA_SPITX_obj;
extern const cyhal_dma_configurator_t DMA_SPITX_hal_config;
#endif /* defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE) */

#if defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA)
extern mtb_hal_dma_configurator_t DMA_SPITX_hal_config;
#endif /* defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA) */

extern cy_stc_dma_descriptor_config_t DMA_SPIRX_Descriptor_0_config;
extern cy_stc_dma_descriptor_config_t DMA_SPIRX_Descriptor_1_config;
extern cy_stc_dma_descriptor_t DMA_SPIRX_Descriptor_0;
extern cy_stc_dma_descriptor_t DMA_SPIRX_Descriptor_1;
extern cy_stc_dma_channel_config_t DMA_SPIRX_channelConfig;
extern cy_stc_dma_crc_config_t DMA_SPIRX_crcConfig;

#if defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE)
extern const cyhal_resource_inst_t DMA_SPIRX_obj;
extern const cyhal_dma_configurator_t DMA_SPIRX_hal_config;
#endif /* defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE) */

#if defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA)
extern mtb_hal_dma_configurator_t DMA_SPIRX_hal_config;
#endif /* defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA) */

void reserve_cycfg_dmas(void);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* CYCFG_DMAS_H */
