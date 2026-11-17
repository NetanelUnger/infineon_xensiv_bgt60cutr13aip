/*******************************************************************************
 * File Name: cycfg_dmas.c
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

#include "cycfg_dmas.h"

cy_stc_dma_descriptor_config_t DMA_SPITX_Descriptor_0_config =
{
    .retrigger = CY_DMA_RETRIG_IM,
    .interruptType = CY_DMA_DESCR_CHAIN,
    .triggerOutType = CY_DMA_DESCR_CHAIN,
    .channelState = CY_DMA_CHANNEL_ENABLED,
    .triggerInType = CY_DMA_1ELEMENT,
    .dataSize = CY_DMA_HALFWORD,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_DATA,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .descriptorType = CY_DMA_1D_TRANSFER,
    .srcAddress = NULL,
    .dstAddress = NULL,
    .srcXincrement = 1,
    .dstXincrement = 0,
    .xCount = 9,
    .srcYincrement = 0,
    .dstYincrement = 0,
    .yCount = 1,
    .nextDescriptor = &DMA_SPITX_Descriptor_1,
};
cy_stc_dma_descriptor_config_t DMA_SPITX_Descriptor_1_config =
{
    .retrigger = CY_DMA_RETRIG_IM,
    .interruptType = CY_DMA_DESCR_CHAIN,
    .triggerOutType = CY_DMA_DESCR_CHAIN,
    .channelState = CY_DMA_CHANNEL_ENABLED,
    .triggerInType = CY_DMA_1ELEMENT,
    .dataSize = CY_DMA_HALFWORD,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_DATA,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .descriptorType = CY_DMA_2D_TRANSFER,
    .srcAddress = NULL,
    .dstAddress = NULL,
    .srcXincrement = 1,
    .dstXincrement = 0,
    .xCount = 5,
    .srcYincrement = 5,
    .dstYincrement = 0,
    .yCount = 2,
    .nextDescriptor = NULL,
};
cy_stc_dma_descriptor_t DMA_SPITX_Descriptor_0 =
{
    .ctl = 0UL,
    .src = 0UL,
    .dst = 0UL,
    .xCtl = 0UL,
    .yCtl = 0UL,
    .nextPtr = 0UL,
};
cy_stc_dma_descriptor_t DMA_SPITX_Descriptor_1 =
{
    .ctl = 0UL,
    .src = 0UL,
    .dst = 0UL,
    .xCtl = 0UL,
    .yCtl = 0UL,
    .nextPtr = 0UL,
};
cy_stc_dma_channel_config_t DMA_SPITX_channelConfig =
{
    .descriptor = &DMA_SPITX_Descriptor_0,
    .preemptable = true,
    .priority = 3,
    .enable = false,
    .bufferable = false,
};
cy_stc_dma_crc_config_t DMA_SPITX_crcConfig =
{
    .dataReverse = false,
    .dataXor = 0,
    .reminderReverse = false,
    .reminderXor = 0,
    .polynomial = 79764919,
};

#if defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE)
const cyhal_resource_inst_t DMA_SPITX_obj =
{
    .type = CYHAL_RSC_DW,
    .block_num = 0U,
    .channel_num = DMA_SPITX_CHANNEL,
};
const cyhal_dma_configurator_t DMA_SPITX_hal_config =
{
    .resource = &DMA_SPITX_obj,
    .dw_channel_config = &DMA_SPITX_channelConfig,
    .dw_descriptor_config = &DMA_SPITX_Descriptor_0_config,
};
#endif /* defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE) */

#if defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA)
mtb_hal_dma_configurator_t DMA_SPITX_hal_config =
{
    .dma_type = MTB_HAL_DMA_DW,
    .channel = DMA_SPITX_CHANNEL,
    .dw_base = DMA_SPITX_HW,
    .dw_descriptor = &DMA_SPITX_Descriptor_0,
};
#endif /* defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA) */

cy_stc_dma_descriptor_config_t DMA_SPIRX_Descriptor_0_config =
{
    .retrigger = CY_DMA_RETRIG_IM,
    .interruptType = CY_DMA_DESCR_CHAIN,
    .triggerOutType = CY_DMA_DESCR_CHAIN,
    .channelState = CY_DMA_CHANNEL_ENABLED,
    .triggerInType = CY_DMA_1ELEMENT,
    .dataSize = CY_DMA_HALFWORD,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_DATA,
    .descriptorType = CY_DMA_2D_TRANSFER,
    .srcAddress = NULL,
    .dstAddress = NULL,
    .srcXincrement = 0,
    .dstXincrement = 1,
    .xCount = 5,
    .srcYincrement = 0,
    .dstYincrement = 5,
    .yCount = 2,
    .nextDescriptor = &DMA_SPIRX_Descriptor_1,
};
cy_stc_dma_descriptor_config_t DMA_SPIRX_Descriptor_1_config =
{
    .retrigger = CY_DMA_RETRIG_IM,
    .interruptType = CY_DMA_DESCR_CHAIN,
    .triggerOutType = CY_DMA_DESCR_CHAIN,
    .channelState = CY_DMA_CHANNEL_ENABLED,
    .triggerInType = CY_DMA_1ELEMENT,
    .dataSize = CY_DMA_HALFWORD,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_DATA,
    .descriptorType = CY_DMA_1D_TRANSFER,
    .srcAddress = NULL,
    .dstAddress = NULL,
    .srcXincrement = 0,
    .dstXincrement = 1,
    .xCount = 9,
    .srcYincrement = 0,
    .dstYincrement = 0,
    .yCount = 1,
    .nextDescriptor = NULL,
};
cy_stc_dma_descriptor_t DMA_SPIRX_Descriptor_0 =
{
    .ctl = 0UL,
    .src = 0UL,
    .dst = 0UL,
    .xCtl = 0UL,
    .yCtl = 0UL,
    .nextPtr = 0UL,
};
cy_stc_dma_descriptor_t DMA_SPIRX_Descriptor_1 =
{
    .ctl = 0UL,
    .src = 0UL,
    .dst = 0UL,
    .xCtl = 0UL,
    .yCtl = 0UL,
    .nextPtr = 0UL,
};
cy_stc_dma_channel_config_t DMA_SPIRX_channelConfig =
{
    .descriptor = &DMA_SPIRX_Descriptor_0,
    .preemptable = true,
    .priority = 3,
    .enable = false,
    .bufferable = false,
};
cy_stc_dma_crc_config_t DMA_SPIRX_crcConfig =
{
    .dataReverse = false,
    .dataXor = 0,
    .reminderReverse = false,
    .reminderXor = 0,
    .polynomial = 79764919,
};

#if defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE)
const cyhal_resource_inst_t DMA_SPIRX_obj =
{
    .type = CYHAL_RSC_DW,
    .block_num = 0U,
    .channel_num = DMA_SPIRX_CHANNEL,
};
const cyhal_dma_configurator_t DMA_SPIRX_hal_config =
{
    .resource = &DMA_SPIRX_obj,
    .dw_channel_config = &DMA_SPIRX_channelConfig,
    .dw_descriptor_config = &DMA_SPIRX_Descriptor_0_config,
};
#endif /* defined (CY_USING_HAL) || defined (CY_USING_HAL_LITE) */

#if defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA)
mtb_hal_dma_configurator_t DMA_SPIRX_hal_config =
{
    .dma_type = MTB_HAL_DMA_DW,
    .channel = DMA_SPIRX_CHANNEL,
    .dw_base = DMA_SPIRX_HW,
    .dw_descriptor = &DMA_SPIRX_Descriptor_0,
};
#endif /* defined (COMPONENT_MTB_HAL) && (MTB_HAL_DRIVER_AVAILABLE_DMA) */

void reserve_cycfg_dmas(void)
{
#if defined (CY_USING_HAL)
    cyhal_hwmgr_reserve(&DMA_SPITX_obj);
    cyhal_hwmgr_reserve(&DMA_SPIRX_obj);
#endif /* defined (CY_USING_HAL) */
}
