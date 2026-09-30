/***************************************************************************//**
* \file tmc_class.h
* \version 1.0
*
* \brief Header file providing declarations and definitions for the USB 
*        TMC class.
*
*******************************************************************************
* \copyright
* (c) (2026), Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
*
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

#if USB_TMC_EN

#ifndef _TMC_CLASS_H_
#define _TMC_CLASS_H_

#include "usb_tmc_spec.h"

typedef enum cy_en_tmc_cls_sm
{
    CYFX_TMC_CLS_SM_IDLE                = 0,
} cy_en_tmc_cls_sm;

typedef void (*PFN_Cy_TMC_Class_HaltTransfer)(void);

extern CyFxReturnStatus_t
Cy_TMC_Class_Init(
        void);

extern void Cy_TMC_Class_Reset(
        cy_stc_hbdma_channel_t *pMsgInDmaChannel,
        cy_stc_hbdma_channel_t* pNtfInDmaChannel,
        uint32_t DmaSize);

extern void
Cy_TMC_Class_Abort(
        void);

extern bool
Cy_TMC_Class_InterfaceRequestHandler(
        cy_stc_usb_app_ctxt_t *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        uint8_t bReqType,
        uint8_t bRequest,
        uint16_t wValue,
        uint16_t wLength);

extern bool
Cy_TMC_Class_EndpointRequestHandler(
        cy_stc_usb_app_ctxt_t *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        uint8_t bReqType,
        uint8_t bRequest,
        uint16_t wValue,
        uint16_t wIndex,
        uint16_t wLength);

extern void
Cy_TMC_Class_HaltMsgOutTransfer(
        void);

extern void
Cy_TMC_Class_HaltMsgInTransfer(
        void);

extern void
Cy_TMC_Class_HaltNtfInTransfer(
        void);

extern CyFxReturnStatus_t
Cy_TMC_Class_MsgOutDmaHandler(
        cy_stc_usb_app_ctxt_t *pAppCtxt,
        void* pDmaBuf,
        uint8_t DmaSize);

extern CyFxReturnStatus_t
Cy_TMC_Class_IssueMsgInXfer(
        cy_stc_usb_app_ctxt_t *pAppCtxt);

extern CyFxReturnStatus_t
Cy_TMC_Class_IssueNtfInXfer(
        cy_stc_usb_app_ctxt_t *pAppCtxt);

#endif /* _TMC_CLASS_H_ */

#endif /* USB_TMC_EN */

