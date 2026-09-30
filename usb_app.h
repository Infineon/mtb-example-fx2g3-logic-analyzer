/***************************************************************************//**
* \file usb_app.h
* \version 1.0
*
* \brief Header file providing declarations and definitions for the USB
*        Logic Analyzer application.
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

#ifndef _CY_USB_APP_H_
#define _CY_USB_APP_H_

#include "cy_debug.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_lvds.h"
#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define USB_DESC_ATTRIBUTES __attribute__ ((section(".descSection"), used)) __attribute__ ((aligned (32)))
#define HBDMA_BUF_ATTRIBUTES __attribute__ ((section(".hbBufSection"), used)) __attribute__ ((aligned (32)))

#define RED                             "\033[0;31m"
#define CYAN                            "\033[0;36m"
#define COLOR_RESET                     "\033[0m"

#define LOG_COLOR(...)                  Cy_Debug_AddToLog(1,CYAN);\
                                        Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                        Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_ERROR(...)                  Cy_Debug_AddToLog(1,RED);\
                                        Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                        Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_CLR(CLR, ...)               Cy_Debug_AddToLog(1,CLR);\
                                        Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                        Cy_Debug_AddToLog(1,COLOR_RESET);


#define LOG_TRACE()                     LOG_COLOR("-->[%s]:%d\r\n",__func__,__LINE__);


#define DELAY_MICRO(us)                 Cy_SysLib_DelayUs(us)
#define DELAY_MILLI(ms)                 Cy_SysLib_Delay(ms)


#define SET_BIT(byte, mask)              (byte) |= (mask)
#define CLR_BIT(byte, mask)              (byte) &= ~(mask)
#define CHK_BIT(byte, mask)              (byte) & (mask)

#define ASSERT(condition, value)        Cy_CheckStatus(__func__, __LINE__, condition, value, true);
#define ASSERT_NON_BLOCK(condition, value) Cy_CheckStatus(__func__, __LINE__, condition, value, false);
#define ASSERT_AND_HANDLE(condition, value, failureHandler) Cy_CheckStatusHandleFailure(__func__, __LINE__, condition, value, false, Cy_FailHandler);

/* Get the LS byte from a 16-bit number */
#define CY_GET_LSB(w)                           ((uint8_t)((w) & UINT8_MAX))

/* Get the MS byte from a 16-bit number */
#define CY_GET_MSB(w)                           ((uint8_t)((w) >> 8))

#define LA_RX_MAX_BUFFER_COUNT                  (4)             /* Number of FIFO buffers used for streaming. */
#define LA_RX_MAX_BUFFER_SIZE                   (61440)         /* Size of each buffer used for streaming. */

#define LOGIC_ANALYZER_BULK_IN_ENDPOINT_1       (0x02)          /* Logic analyzer endpoint: 2-IN */

#define CY_USB_DEVICE_MSG_QUEUE_SIZE            (16)
#define CY_USB_DEVICE_MSG_SIZE                  (sizeof (cy_stc_usbd_app_msg_t))

/* List of codes for messages sent to the application task. */
#define CY_USB_UVC_VBUS_CHANGE_INTR             (0x0E)          /* Vbus voltage change interrupt received */
#define CY_USB_UVC_VBUS_CHANGE_DEBOUNCED        (0x0F)          /* Vbus voltage change debounce completed */
#define CY_USB_STREAMING_START                  (0x10)          /* Streaming start request */
#define CY_USB_STREAMING_STOP                   (0x11)          /* Streaming stop request */
#define USB_TMC_APP_MSG_OUT_RCVD_EVENT          (0x12)          /* TMC command received event */
#define USB_TMC_APP_MSG_IN_SEND_EVENT           (0x13)          /* TMC response to be sent event */

/* P4.0 is used for VBus detect functionality. */
#define VBUS_DETECT_GPIO_PORT                   (P4_0_PORT)
#define VBUS_DETECT_GPIO_PIN                    (P4_0_PIN)
#define VBUS_DETECT_GPIO_INTR                   (ioss_interrupts_gpio_dpslp_4_IRQn)
#define VBUS_DETECT_STATE                       (0u)

/* Vendor command code used to return WinUSB specific descriptors. */
#define MS_VENDOR_CODE                         (0xF0)           /* Vendor code for Microsoft OS descriptor fetch */
#define CMD_GET_FW_VERSION                     (0xB0)           /* Get firmware version request */
#define CMD_GET_REVID_VERSION                  (0xB1)           /* Get revision ID request */
#define CMD_START                              (0xB2)           /* Logic analyzer capture start request */
#define CMD_STOP                               (0xB3)           /* Logic analyzer capture stop request */

#define USB_PID_LOGIC_ANALYZER                 (0x4907)         /* USB Product ID used by logic analyzer device */

#if USB_TMC_EN
#define USB_INTF_TMC                            (0)
#define USBTMC_CLASS                            0xFE
#define USBTMC_SUBCLASS                         0x03
#define USBTMC_PROTOCOL_GENERIC                 0x00
#define TMC_PROTOCOL_IEEE488                    0x01
#define USBTMC_PROTOCOL                         TMC_PROTOCOL_IEEE488
#define USB_PID_TMC                             (0xCCC0 + USBTMC_PROTOCOL)
#define CY_FX_USB_PID                           (USB_PID_TMC)

#if (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488)
#define TMC_SUPPORT_INTERRUPT  1                                /* TMC interrupt endpoint enabled */
#else
#define TMC_SUPPORT_INTERRUPT  0                                /* TMC interrupt endpoint disabled */
#endif

#define TMC_ENDPOINT_COUNT                      (0x02 + TMC_SUPPORT_INTERRUPT)
#define TMC_MSG_OUT_ENDPOINT                    0x04   /* EP 4 OUT */
#define TMC_MSG_IN_ENDPOINT                     0x05   /* EP 5 IN */
#define TMC_MSG_INTR_ENDPOINT                   0x06   /* EP 6 IN */

typedef enum cy_en_tmc_string_index
{
    CY_FX_USBTMC_STR_IDX_LID            = 0,
    CY_FX_USBTMC_STR_IDX_MFG,
    CY_FX_USBTMC_STR_IDX_PRODUCT,
    CY_FX_USBTMC_STR_IDX_SN,
} cy_en_tmc_string_index;

#else
#define TMC_ENDPOINT_COUNT                      (0x00)
#define CY_FX_USB_PID                           (USB_PID_LOGIC_ANALYZER)
#endif /* USB_TMC_EN */

#define CY_FX_USB_VID                            (0x04B4)
#define CY_FX_USB_PID_HS                         (CY_FX_USB_PID)

#define USB_CFG_DESC_LEN_HS                             \
    (9 +                                                \
     (USB_TMC_EN *(9 + (TMC_ENDPOINT_COUNT * 7))) +     \
     (USB_LOGIC_ANALYZER_EN * (9 + 7))                  \
    )

typedef unsigned int CyFxReturnStatus_t;

#define CY_FX_USB_MAJOR_VER                       0
#define CY_FX_USB_MINOR_VER                       1
#define CY_FX_USB_SUCCESS                        (0)
#define CY_FX_ERROR_BAD_ARGUMENT                 (0x40)
#define CY_FX_ERROR_NOT_SUPPORTED                (0x46)
#define CY_FX_ERROR_FAILURE                      (0x4A)
#define CY_FX_ERROR_BAD_SIZE                     (0x05)
#define CY_FX_USB_REQUEST_IN                     (0x80)
#define CY_FX_USB_REQUEST_OUT                    (0x00)

#define ARRAYSIZE(A)                             (sizeof(A)/sizeof((A)[0]))
#define LA_TX_MAX_BUFFER_COUNT                   (4)
#define LA_TX_MAX_BUFFER_SIZE                    (16384)
#define C_ASSERT(e)                              typedef char __C_ASSERT__[(e)?1:-1]

/* Global descriptor arrays. */
extern uint8_t glOsString[];
extern uint8_t glOsCompatibilityId[];
extern uint8_t glOsFeature[];
extern uint8_t CyFxUSB20DeviceDscr[];
extern uint8_t CyFxUSBDeviceQualDscr[];
extern uint8_t CyFxUSBStringLangIDDscr[];
extern uint8_t CyFxUSBManufactureDscr[];
extern uint8_t CyFxUSBProductDscr[];
extern uint8_t CyFxUSBBOSDscr[];
extern uint8_t CyFxUSBHSConfigDscr[];
extern uint8_t CyFxUSBFSConfigDscr[];
extern uint8_t CyFxUSBSNumberDscr[];

/* Global HBWSS DMA channel handle */
extern  cy_stc_hbdma_channel_t *glTmcResponseChannel;   /* Response Channel pointer. */
extern  cy_stc_hbdma_channel_t *glTmcCommandChannel;    /* Command Channel pointer. */
#if TMC_SUPPORT_INTERRUPT
extern  cy_stc_hbdma_channel_t *glTmcNotifChannel;      /* Notification Channel pointer. */
#endif /* TMC_SUPPORT_INTERRUPT */

extern uint8_t *g_MsgOutDmaBuffer[];
extern uint8_t *g_MsgInDmaBuffer;
extern uint8_t g_CtrlXferBuffer[];

typedef struct cy_stc_usb_app_ctxt_ cy_stc_usb_app_ctxt_t;

typedef enum cy_en_sampling_freq_type {
    SAMPLING_FREQ_500kHz=1,
    SAMPLING_FREQ_1MHz,
    SAMPLING_FREQ_5MHz,
    SAMPLING_FREQ_10MHz,
    SAMPLING_FREQ_15MHz,
    SAMPLING_FREQ_20MHz,
    SAMPLING_FREQ_25MHz,
    SAMPLING_FREQ_30MHz,
    SAMPLING_FREQ_35MHz,
    SAMPLING_FREQ_40MHz,
} cy_en_sampling_freq_type;

/*
 * USB application data structure which is bridge between USB system and device
 * functionality.
 * It maintains some usb system information which comes from USBD and it also
 * maintains info about functionality.
 */
struct cy_stc_usb_app_ctxt_
{
    uint8_t                    firstInitDone;           /* APP initialization done */
    bool                       vbusChangeIntr;          /* VBus change interrupt received */
    bool                       vbusPresent;             /* VBus supply is active */
    bool                       usbConnected;            /* USB connection is enabled */

    uint8_t                    logicAnalyzerEp;         /* Endpoint number used for logic analyzer streaming */
    uint8_t                    tmcResponseEp;           /* IN endpoint used to send TMC responses */
    uint8_t                    tmcNotifyEp;             /* IN endpoint used to send TMC notifications */
    uint8_t                    tmcCommandEp;            /* OUT endpoint used to receive TMC commands */

    uint8_t                    devAddr;                 /* USB device address assigned */
    uint8_t                    activeCfgNum;            /* Current USB configuration index */
    uint8_t                    prevAltSetting;          /* Previous alternate setting */
    cy_en_usb_speed_t          devSpeed;                /* Current USB connection speed */

    cy_en_usb_device_state_t   devState;                /* Current USB device state */
    cy_en_usb_device_state_t   prevDevState;            /* Previous USB device state */

    cy_stc_app_endp_dma_set_t  endpInDma[CY_USB_MAX_ENDP_NUMBER];
    cy_stc_app_endp_dma_set_t  endpOutDma[CY_USB_MAX_ENDP_NUMBER];

    DMAC_Type                  *pCpuDmacBase;           /* DMAC register base */
    DW_Type                    *pCpuDw0Base;            /* DataWire-0 register base */
    DW_Type                    *pCpuDw1Base;            /* DataWire-1 register base */
    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt;          /* High BandWidth DMA manager context structure */
    cy_stc_usb_usbd_ctxt_t     *pUsbdCtxt;              /* USB stack context structure */

    cy_stc_hbdma_channel_t     *hbLADmaChannel;         /* DMA channel used for logic analyzer streaming */
    cy_stc_hbdma_channel_t     *hbTMCCommandChannel;    /* DMA channel used to receive TMC commands */
    cy_stc_hbdma_channel_t     *hbTMCResponseChannel;   /* DMA channel used to send TMC responses */
    cy_stc_hbdma_channel_t     *hbTMCNotifyChannel;     /* DMA channel used to send TMC notifications */

    uint8_t                    curTmcBufIndex;          /* Index of TMC command buffer to be filled */

    TaskHandle_t               laTaskHandle;            /* Application task handle */
    QueueHandle_t              usbMsgQueue;             /* Queue used to send messages to the application task */
    TimerHandle_t              vbusDebounceTimer;       /* Timer used to debounce VBus */
};

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
                    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt);

/**
 * \name Cy_USB_AppRegisterCallback
 * \brief  This function will register all callbacks with USBD layer.
 * \param pAppCtxt application layer context pointer.
 * \retval None
 */
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt);

/**
 * \name Cy_USB_AppSetCfgCallback
 * \brief Callback function will be invoked by USBD when set configuration is received
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD layer context pointer.
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetCfgCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppBusResetCallback
 * \brief Callback function will be invoked by USBD when bus detects RESET
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppBusResetDoneCallback
 * \brief Callback function will be invoked by USBD when RESET is completed
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppBusSpeedCallback
 * \brief   Callback function will be invoked by USBD when speed is identified or
 *          speed change is detected
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSetupCallback
 * \brief Callback function will be invoked by USBD when SETUP packet is received
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSuspendCallback
 * \brief Callback function will be invoked by USBD when Suspend signal/message is detected
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppResumeCallback
 * \brief Callback function will be invoked by USBD when Resume signal/message is detected
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppResumeCallback (void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSetIntfCallback
 * \brief Callback function will be invoked by USBD when SET_INTERFACE is  received
 * \param pAppCtxt application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetIntfCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppQueueWrite
 * \brief Queue USBHS Write on the USB endpoint
 * \param pAppCtxt application layer context pointer.
 * \param endpNumber Endpoint number
 * \param pBuffer Data Buffer Pointer
 * \param dataSize DataSize to send on USB bus
 * \retval None
 */
void Cy_USB_AppQueueWrite(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, uint8_t *pBuffer, uint16_t dataSize);

/**
 * \name Cy_USB_AppInitDmaIntr
 * \brief Function to register an ISR for the DMA channel associated with an endpoint
 * \param endpNumber USB endpoint number
 * \param endpDirection Endpoint direction
 * \param userIsr ISR function pointer. Can be NULL if interrupt is to be disabled.
 * \retval None
 */
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection, cy_israddress userIsr);

/**
 * \name Cy_USB_AppClearDmaInterrupt
 * \brief Clear DMA Interrupt
 * \param pAppCtxt application layer context pointer.
 * \param endpNumber Endpoint number
 * \param endpDirection Endpoint direction
 * \retval None
 */
void Cy_USB_AppClearDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt, uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection);

/**
 * \name Cy_LA_AppHandleRxCompletion
 * \brief Logic Analyzer receive completion handler
 * \param pUsbApp application layer context pointer.
 * \param  index Logic Analyzer channel index number
 * \retval None
 */
void Cy_LA_AppHandleRxCompletion (cy_stc_usb_app_ctxt_t *pUsbApp, uint8_t index);

/**
 * \name Cy_App_UsbOutISR
 * \brief ISR for DataWire-0 channels (0 to 15) used for USB OUT data transfers.
 * \retval None
 */
void Cy_App_UsbOutISR(void);

/**
 * \name Cy_App_UsbInISR
 * \brief ISR for DataWire-1 channels (0 to 15) used for USB IN data transfers.
 * \retval None
 */
void Cy_App_UsbInISR(void);

/**
 * \name Cy_LA_RxChannel2_ISR
 * \brief Datawire ISR for Logic Analyzer RX for channel#2
 * \retval None
 */
void Cy_LA_RxChannel2_ISR(void);

/**
 * \name Cy_LA_RxDataWireCombined_ISR
 * \brief Datawire combined ISR for CM0+
 * \retval None
 */
void Cy_LA_RxDataWireCombined_ISR (void);

/**
 * \name Cy_USB_EnableUsbHSConnection
 * \brief Enable USBHS connection
 * \param pAppCtxt Pointer to UVC application context structure.
 * \retval None
 */
bool Cy_USB_EnableUsbHSConnection(cy_stc_usb_app_ctxt_t *pAppCtxt);

/**
 * \name Cy_USB_DisableUsbHSConnection
 * \brief Disable USBHS connection
 * \retval None
 */
void Cy_USB_DisableUsbHSConnection (cy_stc_usb_app_ctxt_t *pAppCtxt);

/**
 * \name Cy_LA_LvdsInit
 * \brief   Initialize the LVDS interface. Currently, only the SIP #0 is being initialized
 *          and configured to allow transfers into the HBW SRAM through DMA.
 * \retval None
 */
void Cy_LA_LvdsInit(void);

/**
 * \name Cy_LA_LvdsDeinit
 * \brief  De-initialize the LVCMOS interface which collects the incoming digital channel data.
 * \retval None
 */
void Cy_LA_LvdsDeinit(void);

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
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking);

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
void Cy_CheckStatusHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)());

/**
 * \name Cy_FailHandler
 * \brief Error Handler
 * \retval None
 */
void Cy_FailHandler(void);

/**
 * \name Cy_USB_AppL1SleepCallback
 * \brief This Function will be called by USBD layer when L1 Sleep message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppL1SleepCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppL1ResumeCallback
 * \brief This Function will be called by USBD layer when L1 Resume message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppL1ResumeCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppZlpCallback
 * \brief This Function will be called by USBD layer when ZLP message comes
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSetFeatureCallback
 * \brief This Function will be called by USBD layer when set feature message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppClearFeatureCallback
 * \brief This Function will be called by USBD layer when clear feature message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppClearFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

void Cy_USBTMCAppHaltEndpoint(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t Endpoint);

/**
 * \name Cy_USB_AppZlpCallback
 * \brief This Function will be called by USBD layer when ZLP message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSlpCallback
 * \brief This Function will be called by USBD layer when SLP message comes.
 * \param pUsbApp application layer context pointer.
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_LVDS_GpifRegUpdate
 * \brief set lvcmos parameters
 * \param use16Bits Whether to use 16 bit data
 * \param clkRegValue register value
 * \param clkSrc source
 * \param clkDivider Divider
 * \retval None
 */
void Cy_LVDS_GpifRegUpdate(bool use16Bits, uint32_t clkRegValue, cy_en_lvds_gpif_clk_src_t clkSrc, cy_en_lvds_gpif_clk_divider_t clkDivider);

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_APP_H_ */

/* End of File */

