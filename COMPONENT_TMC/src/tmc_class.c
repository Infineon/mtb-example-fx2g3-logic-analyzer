/***************************************************************************//**
* \file tmc_class.c
* \version 1.0
*
* \brief Implements the USB Test and Measurement Class TMC.
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

#include "usb_app.h"
#include "tmc_class.h"
#include "tmc_scpi.h"
#include "usb_tmc_spec.h"
#include "ieee488_spec.h"

/* Debug logs for TMC. Not implemented at present.
 * Can be mapped to FX2G3 debug logging function (Cy_Debug_AddToLog)
 */
#define DEBUG_MSG_TMC_REQ(...)
#define DEBUG_MSG_MO_DMA(...)
#define DEBUG_BUF_MO_DMA(...)
#define DEBUG_MSG_MI_DMA(...)
#define DEBUG_BUF_MI_DMA(...)
#define DEBUG_MSG_NI_DMA(...)
#define DEBUG_BUF_NI_DMA(...)

typedef struct cy_stc_ieee488_ctx
{
    IEEE488_ASSERTION                   m_Assertion;
    IEEE488_SM_DT                       m_SM_DT;
    IEEE488_SM_RL                       m_SM_RL;
    IEEE488_SM_SR                       m_SM_SR;
} cy_stc_ieee488_ctx;

typedef struct cy_stc_tmc_class_ctx
{
    uint8_t                             m_MsgInBuffer[1024];
    USBTMC_CAPABILITIES                 m_Capabilities;
    cy_stc_hbdma_channel_t*             m_pMsgInDmaChannel;
    cy_stc_hbdma_channel_t*             m_pNtfInDmaChannel;
    uint32_t                            m_DmaSize;
    uint32_t                            m_MsgInSize;
    USBTMC_NOTIFY_DATA                  m_NotifyData;
    USBTMC_STATUS                       m_TmcStatus;
    cy_stc_ieee488_ctx                  m_IEEE488Ctx;
} cy_stc_tmc_class_ctx;

/* Global TMC class context structure. */
static cy_stc_tmc_class_ctx g_TmcClassCtx __attribute__((aligned(32)));

/**
 * \name Cy_TMC_Class_ValidateControlRequest
 * \brief Validate Control Request
 * \param bReqType Request Type
 * \param bRequest Request
 * \param wReqLength Request Length
 * \param CheckDir end point direction
 * \param wCheckLength length check
 * \param pszReqName name of the request
 * \retval success or error code
 */
static CyFxReturnStatus_t
Cy_TMC_Class_ValidateControlRequest (
    uint8_t     bReqType,
    uint8_t     bRequest,
    uint16_t    wReqLength,
    uint8_t     CheckDir,
    uint16_t    wCheckLength,
    char*       pszReqName
    )
{
    if ((bReqType & 0x80) != CheckDir)
    {
        LOG_ERROR("TMC: Invalid Request - Request:%xh Type:%xh\r\n", bRequest, bReqType);
        return CY_FX_ERROR_BAD_ARGUMENT;
    }
    else
    {
        if (wReqLength < wCheckLength)
        {
            LOG_ERROR("TMC: Invalid Request Size - Request:%xh Length:%xh<%xh\r\n", bRequest, wReqLength, wCheckLength);
            return CY_FX_ERROR_BAD_SIZE;
        }
        else
        {
            if ((pszReqName != NULL) && (pszReqName[0] != 0))
            {
                LOG_COLOR("TMC: Request: %s\r\n", pszReqName);
            }

            return CY_FX_USB_SUCCESS;
        }
    }
}

/**
 * \name Cy_TMC_Class_MsgOutHandler
 * \brief Message Out Handler
 * \param pAppCtxt application layer context pointer
 * \param pMsgReqHdr Header
 * \retval success or error code
 */
static CyFxReturnStatus_t
Cy_TMC_Class_MsgOutHandler (
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    const USBTMC_DEV_DEP_MSG_OUT_HDR* pMsgReqHdr
    )
{
    CyFxReturnStatus_t status;
    const char* pszMsgReq = (const char*)(pMsgReqHdr + 1);

    if (pMsgReqHdr->m_TransferSize == 0)
    {
        LOG_ERROR("TMC: No Message:%d Tag:%xh Zero Size\r\n", pMsgReqHdr->m_MsgID, pMsgReqHdr->m_bTag);
        status = CY_FX_ERROR_BAD_ARGUMENT;
    }
    else
    {
        status = Cy_TMC_Scpi_ParserCommand(pszMsgReq, pMsgReqHdr->m_TransferSize);
    }

    return status;
}

/**
 * \name Cy_TMC_Class_RequestMsgInHandler
 * \brief Message In Handler
 * \param pAppCtxt application layer context pointer
 * \param pMsgReqHdr Header
 * \retval success or error code
 */
static CyFxReturnStatus_t
Cy_TMC_Class_RequestMsgInHandler (
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    const USBTMC_REQUEST_DEV_DEP_MSG_IN_HDR* pMsgReqHdr
    )
{
    USBTMC_RESPONSE_DEV_DEP_MSG_IN_HDR* pMsgRspHdr = (USBTMC_RESPONSE_DEV_DEP_MSG_IN_HDR*)g_TmcClassCtx.m_MsgInBuffer;
    uint8_t* pData;
    uint8_t* pSrc;
    uint8_t* pDst;
    uint32_t Padding;
    uint32_t ResponseSize;

    /* Zero out the response header to start with */
    memset((void *)pMsgRspHdr, 0, sizeof(*pMsgRspHdr));

    pDst = pData = (uint8_t*)(pMsgRspHdr + 1);
    pSrc = Cy_TMC_Scpi_GetParserResponse(&ResponseSize);

    if (ResponseSize > 0)
    {
        memcpy(pDst, pSrc, ResponseSize);
        pDst += ResponseSize;

        if (pMsgReqHdr->m_bmTransferAttributes & USBTMC_ATTRIBUTE_TERM_CHAR_ENABLED)
        {
            *pDst++ = pMsgReqHdr->m_TermChar;
        }
    }

    pMsgRspHdr->m_MsgID        = pMsgReqHdr->m_MsgID;
    pMsgRspHdr->m_bTag         = pMsgReqHdr->m_bTag;
    pMsgRspHdr->m_bTagInverse  = pMsgReqHdr->m_bTagInverse;
    pMsgRspHdr->m_TransferSize = (uint32_t)(pDst - pData);
    pMsgRspHdr->m_TermChar     = pMsgReqHdr->m_TermChar;

    pMsgRspHdr->m_bmTransferAttributes = (pMsgReqHdr->m_bmTransferAttributes & USBTMC_ATTRIBUTE_TERM_CHAR_ENABLED) |
                                         USBTMC_ATTRIBUTE_EOM;

    /* Fill zeros for alignment */
    Padding = (uint8_t)(pMsgRspHdr->m_TransferSize & 0x03);
    switch (Padding)
    {
        case 1:         /* Need to add 3 zeroes */
            *pDst++ = 0;

        case 2:         /* Need to add 2 zeroes */
            *pDst++ = 0;

        case 3:         /* Need to add 1 zero */
            *pDst++ = 0;

        default:
            break;
    }

    g_TmcClassCtx.m_MsgInSize = (sizeof (*pMsgRspHdr) + pMsgRspHdr->m_TransferSize + 3) & (~3);

    return Cy_TMC_Class_IssueMsgInXfer (pAppCtxt);
}

/**
 * \name Cy_TMC_Class_Init
 * \brief Message In Handler
 * \retval success or error code
 */
CyFxReturnStatus_t
Cy_TMC_Class_Init (
    void
    )
{
    Cy_TMC_Class_Reset(NULL, NULL, 1024);
    Cy_TMC_Scpi_Init();

    return CY_FX_USB_SUCCESS;
}

/**
 * \name Cy_TMC_Class_Reset
 * \brief Message In Handler
 * \param pMsgInDmaChannel DMA Channel handle
 * \param pNtfInDmaChannel DMA Channel handle
 * \retval success or error code
 */
void
Cy_TMC_Class_Reset (
    cy_stc_hbdma_channel_t* pMsgInDmaChannel,
    cy_stc_hbdma_channel_t* pNtfInDmaChannel,
    uint32_t DmaSize
    )
{
    USBTMC_CAPABILITIES* pCapabilities;

    memset((void *)&g_TmcClassCtx, 0, sizeof(g_TmcClassCtx));

    g_TmcClassCtx.m_pMsgInDmaChannel = pMsgInDmaChannel;
    g_TmcClassCtx.m_pNtfInDmaChannel = pNtfInDmaChannel;
    g_TmcClassCtx.m_DmaSize = DmaSize;
    g_TmcClassCtx.m_NotifyData.m_Notify1 = 0x80;
    g_TmcClassCtx.m_TmcStatus = USBTMC_STATUS_SUCCESS;

    pCapabilities = &g_TmcClassCtx.m_Capabilities;

    pCapabilities->m_Status                      = USBTMC_STATUS_SUCCESS;
    pCapabilities->m_bcdUSBTMC                   = USBTMC_BCD_VERSION;
    pCapabilities->m_USBTMCInterfaceCapabilities = USBTMC_USBTMCINTF_CAP_INDICATOR_PULSE;
    pCapabilities->m_USBTMCDeviceCapabilities    = USBTMC_USBTMCDEV_CAP_TERM_CHAR;

#if (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488)
    pCapabilities->m_bcdUSB488                   = USB488_BCD_VERSION;
    pCapabilities->m_USB488InterfaceCapabilities = USBTMC_USB488INTF_CAP_TRIGGER |
        USBTMC_USB488INTF_CAP_CONTROLS | USBTMC_USB488INTF_CAP_IEEE488;
    pCapabilities->m_USB488DeviceCapabilities    = USBTMC_USB488DEV_CAP_DT |
        USBTMC_USB488DEV_CAP_RL | USBTMC_USB488DEV_CAP_SR | USBTMC_USB488DEV_CAP_SCPI;
#endif /* (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488) */

    g_TmcClassCtx.m_IEEE488Ctx.m_SM_DT = IEEE488_SM_DT_DTIS;
    g_TmcClassCtx.m_IEEE488Ctx.m_SM_RL = IEEE488_SM_RL_LOCS;
    g_TmcClassCtx.m_IEEE488Ctx.m_SM_SR = IEEE488_SM_SR_NPRS;
}

/**
 * \name Cy_TMC_Class_Abort
 * \brief Place-holder for TMC class abort handling.
 * \retval none
 */
void
Cy_TMC_Class_Abort (
    void
    )
{
}

/**
 * \name Cy_TMC_Class_InterfaceRequestHandler
 * \brief TMC interface request handler
 *
 * \param pAppCtxt application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param bReqType Request Type
 * \param bRequest Request
 * \param wValue value
 * \param wLength Length
 * \retval true or false
 */
bool
Cy_TMC_Class_InterfaceRequestHandler (
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
    uint8_t     bReqType,
    uint8_t     bRequest,
    uint16_t    wValue,
    uint16_t    wLength
)
{
    CyFxReturnStatus_t status;
    bool isHandled = false;

    switch (bRequest)
    {
    case USBTMC_REQ_INITIATE_CLEAR:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, 1, "initiate clear");
        if (status != CY_FX_USB_SUCCESS)
        {
            break;
        }

        g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
        if (wLength > 1)
        {
            memset(g_CtrlXferBuffer + 1, 0, wLength - 1);
        }

        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *) g_CtrlXferBuffer,wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            isHandled = true;
        }
        break;

    case USBTMC_REQ_CHECK_CLEAR_STATUS:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, 2, "check clear");
        if (status != CY_FX_USB_SUCCESS)
        {
            break;
        }

        g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
        g_CtrlXferBuffer[1] = 0;
        if (wLength > 2)
        {
            memset(g_CtrlXferBuffer + 2, 0, wLength - 2);
        }

        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            isHandled = true;
        }
        break;

    case USBTMC_REQ_GET_CAPABILITIES:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, sizeof(g_TmcClassCtx.m_Capabilities), "get capabilities");
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("status: 0x%xh of USBTMC_REQ_GET_CAPABILITIES", status);
            break;
        }

        memcpy(g_CtrlXferBuffer, (uint8_t*)&g_TmcClassCtx.m_Capabilities, sizeof(g_TmcClassCtx.m_Capabilities));
        if (wLength > sizeof(g_TmcClassCtx.m_Capabilities))
        {
            memset(g_CtrlXferBuffer + sizeof(g_TmcClassCtx.m_Capabilities), 0,
                    wLength - sizeof(g_TmcClassCtx.m_Capabilities));
        }


        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            isHandled = true;
        }
        break;

    case USBTMC_REQ_INDICATOR_PULSE:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, sizeof(g_TmcClassCtx.m_TmcStatus), "indicator pulse");
        if (status != CY_FX_USB_SUCCESS)
        {
            break;
        }

        memcpy(g_CtrlXferBuffer, &g_TmcClassCtx.m_TmcStatus, sizeof(g_TmcClassCtx.m_TmcStatus));
        if (wLength > sizeof(g_TmcClassCtx.m_TmcStatus))
        {
            memset(g_CtrlXferBuffer + sizeof(g_TmcClassCtx.m_TmcStatus), 0, wLength - sizeof(g_TmcClassCtx.m_TmcStatus));
        }


        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            isHandled = true;
        }
        break;

#if (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488)
    case USBTMC_REQ_READ_STATUS_BYTE:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, 3, "read status byte");
        if (status != CY_FX_USB_SUCCESS)
        {
            break;
        }

        g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
        g_CtrlXferBuffer[1] = g_TmcClassCtx.m_NotifyData.m_Notify1 = (uint8_t)(wValue & 0x7F);
        g_CtrlXferBuffer[2] = g_TmcClassCtx.m_NotifyData.m_Notify2 = Cy_TMC_Scpi_GetSTB ();
        if (wLength > 3)
        {
            memset(g_CtrlXferBuffer + 3, 0, wLength - 3);
        }

        DBG_APP_TRACE("TMC: Buf[0]=%x Buf[1]%x Buf[2]=%x\r\n",
                g_CtrlXferBuffer[0], g_CtrlXferBuffer[1], g_CtrlXferBuffer[2]);

        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)g_CtrlXferBuffer, wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            g_TmcClassCtx.m_NotifyData.m_Notify1 |= 0x80;
            Cy_TMC_Class_IssueNtfInXfer(pAppCtxt);
            isHandled = true;
        }
        break;

    case USBTMC_REQ_REN_CONTROL:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, 1, "ren ctl");
        if (status != CY_FX_USB_SUCCESS)
        {
            break;
        }

        g_TmcClassCtx.m_IEEE488Ctx.m_Assertion.REN = (uint8_t)wValue;
        g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
        if (wLength > 1)
        {
            memset(g_CtrlXferBuffer + 1, 0, wLength - 1);
        }

        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            isHandled = true;
        }
        break;

    case USBTMC_REQ_GO_TO_LOCAL:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, 1, "goto local");
        if (status != CY_FX_USB_SUCCESS)
        {
            break;
        }

        g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
        if (wLength > 1)
        {
            memset(g_CtrlXferBuffer + 1, 0, wLength - 1);
        }

        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            isHandled = true;
        }
        break;

    case USBTMC_REQ_LOCAL_LOCKOUT:
        status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                CY_FX_USB_REQUEST_IN, 1, "local lockout");
        if (status != CY_FX_USB_SUCCESS)
        {
            break;
        }

        g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
        if (wLength > 1)
        {
            memset(g_CtrlXferBuffer + 1, 0, wLength - 1);
        }

        status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,wLength);
        if (status != CY_FX_USB_SUCCESS)
        {
            LOG_ERROR("TMC: Send EP0 Data Failed. Request: %xh Error:%xh", bRequest, status);
        }
        else
        {
            isHandled = true;
        }
        break;
#endif /* (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488) */

    default:
        LOG_ERROR("TMC: Unhandled Request %xh", bRequest);
        break;
    }

    return isHandled;
}

/**
 * \name Cy_TMC_Class_EndpointRequestHandler
 * \brief TMC endpoint request handler
 *
 * \param pAppCtxt USB application context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param bReqType Request Type
 * \param bRequest Request
 * \param wValue value
 * \param wIndex Index
 * \param wLength Length
 * \retval true or false
 */
bool
Cy_TMC_Class_EndpointRequestHandler (
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
    uint8_t     bReqType,
    uint8_t     bRequest,
    uint16_t    wValue,
    uint16_t    wIndex,
    uint16_t    wLength
)
{
    CyFxReturnStatus_t status;
    bool isHandled = false;

    switch (bRequest)
    {
    case USBTMC_REQ_INITIATE_ABORT_BULK_OUT:
        if (wIndex == TMC_MSG_OUT_ENDPOINT)
        {
            status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                    CY_FX_USB_REQUEST_IN, 2, "initiate abort mo");
            if (status != CY_FX_USB_SUCCESS)
            {
                break;
            }

            Cy_USBTMCAppHaltEndpoint(pAppCtxt,pUsbdCtxt,wIndex);

            g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
            g_CtrlXferBuffer[1] = (uint8_t)(wValue & 0x7F);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,2);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                isHandled = true;
            }
        }
        break;

    case USBTMC_REQ_CHECK_ABORT_BULK_OUT_STATUS:
        if (wIndex == TMC_MSG_OUT_ENDPOINT)
        {
            status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                    CY_FX_USB_REQUEST_IN, 8, "check abort mo");
            if (status != CY_FX_USB_SUCCESS)
            {
                break;
            }

            g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
            memset(g_CtrlXferBuffer + 1, 0, 7);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,8);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                isHandled = true;
            }
        }
        break;

    case USBTMC_REQ_INITIATE_ABORT_BULK_IN:
        if (wIndex == TMC_MSG_IN_ENDPOINT)
        {
            status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                    CY_FX_USB_REQUEST_IN, 2, "initiate abort mi");
            if (status != CY_FX_USB_SUCCESS)
            {
                break;
            }

            Cy_USBTMCAppHaltEndpoint(pAppCtxt,pUsbdCtxt,wIndex);

            g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
            g_CtrlXferBuffer[1] = (uint8_t)(wValue & 0x7F);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,(uint8_t *)g_CtrlXferBuffer,2);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                isHandled = true;
            }
        }
        break;

    case USBTMC_REQ_CHECK_ABORT_BULK_IN_STATUS:
        if (wIndex == TMC_MSG_IN_ENDPOINT)
        {
            status = Cy_TMC_Class_ValidateControlRequest(bReqType, bRequest, wLength,
                    CY_FX_USB_REQUEST_IN, 8, "check abort mi");
            if (status != CY_FX_USB_SUCCESS)
            {
                break;
            }

            g_CtrlXferBuffer[0] = g_TmcClassCtx.m_TmcStatus;
            memset(g_CtrlXferBuffer + 1, 0, 7);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,g_CtrlXferBuffer,8);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                isHandled = true;
            }
        }
        break;

    default:
        LOG_ERROR("TMC: Unhandled Request %xh", bRequest);
        break;
    }

    return isHandled;
}

/**
 * \name Cy_TMC_Class_HaltMsgOutTransfer
 * \brief Halt Message Out Transfer
 * \retval none
 */
void
Cy_TMC_Class_HaltMsgOutTransfer (
    void
    )
{
    g_TmcClassCtx.m_MsgInSize = 0;
    Cy_TMC_Scpi_ResetParserResponse ();
}

/**
 * \name Cy_TMC_Class_HaltMsgInTransfer
 * \brief Halt Message In Transfer
 * \retval none
 */
void
Cy_TMC_Class_HaltMsgInTransfer (
    void
    )
{
    g_TmcClassCtx.m_MsgInSize = 0;
}

/**
 * \name Cy_TMC_Class_HaltNtfInTransfer
 * \brief Halt notifier Message In Transfer
 * \retval none
 */
void
Cy_TMC_Class_HaltNtfInTransfer (
    void
    )
{
    LOG_COLOR("TMC: Halt Interrupt transfer");
}

/**
 * \name Cy_TMC_Class_MsgOutDmaHandler
 * \brief Message Out Transfer
 * \param pAppCtxt application layer context pointer
 * \param pDmaBuf  DMA buffer pointer
 * \param DmaSize DMA size
 * \retval success or failure error code
 */
CyFxReturnStatus_t
Cy_TMC_Class_MsgOutDmaHandler (
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    void*       pDmaBuf,
    uint8_t     DmaSize
    )
{
    CyFxReturnStatus_t status;
    const USBTMC_MSG_COMMON_HDR* pMsgHdr = (const USBTMC_MSG_COMMON_HDR*)pDmaBuf;

    /* Check validity of header control fields. */
    if ((pMsgHdr->m_bTag == 0) ||
        (pMsgHdr->m_bTag != (uint8_t)(~pMsgHdr->m_bTagInverse)) ||
        (pMsgHdr->rsvd_03h != 0))
    {
        LOG_ERROR("TMC: Invalid TMC message %x Tag:%x %x RSVD:%x\r\n",
                pMsgHdr->m_MsgID, pMsgHdr->m_bTag, pMsgHdr->m_bTagInverse,
                pMsgHdr->rsvd_03h);
        status = CY_FX_ERROR_BAD_ARGUMENT;
    }
    else
    {
        switch (pMsgHdr->m_MsgID)
        {
        case USBTMC_MSGID_DEV_DEP_MSG_OUT:
            status = Cy_TMC_Class_MsgOutHandler(pAppCtxt, (USBTMC_DEV_DEP_MSG_OUT_HDR*)pMsgHdr);
            break;

        case USBTMC_MSGID_REQUEST_DEV_DEP_MSG_IN:
            status = Cy_TMC_Class_RequestMsgInHandler(pAppCtxt, (USBTMC_REQUEST_DEV_DEP_MSG_IN_HDR*)pMsgHdr);
            break;

        default:
            LOG_COLOR("TMC: Unhandled TMC message %xh", pMsgHdr->m_MsgID);
            status = CY_FX_ERROR_NOT_SUPPORTED;
            break;
        }
    }

    return status;
}

/**
 * \name Cy_TMC_Class_IssueMsgInXfer
 * \brief Message In Transfer
 * \param pAppCtxt application layer context pointer
 * \retval success or failure error code
 */
CyFxReturnStatus_t
Cy_TMC_Class_IssueMsgInXfer (
    cy_stc_usb_app_ctxt_t *pAppCtxt
    )
{
    cy_stc_hbdma_channel_t* pDmaChannel = g_TmcClassCtx.m_pMsgInDmaChannel;
    cy_en_hbdma_mgr_status_t hbdma_stat = CY_HBDMA_MGR_SUCCESS;
    cy_stc_hbdma_buff_status_t buffStat;
    uint16_t length = g_TmcClassCtx.m_MsgInSize;

    if (0 == length) {
        return CY_FX_ERROR_BAD_SIZE;
    }
    if( NULL == pDmaChannel){
        return CY_FX_ERROR_FAILURE;
    }

    hbdma_stat = Cy_HBDma_Channel_GetBuffer(pDmaChannel, &buffStat);
    if (hbdma_stat != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("LA: HB-DMA GetBuffer Error\r\n");
        return hbdma_stat;
    }
    if (length > buffStat.size) {
        DBG_APP_ERR("LA: Bad message size %d\r\n", length);
        return CY_FX_ERROR_BAD_SIZE;
    }

    memcpy((uint8_t*)buffStat.pBuffer, g_TmcClassCtx.m_MsgInBuffer, length);
    buffStat.count = length;

    hbdma_stat = Cy_HBDma_Channel_CommitBuffer(pDmaChannel, &buffStat);
    if (hbdma_stat != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("LA: HB-DMA CommitBuffer Error\r\n");
        return hbdma_stat;
    }

    g_TmcClassCtx.m_MsgInSize = 0;
    return hbdma_stat;
}


/**
 * \name Cy_TMC_Class_IssueNtfInXfer
 * \brief Halt Message Out Transfer
 * \param pAppCtxt application layer context pointer
 * \retval success or failure error code
 */
CyFxReturnStatus_t
Cy_TMC_Class_IssueNtfInXfer (
    cy_stc_usb_app_ctxt_t *pAppCtxt
    )
{
#if TMC_SUPPORT_INTERRUPT
    uint16_t length = sizeof(g_TmcClassCtx.m_NotifyData);

    cy_stc_hbdma_channel_t* pDmaChannel = g_TmcClassCtx.m_pNtfInDmaChannel;
    cy_en_hbdma_mgr_status_t  hbdma_stat = CY_HBDMA_MGR_SUCCESS;
    cy_stc_hbdma_buff_status_t buffStat;

    if( NULL == pDmaChannel){
        return CY_FX_ERROR_FAILURE;
    }  

    LOG_COLOR(" TMC: Commit size:%d ntf:[%xh %xh]",
            length, g_TmcClassCtx.m_NotifyData.m_Notify1, g_TmcClassCtx.m_NotifyData.m_Notify2);

    hbdma_stat = Cy_HBDma_Channel_GetBuffer(pDmaChannel, &buffStat);
    if (hbdma_stat != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("LA: HB-DMA GetBuffer Error\r\n");
        return hbdma_stat;
    }

    memcpy((uint8_t*)buffStat.pBuffer, (uint8_t*)&g_TmcClassCtx.m_NotifyData, length);
    buffStat.count = length;

    hbdma_stat = Cy_HBDma_Channel_CommitBuffer(pDmaChannel, &buffStat);
    if (hbdma_stat != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("LA: HB-DMA CommitBuffer Error\r\n");
        return hbdma_stat;
    }

    return CY_FX_USB_SUCCESS;
#else
    return CY_FX_ERROR_NOT_SUPPORTED;
#endif /* TMC_SUPPORT_INTERRUPT */
}

#endif /* USB_TMC_EN */

