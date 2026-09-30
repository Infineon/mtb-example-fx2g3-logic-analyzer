/***************************************************************************//**
* \file usb_app.c
* \version 1.0
*
* \brief Implements the USB Protocol Analyzer application.
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

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_usb_common.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_usb_usbd.h"
#include "usb_app.h"
#include "cy_debug.h"
#if USB_TMC_EN
#include <tmc_class.h>
#endif /* USB_TMC_EN */
#include "cy_lvds.h"

volatile bool glIsStartReceived = false;                /* Whether logic analyzer streaming is ongoing */
#if USB_TMC_EN
static volatile bool glIsTMCApplnActive = false;
#endif /* USB_TMC_EN */

uint8_t *g_MsgOutDmaBuffer[LA_TX_MAX_BUFFER_COUNT] = {};
uint8_t *g_MsgInDmaBuffer = NULL;

/* Buffer used for control transfer testing. */
__attribute__ ((section(".descSection"), used)) uint8_t g_CtrlXferBuffer[1024];

/**
 * \name Cy_LA_AppStop
 * \brief Stop the data stream channels
 * \param pAppCtxt application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param IsStreamStart Pass 1 to start streaming, else 0
 * \retval None
 */
static void Cy_LA_AppStop(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint32_t epNumber)
{
    cy_en_hbdma_mgr_status_t status = CY_HBDMA_MGR_SUCCESS;
    cy_stc_hbdma_sock_t sckStat;

    (void)status;
    (void)sckStat;

    DBG_APP_INFO("LA: App Stop epNumber=0x%x\r\n",epNumber);

#if USB_LOGIC_ANALYZER_EN
    if (LOGIC_ANALYZER_BULK_IN_ENDPOINT_1 == epNumber)
    {
        /* Stop the GPIF state machine. */
        Cy_LVDS_GpifSMStop(LVDSSS_LVDS, 0);
        Cy_SysLib_Delay(1);

        /* Reset the DMA channel through which data is received from the LVDS side. */
        status = Cy_HBDma_Channel_Reset(pAppCtxt->hbLADmaChannel);
        ASSERT_NON_BLOCK(CY_HBDMA_MGR_SUCCESS == status, status);

        Cy_HBDma_GetSocketStatus(pAppCtxt->pHbDmaMgrCtxt->pDrvContext,
                pAppCtxt->hbLADmaChannel->prodSckId[0], &sckStat);
        DBG_APP_INFO("LA: DMA Socket %x status is %x\r\n",
                pAppCtxt->hbLADmaChannel->prodSckId[0], sckStat.status);

        Cy_HBDma_GetSocketStatus(pAppCtxt->pHbDmaMgrCtxt->pDrvContext,
                pAppCtxt->hbLADmaChannel->prodSckId[1], &sckStat);
        DBG_APP_INFO("LA: DMA Socket %x status is %x\r\n",
                pAppCtxt->hbLADmaChannel->prodSckId[1], sckStat.status);

        Cy_LA_LvdsDeinit();
    }
#endif /* USB_LOGIC_ANALYZER_EN */

    /* Flush and reset the endpoint and clear the STALL bit. */
    Cy_USBD_FlushEndp(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN);
    Cy_USBD_ResetEndp(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN, false);
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN, false);
}

/**
 * \name Cy_LA_AppHbDmaRxCallback
 * \brief Callback function for the channel receiving data into LVDS socket
 * \param handle HBDMA channel handle
 * \param cy_en_hbdma_cb_type_t HBDMA channel type
 * \param pbufStat fHBDMA buffer status
 * \param userCtx user context
 * \retval None
 */
void Cy_LA_AppHbDmaRxCallback(
        cy_stc_hbdma_channel_t *handle,
        cy_en_hbdma_cb_type_t type,
        cy_stc_hbdma_buff_status_t *pbufStat,
        void *userCtx)
{
    cy_en_hbdma_mgr_status_t status = CY_HBDMA_MGR_SUCCESS;

    if (type == CY_HBDMA_CB_PROD_EVENT)
    {
        /* Commit the buffer so that the data can be sent to the USB host. */
        status = Cy_HBDma_Channel_CommitBuffer(handle, pbufStat);
        if (status != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("LA: CommitBuffer failed with error %x\r\n", status);
            return;
        }
    }
}

#if USB_TMC_EN
/**
 * \name Cy_TMC_AppStart
 * \brief Function to start TMC application
 * \param pUsbdCtxt USBD layer context pointer.
 * \param pAppCtxt application layer context pointer.
 * \retval None
 */
void Cy_TMC_AppStart(
    cy_stc_usb_app_ctxt_t *pAppCtxt,
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt)
{
    LOG_COLOR("TMC: App Start\r\n");

#if (USB_TMC_EN)
    if (!glIsTMCApplnActive) {
        Cy_TMC_Class_Init();
        glIsTMCApplnActive = true;

#if TMC_SUPPORT_INTERRUPT
        Cy_TMC_Class_Reset (pAppCtxt->hbTMCResponseChannel, pAppCtxt->hbTMCNotifyChannel, 0x400);
#else
        Cy_TMC_Class_Reset (pAppCtxt->hbTMCResponseChannel, NULL, 0x400);
#endif /* TMC_SUPPORT_INTERRUPT */

    } else{
        LOG_COLOR("TMC: Already started\r\n");
    }
#endif /* USB_TMC_EN */
}

/**
 * \name Cy_USBTMCAppHaltEndpoint
 * \brief Function to handle endpoint halt
 * \param pUsbdCtxt USBD layer context pointer.
 * \param epNumber USB endpoint
 * \retval None
 */
void
Cy_USBTMCAppHaltEndpoint (
        cy_stc_usb_app_ctxt_t  *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        uint8_t epNumber
        )
{
    cy_stc_hbdma_channel_t* pDmaChannel = NULL;
    PFN_Cy_TMC_Class_HaltTransfer pfnCy_TMC_Class_HaltTransfer = NULL;
    cy_en_usb_endp_dir_t epDir = CY_USB_ENDP_DIR_INVALID;
    bool isStall = false;

    LOG_COLOR("TMC: Halt ep %x \n\r", epNumber);

    if (epNumber == TMC_MSG_OUT_ENDPOINT)
    {
        epDir = CY_USB_ENDP_DIR_OUT;
        pDmaChannel = pAppCtxt->hbTMCCommandChannel;
        pfnCy_TMC_Class_HaltTransfer = Cy_TMC_Class_HaltMsgOutTransfer;

        pAppCtxt->curTmcBufIndex = 0;
    } else if (epNumber == (TMC_MSG_IN_ENDPOINT | 0x80)) {
        epDir = CY_USB_ENDP_DIR_IN;
        pDmaChannel = pAppCtxt->hbTMCResponseChannel;
        pfnCy_TMC_Class_HaltTransfer = Cy_TMC_Class_HaltMsgInTransfer;
#if TMC_SUPPORT_INTERRUPT
    } else if (epNumber == (TMC_MSG_INTR_ENDPOINT | 0x80)) {
        epDir = CY_USB_ENDP_DIR_IN;
        pDmaChannel = NULL;
        pfnCy_TMC_Class_HaltTransfer = Cy_TMC_Class_HaltNtfInTransfer;
#endif
    }

    if (glIsTMCApplnActive) {
        Cy_USB_USBD_EndpSetClearNakNrdy(pUsbdCtxt, epNumber, epDir, true);
        Cy_SysLib_DelayUs(125);

        if (pDmaChannel != NULL)  {
            Cy_HBDma_Channel_Reset(pDmaChannel);
        }

        Cy_USBD_FlushEndp(pUsbdCtxt, epNumber, epDir);
        Cy_USBD_ResetEndp(pUsbdCtxt, epNumber, epDir, true);
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, epNumber, epDir, false);
        Cy_USB_USBD_EndpSetClearNakNrdy(pUsbdCtxt, epNumber, epDir, false);

        /* Check EP Status : Stall/Clear */
        isStall = Cy_USBD_EndpIsStallSet(pUsbdCtxt, epNumber, epDir);
        LOG_COLOR("TMC: Stall status: 0x%x \n\r", isStall);
        if (pfnCy_TMC_Class_HaltTransfer != NULL) {
            pfnCy_TMC_Class_HaltTransfer ();
        }

        if (pDmaChannel != NULL) {
            Cy_HBDma_Channel_Enable(pDmaChannel, 0);
        }
    }
}

#endif /* USB_TMC_EN */

/**
 * \name Cy_LA_StreamHandler
 * \brief Stream handler
 * \param pAppCtxt application layer context pointer.
 * \param wLength length of received data
 * \param isStart start or stop received
 * \retval None
 */
void Cy_LA_StreamHandler(cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength, bool isStart)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t retStatus;
    uint16_t loopCnt = 500u;
    uint32 clkRegValue = 0;
    uint32_t channel_mask = 0;
    uint32_t sampling_freq = 0;
    cy_en_hbdma_mgr_status_t mgrStatus = CY_HBDMA_MGR_SUCCESS;
    static bool isRunning = false;
    cy_en_lvds_gpif_clk_src_t clkSrc = CY_LVDS_GPIF_CLOCK_HF;
    cy_en_lvds_gpif_clk_divider_t clkDivider = CY_LVDS_GPIF_CLOCK_DIV_3;

    DBG_APP_TRACE("LA: Stream Handler\r\n");

    /* Read the data out buffer */
    retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)g_CtrlXferBuffer, wLength);
    if (retStatus == CY_USBD_STATUS_SUCCESS)
    {
        /* Wait until receive DMA transfer has been completed. */
        while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) {
            Cy_SysLib_DelayUs(10);
        }
        if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
            Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
            Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
            return;
        }

        if (isStart) {
            channel_mask = *((uint32_t *)&g_CtrlXferBuffer[0]);
            sampling_freq = *((uint32_t *)&g_CtrlXferBuffer[4]);

            /* Reduce frequency to KHz units. */
            sampling_freq /= 1000;

            switch(sampling_freq) {
            case 1500:
            default:
                DBG_APP_INFO("LA: Sampling Frequency: 1500 KHz \r\n");
                /* Clock GPIF state machine at 75 MHz and interface at 1500 KHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_HF;           /* 75 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_INVALID;  /* 75 MHz */
                clkRegValue  = ((50 << 4) | CY_LVDS_GPIF_CLOCK_HF);
                break;

            case 3000:
                DBG_APP_INFO("LA: Sampling Frequency: 3 MHz \r\n");
                /* Clock GPIF state machine at 75 MHz and interface at 3 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_HF;           /* 75 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_INVALID;  /* 75 MHz */
                clkRegValue  = ((25 << 4) | CY_LVDS_GPIF_CLOCK_HF);
                break;

            case 5000:
                DBG_APP_INFO("LA: Sampling Frequency: 5 MHz \r\n");
                /* Clock GPIF state machine at 75 MHz and interface at 5 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_HF;           /* 75 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_INVALID;  /* 75 MHz */
                clkRegValue  = ((15 << 4) | CY_LVDS_GPIF_CLOCK_HF);
                break;

            case 10000:
                DBG_APP_INFO("LA: Sampling Frequency: 10 MHz \r\n");
                /* Clock GPIF state machine at 160 MHz and interface at 10 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_3;    /* 160 MHz */
                clkRegValue  = ((48 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 12000:
                DBG_APP_INFO("LA: Sampling Frequency: 12 MHz \r\n");
                /* Clock GPIF state machine at 240 MHz and interface at 12 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((40 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 15000:
                DBG_APP_INFO("LA: Sampling Frequency: 15 MHz \r\n");
                /* Clock GPIF state machine at 240 MHz and interface at 15 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((32 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 20000:
                DBG_APP_INFO("LA: Sampling Frequency: 20 MHz \r\n");
                /* Clock GPIF state machine at 240 MHz and interface at 20 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((24 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 24000:
                DBG_APP_INFO("LA: Sampling Frequency: 24 MHz \r\n");
                /* Clock GPIF state machine at 240 MHz and interface at 24 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((20 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 25000:
                DBG_APP_INFO("LA: Sampling Frequency: 25 MHz \r\n");
                /* Clock GPIF state machine at 75 MHz and interface at 25 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_HF;                   /* 75 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_INVALID;          /* 75 MHz */
                clkRegValue  = ((3 << 4) | CY_LVDS_GPIF_CLOCK_HF);
                break;

            case 30000:
                DBG_APP_INFO("LA: Sampling Frequency: 30 MHz \r\n");
                /* Clock GPIF state machine at 240 MHz and interface at 30 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((16 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 35000:
                DBG_APP_INFO("LA: Sampling Frequency: 35 MHz \r\n");;
                /* Clock GPIF state machine at 240 MHz and interface at 34.3 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((14 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 40000:
                DBG_APP_INFO("LA: Sampling Frequency: 40 MHz \r\n");
                /* Clock GPIF state machine at 240 MHz and interface at 40 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((12 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;

            case 48000:
                DBG_APP_INFO("LA: Sampling Frequency: 48 MHz \r\n");
                /* Clock GPIF state machine at 240 MHz and interface at 48 MHz. */
                clkSrc       = CY_LVDS_GPIF_CLOCK_USB2;     /* 480 MHz */
                clkDivider   = CY_LVDS_GPIF_CLOCK_DIV_2;    /* 240 MHz */
                clkRegValue  = ((10 << 4) | CY_LVDS_GPIF_CLOCK_USB2);
                break;
            }

            if (channel_mask <= 0xFF) {
                DBG_APP_INFO("LA: Support 8 bit mode\r\n");
            } else {
                DBG_APP_INFO("LA: Support 16 bit mode\r\n");
            }

            Cy_LVDS_GpifRegUpdate((channel_mask > 0xFF), clkRegValue, clkSrc, clkDivider);

            /* Initialize the LVDS interface. */
            Cy_LA_LvdsInit();

            isRunning = true;

            /* Reset the DMA channel through which data is received from the LVDS side. */
            mgrStatus = Cy_HBDma_Channel_Reset(pAppCtxt->hbLADmaChannel);
            ASSERT_NON_BLOCK(CY_HBDMA_MGR_SUCCESS == mgrStatus, mgrStatus);

            mgrStatus = Cy_HBDma_Channel_Enable(pAppCtxt->hbLADmaChannel, 0);
            if (mgrStatus != CY_HBDMA_MGR_SUCCESS) {
                DBG_APP_ERR(" LA: DMA channel Enable failed %x\r\n", mgrStatus);
            }
        } else {
            LOG_COLOR("LA: App running %d \r\n", isRunning);
            if (isRunning == true) {
                Cy_LA_AppStop(pAppCtxt, pAppCtxt->pUsbdCtxt, LOGIC_ANALYZER_BULK_IN_ENDPOINT_1);
            }
            isRunning = false;
        }
    }
    else
    {
        DBG_APP_ERR("USB: Error RecvEndp0Data\r\n");
    }
}

/**
 * \name Cy_LA_AppTaskHandler
 * \brief Stop the data stream channels
 * \param pTaskParam Task parameter
 * \retval None
 */
void Cy_LA_AppTaskHandler(void *pTaskParam)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t xStatus;
    uint32_t lpEntryTime = 0;

    /* If VBus is present, enable the USB connection. */
    pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);
    if (pAppCtxt->vbusPresent) {
        Cy_USB_EnableUsbHSConnection(pAppCtxt);
    }

    for (;;)
    {

        /*
         * If the link has been in USB2-L1 for more than 0.5 seconds, initiate LPM exit so that
         * transfers do not get delayed significantly.
         */
        if ((MXS40USBHSDEV_USBHSDEV->DEV_PWR_CS & USBHSDEV_DEV_PWR_CS_L1_SLEEP) != 0)
        {
            if ((Cy_USBD_GetTimerTick() - lpEntryTime) >= 500UL) {
                lpEntryTime = Cy_USBD_GetTimerTick();
                Cy_USBD_GetUSBLinkActive(pAppCtxt->pUsbdCtxt);
            }
        } else {
            lpEntryTime = Cy_USBD_GetTimerTick();
        }

        /*
         * Wait until some data is received from the queue.
         * Timeout after 100 ms.
         */
        xStatus = xQueueReceive(pAppCtxt->usbMsgQueue, &queueMsg, 100);
        if (xStatus != pdPASS) {
            continue;
        }

        switch (queueMsg.type) {

            case CY_USB_UVC_VBUS_CHANGE_INTR:
                /* Start the debounce timer. */
                xTimerStart(pAppCtxt->vbusDebounceTimer, 0);
                break;

            case CY_USB_UVC_VBUS_CHANGE_DEBOUNCED:
                /* Check whether VBus state has changed. */
                pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);

                if (pAppCtxt->vbusPresent) {
                    if (!pAppCtxt->usbConnected) {
                        DBG_APP_INFO("USB: Enabling USB connection due to VBus detect\r\n");
                        Cy_USB_EnableUsbHSConnection(pAppCtxt);
                    }
                } else {
                    if (pAppCtxt->usbConnected) {
                        DBG_APP_TRACE("USB: HBDMA destroy\r\n");
                        if (pAppCtxt->hbLADmaChannel != NULL)
                        {
                            Cy_HBDma_Channel_Disable(pAppCtxt->hbLADmaChannel);
                            Cy_HBDma_Channel_Destroy(pAppCtxt->hbLADmaChannel);
                            pAppCtxt->hbLADmaChannel = NULL;
                        }
                        if (pAppCtxt->hbTMCCommandChannel != NULL)
                        {
                            Cy_HBDma_Channel_Disable(pAppCtxt->hbTMCCommandChannel);
                            Cy_HBDma_Channel_Destroy(pAppCtxt->hbTMCCommandChannel);
                            pAppCtxt->hbTMCCommandChannel = NULL;
                        }
                        if (pAppCtxt->hbTMCResponseChannel != NULL)
                        {
                            Cy_HBDma_Channel_Disable(pAppCtxt->hbTMCResponseChannel);
                            Cy_HBDma_Channel_Destroy(pAppCtxt->hbTMCResponseChannel);
                            pAppCtxt->hbTMCResponseChannel = NULL;
                        }
                        if (pAppCtxt->hbTMCNotifyChannel != NULL)
                        {
                            Cy_HBDma_Channel_Disable(pAppCtxt->hbTMCNotifyChannel);
                            Cy_HBDma_Channel_Destroy(pAppCtxt->hbTMCNotifyChannel);
                            pAppCtxt->hbTMCNotifyChannel = NULL;
                        }
                    }

                    DBG_APP_INFO("USB: Disabling USB connection due to VBus removal\r\n");
                    Cy_USB_DisableUsbHSConnection(pAppCtxt);
                }
                break;

            case CY_USB_STREAMING_START:
                DBG_APP_INFO("LA: Start stream event for ep %x\r\n", (uint8_t)queueMsg.data[0]);
                if((uint8_t)(queueMsg.data[0]) == LOGIC_ANALYZER_BULK_IN_ENDPOINT_1)
                {
                    Cy_LA_StreamHandler(pAppCtxt, queueMsg.data[1], true);
                }
            break;

            case CY_USB_STREAMING_STOP:
                DBG_APP_INFO("LA: Start stop event for ep %x\r\n", (uint8_t)queueMsg.data[0]);
                 if((uint8_t)(queueMsg.data[0]) == LOGIC_ANALYZER_BULK_IN_ENDPOINT_1)
                {
                    Cy_LA_StreamHandler(pAppCtxt, queueMsg.data[1], false);
                }
            break;

#if USB_TMC_EN
            case USB_TMC_APP_MSG_OUT_RCVD_EVENT:
            DBG_APP_TRACE("TMC: Message of %d bytes received\r\n", queueMsg.data[1]);
            Cy_TMC_Class_MsgOutDmaHandler(pAppCtxt, (void *)queueMsg.data[0], queueMsg.data[1]);
            break;
#endif /* USB_TMC_EN */

            default:
            break;
        }


    } /* End of for(;;) */
}

/**
 * \name Cy_USB_VbusDebounceTimerCallback
 * \brief Timer used to do debounce on VBus changed interrupt notification.
 * \param xTimer Timer Handle
 * \retval None
 */
void
Cy_USB_VbusDebounceTimerCallback (TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    DBG_APP_INFO("USB: VBUS Timer CB\r\n");
    if (pAppCtxt->vbusChangeIntr) {
        /* Notify the VCOM task that VBus debounce is complete. */
        xMsg.type = CY_USB_UVC_VBUS_CHANGE_DEBOUNCED;
        xQueueSendFromISR(pAppCtxt->usbMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

        /* Clear and re-enable the interrupt. */
        pAppCtxt->vbusChangeIntr = false;
        Cy_GPIO_ClearInterrupt(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN);
        Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 1);
    }
}   /* end of function  */

/**
 * \name Cy_USB_AppInit
 * \brief   This function Initializes application related data structures, register callback
 *          creates task for device function.
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD layer Context pointer
 * \param pCpuDmacBase DMAC base address
 * \param pCpuDw0Base DataWire 0 base address
 * \param pCpuDw1Base DataWire 1 base address
 * \param pHbDmaMgrCtxt HBDMA Manager Context
 * \retval None
 */
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                    DMAC_Type *pCpuDmacBase, DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
                    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
{
    uint32_t index;
    BaseType_t status = pdFALSE;
    cy_stc_app_endp_dma_set_t *pEndpInDma;
    cy_stc_app_endp_dma_set_t *pEndpOutDma;

    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->devSpeed = CY_USBD_USB_DEV_FS;
    pAppCtxt->devAddr = 0x00;
    pAppCtxt->activeCfgNum = 0x00;
    pAppCtxt->prevAltSetting = 0x00;
    pAppCtxt->pHbDmaMgrCtxt = pHbDmaMgrCtxt;
    pAppCtxt->pCpuDmacBase = pCpuDmacBase;
    pAppCtxt->pCpuDw0Base = pCpuDw0Base;
    pAppCtxt->pCpuDw1Base = pCpuDw1Base;
    pAppCtxt->pUsbdCtxt = pUsbdCtxt;

    for (index = 0x00; index < CY_USB_MAX_ENDP_NUMBER; index++)
    {
        pEndpInDma = &(pAppCtxt->endpInDma[index]);
        memset((void *)pEndpInDma, 0, sizeof(cy_stc_app_endp_dma_set_t));

        pEndpOutDma = &(pAppCtxt->endpOutDma[index]);
        memset((void *)pEndpOutDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
    }

    /*
     * Callbacks registered with USBD layer. These callbacks will be called
     * based on appropriate event.
     */
    Cy_USB_AppRegisterCallback(pAppCtxt);

    if (!(pAppCtxt->firstInitDone))
    {

        /* Create the message queue and register it with the kernel. */
        pAppCtxt->usbMsgQueue = xQueueCreate(CY_USB_DEVICE_MSG_QUEUE_SIZE,
                CY_USB_DEVICE_MSG_SIZE);
        if (pAppCtxt->usbMsgQueue == NULL) {
            DBG_APP_ERR("LA: Queue create failed\r\n");
            return;
        }

        vQueueAddToRegistry(pAppCtxt->usbMsgQueue, "DeviceMsgQueue");
        /* Create task and check status to confirm task created properly. */
        status = xTaskCreate(Cy_LA_AppTaskHandler, "LogicAnalyzer_Task", 2048,
                             (void *)pAppCtxt, 5, &(pAppCtxt->laTaskHandle));

        if (status != pdPASS)
        {
            DBG_APP_ERR("LA: Task create failed \r\n");
            return;
        }

        pAppCtxt->vbusDebounceTimer = xTimerCreate("VbusDebounceTimer", 200, pdFALSE,
                (void *)pAppCtxt, Cy_USB_VbusDebounceTimerCallback);
        if (pAppCtxt->vbusDebounceTimer == NULL) {
            DBG_APP_ERR("USB: Timer create failed\r\n");
            return;
        }
        DBG_APP_INFO("VBus debounce timer created\r\n");

#if USB_TMC_EN
        /* Allocate DMA Memory to OUT and IN Msg for TMC */
        uint8_t bIdx;

        pAppCtxt->curTmcBufIndex = 0;
        for (bIdx = 0; bIdx < LA_TX_MAX_BUFFER_COUNT; bIdx++)
        {
            g_MsgOutDmaBuffer[bIdx] = (uint8_t *)Cy_HBDma_BufMgr_Alloc(pHbDmaMgrCtxt->pBufMgr, 0x400);
            ASSERT((g_MsgOutDmaBuffer != NULL), (uint32_t)g_MsgOutDmaBuffer);
        }

        g_MsgInDmaBuffer = (uint8_t *)Cy_HBDma_BufMgr_Alloc(pHbDmaMgrCtxt->pBufMgr, 0x400);
        ASSERT((g_MsgInDmaBuffer != NULL), (uint32_t)g_MsgInDmaBuffer);
#endif /* USB_TMC_EN */

        pAppCtxt->firstInitDone = 0x01;
    }
} /* end of function. */

/**
 * \name Cy_USB_AppRegisterCallback
 * \brief This function will register all calback with USBD layer.
 * \param pAppCtxt application layer context pointer.
 * \retval None
 */
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;

    /* Register relevant callbacks with USB stack */
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET, Cy_USB_AppBusResetCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET_DONE, Cy_USB_AppBusResetDoneCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_BUS_SPEED, Cy_USB_AppBusSpeedCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETUP, Cy_USB_AppSetupCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SUSPEND, Cy_USB_AppSuspendCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESUME, Cy_USB_AppResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_CONFIG, Cy_USB_AppSetCfgCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_INTF, Cy_USB_AppSetIntfCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SLP, Cy_USB_AppSlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_ZLP, Cy_USB_AppZlpCallback);
}

/**
 * \name Cy_USB_AppZlpCallback
 * \brief This Function will be called by USBD layer when ZLP message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppZlpCallback (void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /* Notify the DMA manager about short packet received. */
    if (pMsg->type == CY_USB_CAL_MSG_OUT_ZLP)
    {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgrCtxt, (pMsg->data[0] & 0x7FU), 0);
    }
} /* end of function. */

/**
 * \name Cy_USB_AppSlpCallback
 * \brief This Function will be called by USBD layer when SLP message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSlpCallback (void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /* Notify the DMA manager about short packet received. */
    if (pMsg->type == CY_USB_CAL_MSG_OUT_SLP)
    {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgrCtxt, (pMsg->data[0] & 0x7FU), pMsg->data[1]);
    }
} /* end of function. */

#if USB_TMC_EN
/**
 * \name Cy_LA_AppHbDmaTxCallback
 * \brief Callback function for the channel transmitting data out of LVDS socket
 * \param handle
 * \param type
 * \param pbufStat
 * \param userCtx
 * \retval None
 */
void Cy_LA_AppHbDmaTxCallback(
        cy_stc_hbdma_channel_t *handle,
        cy_en_hbdma_cb_type_t type,
        cy_stc_hbdma_buff_status_t *pbufStat,
        void *userCtx)
{
    cy_en_hbdma_mgr_status_t   status = CY_HBDMA_MGR_SUCCESS;
    cy_stc_hbdma_buff_status_t buffStat;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    if (type == CY_HBDMA_CB_PROD_EVENT)
    {
        status = Cy_HBDma_Channel_GetBuffer(handle, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("LA: HB-DMA GetBuffer Error\r\n");
            return;
        }

        /* Copy the data received into a local buffer and mark the DMA buffer discarded. */
        memcpy(g_MsgOutDmaBuffer[pAppCtxt->curTmcBufIndex], (uint8_t *)buffStat.pBuffer, buffStat.count);

        status = Cy_HBDma_Channel_DiscardBuffer(handle, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("LA: HB-DMA GetBuffer Error\r\n");
            return;
        }

        /* Post msg to Task Handler */
        xMsg.type    = USB_TMC_APP_MSG_OUT_RCVD_EVENT;
        xMsg.data[0] = (uint32_t)(g_MsgOutDmaBuffer[pAppCtxt->curTmcBufIndex]);
        xMsg.data[1] = buffStat.count;
        xQueueSendFromISR(pAppCtxt->usbMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

        pAppCtxt->curTmcBufIndex++;
        if (pAppCtxt->curTmcBufIndex >= LA_TX_MAX_BUFFER_COUNT)
        {
            pAppCtxt->curTmcBufIndex = 0;
        }
    }
}
#endif /* USB_TMC_EN */

/**
 * \name Cy_LA_AppHbDmaIntrCallback
 * \brief Callback function for the interrupt ep channel
 * \param handle
 * \param type
 * \param pbufStat
 * \param userCtx
 * \retval None
 */
void Cy_LA_AppHbDmaIntrCallback(
        cy_stc_hbdma_channel_t *handle,
        cy_en_hbdma_cb_type_t type,
        cy_stc_hbdma_buff_status_t *pbufStat,
        void *userCtx)
{
}

/**
 * \name Cy_USB_AppSetupEndpDmaParamsHs
 * \brief Configure and enable HBW DMA channels.
 * \param pAppCtxt application layer context pointer.
 * \param pEndpDscr Endpoint descriptor pointer
 * \retval None
 */
static void Cy_USB_AppSetupEndpDmaParamsHs(cy_stc_usb_app_ctxt_t *pUsbApp, uint8_t *pEndpDscr)
{
    cy_stc_hbdma_chn_config_t dmaConfig;
    cy_en_hbdma_mgr_status_t mgrStat;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

#if USB_LOGIC_ANALYZER_EN
    if ((endpNumber == LOGIC_ANALYZER_BULK_IN_ENDPOINT_1) && (dir))
    {
        if (pUsbApp->hbLADmaChannel != NULL)
        {
            /* Destroy the channel and create it afresh. */
            Cy_HBDma_Channel_Disable(pUsbApp->hbLADmaChannel);
            Cy_HBDma_Channel_Destroy(pUsbApp->hbLADmaChannel);
            pUsbApp->hbLADmaChannel = NULL;
        }

        pUsbApp->logicAnalyzerEp    = (uint8_t)endpNumber;
        dmaConfig.size              = LA_RX_MAX_BUFFER_SIZE;        /* DMA Buffer Size in bytes */
        dmaConfig.count             = LA_RX_MAX_BUFFER_COUNT;       /* DMA Buffer Count */
        dmaConfig.bufferMode        = true;                         /* DMA buffer mode disabled */
        dmaConfig.prodHdrSize       = 0;
        dmaConfig.prodBufSize       = LA_RX_MAX_BUFFER_SIZE;
        dmaConfig.eventEnable       = 0;                            /* Enable for DMA AUTO */
        dmaConfig.intrEnable        = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk |
                                      LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;

        dmaConfig.consSckCount      = 1;                            /* No. of consumer Sockets */
        dmaConfig.consSck[0]        = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
        dmaConfig.consSck[1]        = (cy_hbdma_socket_id_t)0;      /* Consumer Socket ID: None */

        dmaConfig.prodSckCount      = 2;                            /* No. of producer sockets */
        dmaConfig.prodSck[0]        = CY_HBDMA_LVDS_SOCKET_00;
        dmaConfig.prodSck[1]        = CY_HBDMA_LVDS_SOCKET_01;

        dmaConfig.cb                = Cy_LA_AppHbDmaRxCallback;     /* HB-DMA callback */
        dmaConfig.userCtx           = (void *)(pUsbApp);            /* Pass the application context as user context. */

        dmaConfig.chType            = CY_HBDMA_TYPE_IP_TO_IP;
        dmaConfig.usbMaxPktSize     = maxPktSize;

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                                          &(pUsbApp->endpInDma[endpNumber].hbDmaChannel),
                                          &dmaConfig);

        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("LA: DMA channel create failed 0x %x\r\n", mgrStat);
            return;
        }
        else
        {
            /* Store the DMA channel pointer. */
            pUsbApp->hbLADmaChannel = &(pUsbApp->endpInDma[endpNumber].hbDmaChannel);
        }
    }
#endif /* USB_LOGIC_ANALYZER_EN */

#if USB_TMC_EN
    if ((endpNumber == TMC_MSG_IN_ENDPOINT)  && (dir)) {
        if (pUsbApp->hbTMCResponseChannel != NULL)
        {
            Cy_HBDma_Channel_Disable(pUsbApp->hbTMCResponseChannel);
            Cy_HBDma_Channel_Destroy(pUsbApp->hbTMCResponseChannel);
            pUsbApp->hbTMCResponseChannel = NULL;
        }

        pUsbApp->tmcResponseEp      = (uint8_t)endpNumber;

        dmaConfig.chType            = CY_HBDMA_TYPE_MEM_TO_IP;
        dmaConfig.size              = 0x400;                        /* DMA Buffer Size in bytes */
        dmaConfig.prodBufSize       = 0x400;
        dmaConfig.prodHdrSize       = 0;
        dmaConfig.count             = LA_RX_MAX_BUFFER_COUNT;       /* DMA Buffer Count */
        dmaConfig.bufferMode        = true;                         /* DMA buffer mode disabled */
        dmaConfig.eventEnable       = 0;                            /* Enable for DMA AUTO */
        dmaConfig.intrEnable        = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
        dmaConfig.usbMaxPktSize     = maxPktSize;

        dmaConfig.prodSckCount      = 1;                            /* No. of producer sockets */
        dmaConfig.prodSck[0]        = CY_HBDMA_VIRT_SOCKET_WR;
        dmaConfig.prodSck[1]        = (cy_hbdma_socket_id_t)0;      /* Producer Socket ID: None */

        dmaConfig.consSckCount      = 1;                            /* No. of consumer Sockets */
        dmaConfig.consSck[0]        = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
        dmaConfig.consSck[1]        = (cy_hbdma_socket_id_t)0;      /* Consumer Socket ID: None */

        dmaConfig.cb                = Cy_LA_AppHbDmaRxCallback;     /* HB-DMA callback */
        dmaConfig.userCtx           = (void *)(pUsbApp);            /* Pass the application context as user context. */

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                                          &(pUsbApp->endpInDma[endpNumber].hbDmaChannel),
                                          &dmaConfig);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("TMC: Message IN channel create failed 0x%x\r\n", mgrStat);
            return;
        }
        else
        {
            /* Store the DMA channel pointer. */
            pUsbApp->hbTMCResponseChannel = &(pUsbApp->endpInDma[endpNumber].hbDmaChannel);

            mgrStat = Cy_HBDma_Channel_Enable(pUsbApp->hbTMCResponseChannel, 0);
            DBG_APP_INFO("TMC: Message IN channel create status  %x\r\n", mgrStat);
        }
    }

#if TMC_SUPPORT_INTERRUPT
    if ((endpNumber == TMC_MSG_INTR_ENDPOINT)  && (dir)) {
        if (pUsbApp->hbTMCNotifyChannel != NULL)
        {
            Cy_HBDma_Channel_Disable(pUsbApp->hbTMCNotifyChannel);
            Cy_HBDma_Channel_Destroy(pUsbApp->hbTMCNotifyChannel);
            pUsbApp->hbTMCNotifyChannel = NULL;
        }

        pUsbApp->tmcNotifyEp   = (uint8_t)endpNumber;

        dmaConfig.chType        = CY_HBDMA_TYPE_MEM_TO_IP;
        dmaConfig.size          = maxPktSize;                   /* DMA Buffer Size in bytes */
        dmaConfig.prodBufSize   = maxPktSize;
        dmaConfig.prodHdrSize   = 0;
        dmaConfig.count         = LA_RX_MAX_BUFFER_COUNT;       /* DMA Buffer Count */
        dmaConfig.bufferMode    = true;                         /* DMA buffer mode disabled */
        dmaConfig.usbMaxPktSize = maxPktSize;
        dmaConfig.eventEnable   = 0;                            /* Enable for DMA AUTO */
        dmaConfig.intrEnable    = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk |
                                  LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
        dmaConfig.prodSckCount  = 1;                            /* No. of producer sockets */
        dmaConfig.prodSck[0]    = CY_HBDMA_VIRT_SOCKET_WR;
        dmaConfig.prodSck[1]    = (cy_hbdma_socket_id_t)0;      /* Producer Socket ID: None */
        dmaConfig.consSckCount  = 1;                            /* No. of consumer Sockets */
        dmaConfig.consSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
        dmaConfig.consSck[1]    = (cy_hbdma_socket_id_t)0;      /* Consumer Socket ID: None */

        dmaConfig.cb            = Cy_LA_AppHbDmaIntrCallback;   /* HB-DMA callback */
        dmaConfig.userCtx       = (void *)(pUsbApp);            /* Pass the application context as user context */

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                                          &pUsbApp->endpInDma[endpNumber].hbDmaChannel,
                                          &dmaConfig);

        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("TMC: Message INTR channel create failed 0x%x\r\n", mgrStat);
            return;
        }
        else
        {
            /* Store the DMA channel pointer. */
            pUsbApp->hbTMCNotifyChannel = &(pUsbApp->endpInDma[endpNumber].hbDmaChannel);

            mgrStat = Cy_HBDma_Channel_Enable(pUsbApp->hbTMCNotifyChannel, 0);
            DBG_APP_INFO("TMC: Message INTR channel create status: %x\r\n", mgrStat);
        }
    }
#endif  /* TMC_SUPPORT_INTERRUPT */

    /* TMC_MSG_OUT_ENDPOINT setup the DW DMA */
    if ((endpNumber == TMC_MSG_OUT_ENDPOINT) && (dir == 0)) {
        if (pUsbApp->hbTMCCommandChannel != NULL)
        {
            Cy_HBDma_Channel_Disable(pUsbApp->hbTMCCommandChannel);
            Cy_HBDma_Channel_Destroy(pUsbApp->hbTMCCommandChannel);
            pUsbApp->hbTMCCommandChannel = NULL;
        }

        pUsbApp->tmcCommandEp      = (uint8_t)endpNumber;

        dmaConfig.chType        = CY_HBDMA_TYPE_IP_TO_MEM;
        dmaConfig.size          = 0x400;                        /* DMA Buffer Size in bytes */
        dmaConfig.prodBufSize   = 0x400;
        dmaConfig.prodHdrSize   = 0;
        dmaConfig.count         = LA_TX_MAX_BUFFER_COUNT;       /* DMA Buffer Count */
        dmaConfig.bufferMode    = true;                         /* DMA buffer mode disabled */
        dmaConfig.usbMaxPktSize = maxPktSize;

        dmaConfig.eventEnable   = 0;                            /* Enable for DMA AUTO */
        dmaConfig.intrEnable    = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk;

        dmaConfig.prodSckCount  = 1;                            /* No. of producer sockets */
        dmaConfig.prodSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_OUT_EP_00 + endpNumber);
        dmaConfig.prodSck[1]    = (cy_hbdma_socket_id_t)0;      /* Producer Socket ID: None */
        dmaConfig.consSckCount  = 1;                            /* No. of consumer Sockets */
        dmaConfig.consSck[0]    = CY_HBDMA_VIRT_SOCKET_RD;
        dmaConfig.consSck[1]    = (cy_hbdma_socket_id_t)0;      /* Consumer Socket ID: None */

        dmaConfig.cb            = Cy_LA_AppHbDmaTxCallback;     /* HB-DMA callback */
        dmaConfig.userCtx       = (void *)(pUsbApp);            /* Pass the application context as user context */

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                                          &pUsbApp->endpOutDma[endpNumber].hbDmaChannel,
                                          &dmaConfig);

        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("TMC: Message OUT channel create failed 0x%x\r\n", mgrStat);
            return;
        }
        else
        {
            /* Store the DMA channel pointer. */
            pUsbApp->hbTMCCommandChannel = &(pUsbApp->endpOutDma[endpNumber].hbDmaChannel);
            pUsbApp->curTmcBufIndex = 0;

            mgrStat = Cy_HBDma_Channel_Enable(pUsbApp->hbTMCCommandChannel, 0);
            DBG_APP_INFO("TMC: Message OUT channel create status: %x\r\n", mgrStat);
        }
    }
#endif /* USB_TMC_EN */

    return;
} /* end of function  */

/**
 * \name Cy_USB_AppConfigureEndp
 * \brief Configure all endpoints used by application (except EP0)
 * \param pUsbdCtxt USBD layer context pointer
 * \param pEndpDscr Endpoint descriptor pointer
 * \retval None
 */
void Cy_USB_AppConfigureEndp(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr)
{
    cy_stc_usb_endp_config_t endpConfig;
    cy_en_usb_endp_dir_t endpDirection;
    bool valid;
    uint32_t endpType;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint32_t isoPkts = 0x00;
    uint8_t burstSize = 0x00;
    uint8_t maxStream = 0x00;
    uint8_t interval = 0x00;
    cy_en_usbd_ret_code_t usbdRetCode;

    /* If it is not endpoint descriptor then return */
    if (!Cy_USBD_EndpDscrValid(pEndpDscr))
    {
        return;
    }

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

    if (dir)
    {
        endpDirection = CY_USB_ENDP_DIR_IN;
    }
    else
    {
        endpDirection = CY_USB_ENDP_DIR_OUT;
    }
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    if ((CY_USB_ENDP_TYPE_ISO == endpType) || (CY_USB_ENDP_TYPE_INTR == endpType))
    {
        /* The ISOINPKS setting in the USBHS register is the actual packets per microframe value. */
        isoPkts = ((*((uint8_t *)(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_MAX_PKT + 1)) & CY_USB_ENDP_ADDL_XN_MASK) >> CY_USB_ENDP_ADDL_XN_POS) + 1;
    }

    valid = 0x01;
    Cy_USBD_GetEndpInterval(pEndpDscr, &interval);

    /* Prepare endpointConfig parameter. */
    endpConfig.endpType = (cy_en_usb_endp_type_t)endpType;
    endpConfig.endpDirection = endpDirection;
    endpConfig.valid = valid;
    endpConfig.endpNumber = endpNumber;
    endpConfig.maxPktSize = (uint32_t)maxPktSize;
    endpConfig.isoPkts = isoPkts;
    endpConfig.burstSize = burstSize;
    endpConfig.streamID = maxStream;
    endpConfig.interval = interval;
    endpConfig.allowNakTillDmaRdy = true;
    usbdRetCode = Cy_USB_USBD_EndpConfig(pUsbdCtxt, endpConfig);

    /* Print status of the endpoint configuration to help debug. */
    DBG_APP_INFO("USB: Ep Number %d, status %x\r\n", endpNumber, usbdRetCode);
    return;
} /* end of function */

/**
 * \name Cy_USB_AppSetCfgCallback
 * \brief Callback function will be invoked by USBD when set configuration is received
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD layer context pointer.
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetCfgCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    uint8_t *pActiveCfg, *pIntfDscr, *pEndpDscr;
    uint8_t index, numOfIntf, numOfEndp;

    DBG_APP_INFO("USB: Set Configuration CB\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    /*
     * Based on type of application as well as how data flows,
     * data wire can be used so initialize datawire.
     */
    Cy_DMA_Enable(pUsbApp->pCpuDw0Base);
    Cy_DMA_Enable(pUsbApp->pCpuDw1Base);

    pActiveCfg = Cy_USB_USBD_GetActiveCfgDscr(pUsbdCtxt);
    if (!pActiveCfg)
    {
        /* Set config should be called when active config value > 0x00. */
        return;
    }
    numOfIntf = Cy_USBD_FindNumOfIntf(pActiveCfg);
    if (numOfIntf == 0x00)
    {
        return;
    }

    for (index = 0x00; index < numOfIntf; index++)
    {
        /* During Set Config command always altSetting 0 will be active. */
        pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, index, 0x00);
        if (pIntfDscr == NULL)
        {
            DBG_APP_INFO("USB: Get Intf failed \r\n");
            return;
        }

        numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
        if (numOfEndp == 0x00)
        {
            DBG_APP_INFO("USB: No. of ep: 0\r\n");
            continue;
        }

        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParamsHs(pAppCtxt, pEndpDscr);
            numOfEndp--;
            pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));

        }
    }

#if USB_LOGIC_ANALYZER_EN
    Cy_USB_AppInitDmaIntr(LOGIC_ANALYZER_BULK_IN_ENDPOINT_1, CY_USB_ENDP_DIR_IN, Cy_App_UsbInISR);
#endif /* USB_LOGIC_ANALYZER_EN */

#if USB_TMC_EN
    Cy_USB_AppInitDmaIntr(TMC_MSG_INTR_ENDPOINT, CY_USB_ENDP_DIR_IN, Cy_App_UsbInISR);
    Cy_USB_AppInitDmaIntr(TMC_MSG_IN_ENDPOINT, CY_USB_ENDP_DIR_IN, Cy_App_UsbInISR);
    Cy_USB_AppInitDmaIntr(TMC_MSG_OUT_ENDPOINT, CY_USB_ENDP_DIR_OUT, Cy_App_UsbOutISR);
#endif /* USB_TMC_EN */

    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
    pUsbApp->devState = CY_USB_DEVICE_STATE_CONFIGURED;

    /* Enable data stream */
    if(pUsbApp->logicAnalyzerEp == LOGIC_ANALYZER_BULK_IN_ENDPOINT_1)
    {
        DBG_APP_INFO("Enable Device - 0\r\n");
    }

#if USB_TMC_EN
    Cy_TMC_AppStart(pAppCtxt, pUsbdCtxt);
#endif /* USB_TMC_EN */

    Cy_USBD_LpmDisable(pUsbdCtxt);

    return;
} /* end of function */

/**
 * \name Cy_USB_AppBusResetCallback
 * \brief Callback function will be invoked by USBD when bus detects RESET
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    DBG_APP_INFO("USB: Bus Reset CB\r\n");

    /* Stop and destroy the high bandwidth DMA channels when present. */

#if USB_LOGIC_ANALYZER_EN
    if (pUsbApp->hbLADmaChannel != NULL)
    {
        Cy_HBDma_Channel_Disable(pUsbApp->hbLADmaChannel);
        Cy_HBDma_Channel_Destroy(pUsbApp->hbLADmaChannel);
        pUsbApp->hbLADmaChannel = NULL;
    }
#endif /* USB_LOGIC_ANALYZER_EN */

#if USB_TMC_EN
    if (pUsbApp->hbTMCCommandChannel != NULL)
    {
        Cy_HBDma_Channel_Disable(pUsbApp->hbTMCCommandChannel);
        Cy_HBDma_Channel_Destroy(pUsbApp->hbTMCCommandChannel);
        pUsbApp->hbTMCCommandChannel = NULL;
    }

    if (pUsbApp->hbTMCResponseChannel != NULL)
    {
        Cy_HBDma_Channel_Disable(pUsbApp->hbTMCResponseChannel);
        Cy_HBDma_Channel_Destroy(pUsbApp->hbTMCResponseChannel);
        pUsbApp->hbTMCResponseChannel = NULL;
    }

    if (pUsbApp->hbTMCNotifyChannel != NULL)
    {
        Cy_HBDma_Channel_Disable(pUsbApp->hbTMCNotifyChannel);
        Cy_HBDma_Channel_Destroy(pUsbApp->hbTMCNotifyChannel);
        pUsbApp->hbTMCNotifyChannel = NULL;
    }
#endif /* USB_TMC_EN */

    /*
     * USBD layer takes care of reseting its own data structure as well as
     * takes care of calling CAL reset APIs. Application needs to take care
     * of reseting its own data structure as well as "device function".
     */
    Cy_USB_AppInit(pUsbApp, pUsbdCtxt, pUsbApp->pCpuDmacBase, pUsbApp->pCpuDw0Base, pUsbApp->pCpuDw1Base,
            pUsbApp->pHbDmaMgrCtxt);
    pUsbApp->devState = CY_USB_DEVICE_STATE_RESET;
    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_RESET;

    return;
} /* end of function. */

/**
 * \name Cy_USB_AppBusResetDoneCallback
 * \brief Callback function will be invoked by USBD when RESET is completed
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    DBG_APP_INFO("USB: Bus Reset Done CB\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->prevDevState = pUsbApp->devState;
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppBusSpeedCallback
 * \brief   Callback function will be invoked by USBD when speed is identified or
 *          speed change is detected
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppSetupCallback
 * \brief Callback function will be invoked by USBD when SETUP packet is received
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                             cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    uint8_t bRequest, bReqType;
    uint8_t bType, bTarget;
    uint16_t wValue, wIndex, wLength;
    bool isReqHandled = false;
    cy_stc_usbd_app_msg_t xMsg;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;
    cy_en_usb_endp_dir_t epDir = CY_USB_ENDP_DIR_INVALID;
    BaseType_t status = 0;

#if USB_LOGIC_ANALYZER_EN
    uint8_t version[] = {1,0};
#endif

    DBG_APP_TRACE("USB: Setup CB\r\n");

    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function. */

    /* Decode the fields from the setup request. */
    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bType = ((bReqType & CY_USB_CTRL_REQ_TYPE_MASK) >> CY_USB_CTRL_REQ_TYPE_POS);
    bTarget = (bReqType & CY_USB_CTRL_REQ_RECIPENT_OTHERS);
    bRequest = pUsbdCtxt->setupReq.bRequest;
    wValue = pUsbdCtxt->setupReq.wValue;
    wIndex = pUsbdCtxt->setupReq.wIndex;
    wLength = pUsbdCtxt->setupReq.wLength;

#if USB_LOGIC_ANALYZER_EN
    if (bType == CY_USB_CTRL_REQ_VENDOR){
        switch (bRequest){
        case CMD_START:
            DBG_APP_INFO("CMD_START\r\n");
            glIsStartReceived = true;

            /* Start */
            xMsg.type = CY_USB_STREAMING_START;
            xMsg.data[0] = LOGIC_ANALYZER_BULK_IN_ENDPOINT_1;
            xMsg.data[1] = wLength;
            xQueueSendFromISR(pUsbApp->usbMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
            isReqHandled = true;
            break;

        case CMD_STOP:
            DBG_APP_INFO("CMD_STOP\r\n");
            glIsStartReceived = false;

            /* Stop */
            xMsg.type = CY_USB_STREAMING_STOP;
            xMsg.data[0] = LOGIC_ANALYZER_BULK_IN_ENDPOINT_1;
            xMsg.data[1] = wLength;
            xQueueSendFromISR(pUsbApp->usbMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
            isReqHandled = true;
            break;

        case CMD_GET_FW_VERSION:
            DBG_APP_INFO("Version, wLength = %d\r\n",wLength);
            Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,version,2);
            isReqHandled = true;
            break;

        case CMD_GET_REVID_VERSION:
            DBG_APP_INFO("Revision, wLength = %d\r\n",wLength);
            Cy_USB_USBD_SendEndp0Data(pUsbdCtxt,version,1);
            isReqHandled = true;
            break;
        }
    }

    if (isReqHandled == true) {
        return;
    }
#endif /* USB_LOGIC_ANALYZER_EN */

    if (bType == CY_USB_CTRL_REQ_STD)
    {
        DBG_APP_TRACE("USB: Standard request\r\n");

        if (bRequest == CY_USB_SC_SET_FEATURE)
        {
            DBG_APP_INFO("USB: Set Feature request\r\n");
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == 0))
            {
                DBG_APP_TRACE("USB: Set Feature request: Target-interface\r\n");
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);

                isReqHandled = true;
            }

            /* SET-FEATURE(EP-HALT) is only supported to facilitate Chapter 9 compliance tests. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (wValue == CY_USB_FEATURE_ENDP_HALT))
            {
                DBG_APP_TRACE("USB: Set Feature request : Target-endpoint\r\n");
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL),
                        epDir, true);

                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }

        if (bRequest == CY_USB_SC_CLEAR_FEATURE)
        {
            DBG_APP_INFO("USB: Clear Feature request\r\n");
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == 0))
            {
                DBG_APP_TRACE("USB: Clear Feature request: Target-interface\r\n");

                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);

                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (wValue == CY_USB_FEATURE_ENDP_HALT))
            {
                DBG_APP_TRACE("USB: Clear Feature request: Target-endpoint\r\n");
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
                if(((wIndex & 0x7F) == pUsbApp->logicAnalyzerEp))
                {
                    /* Stop data stream. */
                    xMsg.type = CY_USB_STREAMING_STOP;
                    xMsg.data[0] = pUsbApp->logicAnalyzerEp;
                    status = xQueueSendFromISR(pUsbApp->usbMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                    ASSERT_NON_BLOCK(pdTRUE == status,status);

                }

                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;

#if (USB_TMC_EN)
                if ((glIsTMCApplnActive) &&
                        (
                         (wIndex == TMC_MSG_OUT_ENDPOINT) ||
#if TMC_SUPPORT_INTERRUPT
                         (wIndex == (TMC_MSG_INTR_ENDPOINT | 0x80)) ||
#endif
                         (wIndex == (TMC_MSG_IN_ENDPOINT | 0x80))
                        )
                   ) {
                    Cy_USBTMCAppHaltEndpoint (pUsbApp, pUsbdCtxt,wIndex);
                    isReqHandled = true;
                }
#endif /* USB_TMC_EN */
            }
        }

        /* Handle Microsoft OS String Descriptor request. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                (bRequest == CY_USB_SC_GET_DESCRIPTOR) &&
                (wValue == ((CY_USB_STRING_DSCR << 8) | 0xEE))) {

            /* Make sure we do not send more data than requested. */
            if (wLength > glOsString[0]) {
                wLength = glOsString[0];
            }

            DBG_APP_INFO("USB: OS String\r\n");
            retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)glOsString, wLength);
            if(retStatus == CY_USBD_STATUS_SUCCESS) {
                isReqHandled = true;
            }
        }
    }

#if (USB_LOGIC_ANALYZER_EN)
    if (bType == CY_USB_CTRL_REQ_VENDOR) {
        /* If trying to bind to WinUSB driver, we need to support additional control requests. */
        /* Handle OS Compatibility and OS Feature requests */

        if (bRequest == MS_VENDOR_CODE) {
            if (wIndex == 0x04) {
                if (wLength > *((uint16_t *)glOsCompatibilityId)) {
                    wLength = *((uint16_t *)glOsCompatibilityId);
                }

                DBG_APP_INFO("USB: OSCompat\r\n");
                retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)glOsCompatibilityId, wLength);
                if(retStatus == CY_USBD_STATUS_SUCCESS) {
                    isReqHandled = true;
                }
            }
            else if (wIndex == 0x05) {
                if (wLength > *((uint16_t *)glOsFeature)) {
                    wLength = *((uint16_t *)glOsFeature);
                }

                DBG_APP_INFO("USB: OSFeature\r\n");
                retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)glOsFeature, wLength);
                if(retStatus == CY_USBD_STATUS_SUCCESS) {
                    isReqHandled = true;
                }
            }
        }

        if (isReqHandled) {
            return;
        }
    }
#endif

#if (USB_TMC_EN)
    if (bType == CY_USB_CTRL_REQ_CLASS) {
        DBG_APP_TRACE("USB: Class Request \r\n");
        if (bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) {
            if (wIndex == USB_INTF_TMC) {
                /* TMC Specific Requests */
                DBG_APP_TRACE("USB: TMC Class Request \r\n");
                isReqHandled = Cy_TMC_Class_InterfaceRequestHandler(pAppCtxt, pUsbdCtxt, bReqType,
                        (USBTMC_REQ)bRequest, wValue, wLength);
            }
        } else if (bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) {
            if ( 
                    (wIndex == TMC_MSG_OUT_ENDPOINT) ||
#if TMC_SUPPORT_INTERRUPT
                    (wIndex == (TMC_MSG_INTR_ENDPOINT | 0x80)) ||
#endif /* TMC_SUPPORT_INTERRUPT */
                    (wIndex == (TMC_MSG_IN_ENDPOINT | 0x80))
               ) {
                if (bRequest == CY_USB_SC_CLEAR_FEATURE) {
                    DBG_APP_INFO("USB: Clear Fetaure Request \r\n");
                    Cy_USBTMCAppHaltEndpoint(pAppCtxt, pUsbdCtxt, wIndex);
                    isReqHandled = true;
                }
                else {
                    /* TMC Specific Requests */
                    DBG_APP_TRACE("USB: TMC Class Request \r\n");
                    DBG_APP_TRACE("TMC: bType = 0x%x, bTarget= 0x%x, bRequest: 0x%x\r\n", bType, bTarget, bRequest);
                    DBG_APP_TRACE("TMC: wIndex= 0x%x, wValue= 0x%x, wLength= 0x%x \r\n", wIndex, wValue, wLength);
                    isReqHandled = Cy_TMC_Class_EndpointRequestHandler(pAppCtxt, pUsbdCtxt, bReqType,
                            (USBTMC_REQ)bRequest, wValue, wIndex, wLength);
                }
            }
        }
    }
#endif /* USB_TMC_EN */

    /* If Request is not handled by the callback, Stall the command */
    if (!isReqHandled)
    {
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
    }
} /* end of function. */

/**
 * \name Cy_USB_AppSuspendCallback
 * \brief Callback function will be invoked by USBD when Suspend signal/message is detected
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->prevDevState = pUsbApp->devState;
    pUsbApp->devState = CY_USB_DEVICE_STATE_SUSPEND;
} /* end of function. */

/**
 * \name Cy_USB_AppResumeCallback
 * \brief Callback function will be invoked by USBD when Resume signal/message is detected
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    cy_en_usb_device_state_t tempState;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    tempState = pUsbApp->devState;
    pUsbApp->devState = pUsbApp->prevDevState;
    pUsbApp->prevDevState = tempState;
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppSetIntfCallback
 * \brief Callback function will be invoked by USBD when SET_INTERFACE is  received
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetIntfCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_setup_req_t *pSetupReq;
    uint8_t intfNum, altSetting;
    int8_t numOfEndp;
    uint8_t *pIntfDscr, *pEndpDscr;
    uint32_t endpNumber;
    cy_en_usb_endp_dir_t endpDirection;
    cy_stc_usb_app_ctxt_t *pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    DBG_APP_INFO("USB: Set Interface CB\r\n");
    pSetupReq = &(pUsbdCtxt->setupReq);
    /*
     * Get interface and alt setting info. If new setting same as previous
     * then return.
     * If new alt setting came then first Unconfigure previous settings
     * and then configure new settings.
     */
    intfNum = pSetupReq->wIndex;
    altSetting = pSetupReq->wValue;

    if (altSetting == pUsbApp->prevAltSetting)
    {
        DBG_APP_INFO("USB: SameAltSetting\r\n");
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
        return;
    }

    /* New altSetting is different than previous one so unconfigure previous. */
    pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, intfNum, pUsbApp->prevAltSetting);
    DBG_APP_INFO("USB: Unconfig PrevAltSet\r\n");
    if (pIntfDscr == NULL)
    {
        DBG_APP_INFO("USB: pIntfDscrNull\r\n");
        return;
    }
    numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
    if (numOfEndp == 0x00)
    {
        DBG_APP_INFO("USB:prevNumEp 0\r\n");
    }
    else
    {
        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            if (*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS) & 0x80)
            {
                endpDirection = CY_USB_ENDP_DIR_IN;
            }
            else
            {
                endpDirection = CY_USB_ENDP_DIR_OUT;
            }
            endpNumber =
                (uint32_t)((*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS)) & 0x7F);

            /* with FALSE, unconfgure previous settings. */
            Cy_USBD_EnableEndp(pUsbdCtxt, endpNumber, endpDirection, FALSE);

            numOfEndp--;
            pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
        }
    }

    /* Now take care of different config with new alt setting. */
    pUsbApp->prevAltSetting = altSetting;
    pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, intfNum, altSetting);
    if (pIntfDscr == NULL)
    {
        DBG_APP_INFO("USB: pIntfDscrNull\r\n");
        return;
    }

    numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
    if (numOfEndp == 0x00)
    {
        DBG_APP_INFO("USB:numEp 0\r\n");
    }
    else
    {
        pUsbApp->prevAltSetting = altSetting;
        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParamsHs(pAppCtxt, pEndpDscr);
            numOfEndp--;
            pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
        }
    }

    return;
} /* end of function. */

/**
 * \name Cy_USB_AppInitDmaIntr
 * \brief Function to register an ISR for the DMA channel associated with an endpoint
 * \param endpNumber USB endpoint number
 * \param endpDirection Endpoint direction
 * \param userIsr ISR function pointer. Can be NULL if interrupt is to be disabled.
 * \retval None
 */
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection,
                           cy_israddress userIsr)
{
    cy_stc_sysint_t intrCfg;

    if ((endpNumber > 0) && (endpNumber < CY_USB_MAX_ENDP_NUMBER))
    {
#if (!CY_CPU_CORTEX_M4)
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            intrCfg.intrPriority = 3;
            intrCfg.intrSrc = NvicMux1_IRQn;
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            intrCfg.intrPriority = 3;
            intrCfg.intrSrc = NvicMux6_IRQn;
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#else
        intrCfg.intrPriority = 5;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#endif /* (!CY_CPU_CORTEX_M4) */

        if (userIsr != NULL)
        {
            /* If an ISR is provided, register it and enable the interrupt. */
            Cy_SysInt_Init(&intrCfg, userIsr);
            NVIC_EnableIRQ(intrCfg.intrSrc);
        }
        else
        {
            /* ISR is NULL. Disable the interrupt. */
            NVIC_DisableIRQ(intrCfg.intrSrc);
        }
    }
}

/**
 * \name Cy_CheckStatus
 * \brief Function that handles prints error log
 * \param function Pointer to function
 * \param line Line number where error is seen
 * \param condition condition of failure
 * \param value error code
 * \param isBlocking blocking function
 * \retval None
 */
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking)
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/**
 * \name Cy_CheckStatusHandleFailure
 * \brief Function that handles prints error log
 * \param function Pointer to function
 * \param line LineNumber where error is seen
 * \param condition Line number where error is seen
 * \param value error code
 * \param isBlocking blocking function
 * \param failureHandler failure handler function
 * \retval None
 */
void Cy_CheckStatusHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)(void))
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);

        if(failureHandler != NULL)
        {
            (*failureHandler)();
        }
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/**
 * \name Cy_USB_FailHandler
 * \brief Error Handler
 * \retval None
 */
void Cy_FailHandler(void)
{
    DBG_APP_ERR("Reset Done\r\n");
}

/* [] END OF FILE */
