/***************************************************************************//**
* \file main.c
* \version 1.0
*
* \details  This is the source code for the USB Logic Analyzer Application Example
*           for ModusToolbox.
*
*           See \ref README.md ["README.md"]
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
#include "cybsp.h"
#include <string.h>
#include "cy_usb_common.h"
#include "cy_usb_usbd.h"
#include "usb_app.h"
#include "cy_debug.h"
#include "cy_usbd_version.h"
#include "cy_hbdma_version.h"
#include "cy_lvds.h"
#include "app_version.h"
#include "timers.h"
#include "cy_gpif_header.h"
#include "cy_fx_common.h"

/* Select SCB interface used for UART based logging. */
#define LOGGING_SCB             (SCB4)
#define LOGGING_SCB_IDX         (4)
#define DEBUG_LEVEL             (3u)

/* Debug log related initialization */
#if DEBUG_INFRA_EN
/* RAM buffer used to hold debug log data. */
#define LOGBUF_SIZE (1024u)
uint8_t logBuff[LOGBUF_SIZE];

cy_stc_debug_config_t dbgCfg = {
    .pBuffer         = logBuff,
    .traceLvl        = DEBUG_LEVEL,
    .bufSize         = LOGBUF_SIZE,
#if USBFS_LOGS_ENABLE
    .dbgIntfce       = CY_DEBUG_INTFCE_USBFS_CDC,
#else
    .dbgIntfce       = CY_DEBUG_INTFCE_UART_SCB4,
#endif /* USBFS_LOGS_ENABLE */
    .printNow        = true
};

TaskHandle_t printLogTaskHandle;
#endif /* DEBUG_INFRA_EN */

/* Global variables associated with High BandWidth DMA setup. */
cy_stc_hbdma_context_t     HBW_DrvCtxt;         /* High BandWidth DMA driver context. */
cy_stc_hbdma_dscr_list_t   HBW_DscrList;        /* High BandWidth DMA descriptor free list. */
cy_stc_hbdma_buf_mgr_t     HBW_BufMgr;          /* High BandWidth DMA buffer manager. */
cy_stc_hbdma_mgr_context_t HBW_MgrCtxt;         /* High BandWidth DMA manager context. */

/* Global variables associated with LVDS setup. */
cy_stc_lvds_context_t lvdsContext;

/* CPU DMA register pointers. */
DMAC_Type *pCpuDmacBase;
DW_Type   *pCpuDw0Base;
DW_Type   *pCpuDw1Base;

/* USB and application context structures. */
cy_stc_usb_usbd_ctxt_t usbdCtxt;
cy_stc_usb_cal_ctxt_t  hsCalCtxt;
cy_stc_usb_app_ctxt_t  appCtxt;

extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);

/**
 * \name vPortSetupTimerInterrupt
 * \retval None
 */
void vPortSetupTimerInterrupt(void)
{
    /* Register the exception vectors. */
    Cy_SysInt_SetVector(PendSV_IRQn, xPortPendSVHandler);
    Cy_SysInt_SetVector(SVCall_IRQn, vPortSVCHandler);
    Cy_SysInt_SetVector(SysTick_IRQn, xPortSysTickHandler);

    /* Start the SysTick timer with a period of 1 ms. */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU);

#if CY_CPU_CORTEX_M4
    Cy_SysTick_SetReload(Cy_SysClk_ClkFastGetFrequency() / 1000U);
#else
    Cy_SysTick_SetReload(Cy_SysClk_ClkSlowGetFrequency() / 1000U);
#endif /* CY_CPU_CORTEX_M4 */

    Cy_SysTick_Clear();
    Cy_SysTick_Enable();
}

#if DEBUG_INFRA_EN
void PrintTaskHandler(void *pTaskParam)
{
    while (1)
    {
        /* Print any pending logs to the output console. */
        Cy_Debug_PrintLog();

        /* Put the thread to sleep for 5 ms */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif /* DEBUG_INFRA_EN */

/**
 * \name Cy_Slff_GpifIntr
 * \brief GPIF Interrupt Handler
 * \param pApp app context
 * \retval None
 */
void
Cy_LA_GpifIntr(void *pApp)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;

    /* Reset the DMA channel through which data is received from the LVDS side. */
    if (pAppCtxt->hbLADmaChannel != NULL)
    {
        Cy_HBDma_Channel_Reset(pAppCtxt->hbLADmaChannel);

        /* Flush and reset the endpoint and clear the STALL bit. */
        Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->logicAnalyzerEp, CY_USB_ENDP_DIR_IN);
        Cy_USBD_ResetEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->logicAnalyzerEp, CY_USB_ENDP_DIR_IN, false);
        Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, pAppCtxt->logicAnalyzerEp, CY_USB_ENDP_DIR_IN, false);
    }

    return;
}

/**
 * \name Cy_LVDS_GpifEventCb
 * \brief GPIF error callback function.
 * \param smNo state machine number
 * \param gpifEvent GPIF event
 * \param cntxt app context
 * \retval None
 */
void Cy_LVDS_GpifEventCb(uint8_t smNo, cy_en_lvds_gpif_event_type_t gpifEvent, void *cntxt)
{
    Cy_LA_GpifIntr(&appCtxt);
}

/**
 * \name Cy_LVDS_PhyEventCb
 * \brief GPIF error callback function.
 * \param smNo state machine number
 * \param phyEvent LVCMOS PHY event
 * \param cntxt app context
 * \retval None
 */
void Cy_LVDS_PhyEventCb(uint8_t smNo, cy_en_lvds_phy_events_t phyEvent, void *cntxt)
{
    if(phyEvent == CY_LVDS_PHY_L3_ENTRY)
    {
        if(smNo == 0)
        {
            DBG_APP_INFO("P0_L3_Entry\r\n");
        }
    }
}

/**
 * \name Cy_LVDS_GpifErrorCb
 * \brief GPIF error callback function.
 * \param smNo state machine number
 * \param gpifError GPIF error
 * \param cntxt app context
 * \retval None
 */
void Cy_LVDS_GpifErrorCb(uint8_t smNo, cy_en_lvds_gpif_error_t gpifError, void *cntxt)
{
    switch (gpifError)
    {
        case CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_ADDR_RD_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR\n\r");
        break;
    }
}

/**
 * \name Cy_LVDS_GpifThreadErrorCb
 * \brief GPIF thread error callback function.
 * \param ThNo thread number
 * \param ThError thread error
 * \param cntxt app context
 * \retval None
 */
void Cy_LVDS_GpifThreadErrorCb (cy_en_lvds_gpif_thread_no_t ThNo, cy_en_lvds_gpif_thread_error_t ThError, void *cntxt)
{
    switch (ThNo)
    {
        case CY_LVDS_GPIF_THREAD_0:
        case CY_LVDS_GPIF_THREAD_1:
        switch (ThError)
        {
            case CY_LVDS_GPIF_THREAD_DIR_ERROR:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_DIR_ERROR\n\r");
            break;

            case CY_LVDS_GPIF_THREAD_WR_OVERFLOW:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_WR_OVERFLOW\n\r");
            break;

            case CY_LVDS_GPIF_THREAD_RD_UNDERRUN:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_RD_UNDERRUN\n\r");
            break;

            case CY_LVDS_GPIF_THREAD_SCK_ACTIVE:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_SCK_ACTIVE\n\r");
            break;

            case CY_LVDS_GPIF_THREAD_ADAP_OVERFLOW:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_ADAP_OVERFLOW\n\r");
            break;

            case CY_LVDS_GPIF_THREAD_ADAP_UNDERFLOW:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_ADAP_UNDERFLOW\n\r");
            break;

            case CY_LVDS_GPIF_THREAD_READ_FORCE_END:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_READ_FORCE_END\n\r");
            break;

            case CY_LVDS_GPIF_THREAD_READ_BURST_ERR:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_READ_BURST_ERR\n\r");
            break;

            default:
                break;
        }
        break;

        default:
            break;
    }
}

cy_stc_lvds_app_cb_t cb =
{
    .gpif_events = Cy_LVDS_GpifEventCb,
    .gpif_error = Cy_LVDS_GpifErrorCb,
    .gpif_thread_error = Cy_LVDS_GpifThreadErrorCb,
    .gpif_thread_event = NULL,
    .phy_events = Cy_LVDS_PhyEventCb,
    .low_power_events   = NULL
};

/**
 * \name Cy_LVDS_GpifRegUpdate
 * \brief set lvcmos parameters
 * \param use16Bits Whether to use 16 bit data
 * \param clkRegValue register value
 * \param clkSrc source
 * \param clkDivider Divider
 * \retval None
 */
void Cy_LVDS_GpifRegUpdate(bool use16Bits, uint32_t clkRegValue, cy_en_lvds_gpif_clk_src_t clkSrc,
                            cy_en_lvds_gpif_clk_divider_t clkDivider)
{
    uint32_t i;

    cy_lvds_phy0_config.clkSrc = clkSrc;
    cy_lvds_phy0_config.clkDivider = clkDivider;
    if (use16Bits) {
        cy_lvds_phy0_config.dataBusWidth = CY_LVDS_PHY_LVCMOS_MODE_NUM_LANE_16;
    } else {
        cy_lvds_phy0_config.dataBusWidth = CY_LVDS_PHY_LVCMOS_MODE_NUM_LANE_8;
    }

    for (i = 0; i < (sizeof(cy_lvds_gpif0_reg_data)/sizeof(cy_stc_lvds_gpif_reg_data_t)); i++)
    {
        if (cy_lvds_gpif0_reg_data[i].regAddr == 0x000002C0) {
            cy_lvds_gpif0_reg_data[i].regValue = clkRegValue;
            break;
        }
    }
}

/**
 * \name Cy_LA_LvdsDeinit
 * \brief  De-initialize the LVCMOS interface which collects the incoming digital channel data.
 * \retval None
 */
void Cy_LA_LvdsDeinit (void)
{
    uint32_t intState = Cy_SysLib_EnterCriticalSection();

    /* Disable and de-initialize the LVCMOS interface. */
    Cy_LVDS_Disable(LVDSSS_LVDS);
    Cy_LVDS_Deinit(LVDSSS_LVDS, 0, &lvdsContext);

    /* Disable and re-enable the High BandWidth DMA adapters. */
    Cy_HBDma_DeInit(&HBW_DrvCtxt);
    Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);

    Cy_SysLib_ExitCriticalSection(intState);
}

/**
 * \name Cy_LA_LvdsInit
 * \brief  Initialize the LVCMOS interface which receives the incoming digital channel data.
 * \retval None
 */
void Cy_LA_LvdsInit (void)
{
    cy_en_lvds_status_t status = CY_LVDS_SUCCESS;

    uint16_t dataCntLimit;

    if (cy_lvds_phy0_config.dataBusWidth == CY_LVDS_PHY_LVCMOS_MODE_NUM_LANE_16) {
        /* Each cycle brings 2 bytes of data. So, counter needs to run till (LA_RX_MAX_BUFFER_SIZE / 2). */
        dataCntLimit = (LA_RX_MAX_BUFFER_SIZE >> 1U) - 1;
    } else {
        /* Each cycle brings 1 byte of data. */
        dataCntLimit = LA_RX_MAX_BUFFER_SIZE - 1;
    }

    Cy_LVDS_Init(LVDSSS_LVDS, 0, &cy_lvds0_config, &lvdsContext);
    DBG_APP_INFO("LVDS_Init done\r\n");

    /* Set Interrupt Mask for GPIF */
    Cy_LVDS_SetInterruptMask(LVDSSS_LVDS, LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF0_INTERRUPT_Msk);
    DBG_APP_INFO("Set Interrupt Mask for GPIF\r\n");

    Cy_LVDS_RegisterCallback(LVDSSS_LVDS, &cb, &lvdsContext, &appCtxt);

    /* Enable threads 0 and 1 with sockets 0 and 1 respectively. */
    Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 0, 0, 0, 0, 0);
    Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 1, 1, 0, 0, 0);

#if CLKOUT_ENABLE
    /*Enable Clock Out*/
    Cy_LVDS_ClkOutEnable(LVDSSS_LVDS,CY_LVDS_PHY_INTERFACE_CLK_OUT_48_MHZ);
#endif /* CLKOUT_ENABLE */

    Cy_LVDS_Enable(LVDSSS_LVDS);
    DBG_APP_INFO("LVCMOS Enabled\r\n");

    /* GPIF data counter needs to be initialized with proper limit here. */
    Cy_LVDS_GpifInitDataCounter(LVDSSS_LVDS, 0, 0, 0, 0, dataCntLimit, true, true, 1);

    status = Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 0, START, ALPHA_START);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
}

/**
 * \name Cy_LVDS_ISR
 * \brief Handler for LVDS Interrupts.
 * \retval None
 */

void Cy_LVDS_ISR(void)
{
    Cy_LVDS_IrqHandler(LVDSSS_LVDS, &lvdsContext);
}

/**
 * \name Cy_LvdsPortDma_ISR
 * \brief Handler for LVDS Port0 Interrupts.
 * \retval None
 */
void Cy_LvdsPortDma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_LVDS_0);
    portYIELD_FROM_ISR(true);
}

/**
 * \name Cy_USB_HS_ISR
 * \brief Handler for USB-HS Interrupts.
 * \retval None
 */
void Cy_USB_HS_ISR(void)
{
    if (Cy_USBHS_Cal_IntrHandler(&hsCalCtxt))
    {
        portYIELD_FROM_ISR(true);
    }
}

/**
 * \name Cy_App_UsbInISR
 * \brief ISR for DataWire-1 channels (0 to 15) used for USB IN data transfers.
 * \retval None
 */
void Cy_App_UsbInISR (void)
{
    Cy_HBDma_Mgr_HandleDW1Interrupt(appCtxt.pHbDmaMgrCtxt);
    portYIELD_FROM_ISR(true);
}

/**
 * \name Cy_App_UsbOutISR
 * \brief ISR for DataWire-0 channels (0 to 15) used for USB OUT data transfers.
 * \retval None
 */
void Cy_App_UsbOutISR (void)
{
    Cy_HBDma_Mgr_HandleDW0Interrupt(appCtxt.pHbDmaMgrCtxt);
    portYIELD_FROM_ISR(true);
}

/**
 * \name Cy_Fx2g3_InitPeripheralClocks
 * \brief Function used to enable clocks to different peripherals on the FX2G3 device.
 * \param usbfsClkEnable Whether to enable bus reset detect clock input to the USBFS block.
 * \retval None
 */
void Cy_Fx2g3_InitPeripheralClocks (
        bool usbfsClkEnable)
{
    if (usbfsClkEnable) {
        /* Divide PERI clock at 75 MHz by 750 to get 100 KHz clock using 16-bit divider #2. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 2, 749);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 2);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_USB_CLOCK_DEV_BRS, CY_SYSCLK_DIV_16_BIT, 2);
    }
}

/**
 * \name Cy_PrintVersionInfo
 * \brief Function to print version information to UART console.
 * \param type Type of version string.
 * \param version Version number including major, minor, patch and build number.
 * \retval None
 */
void Cy_PrintVersionInfo(const char *type, uint32_t version)
{
    char tString[32];
    uint16_t vBuild;
    uint8_t vMajor, vMinor, vPatch;
    uint8_t typeLen = strlen(type);

    vMajor = (version >> 28U);
    vMinor = ((version >> 24U) & 0x0FU);
    vPatch = ((version >> 16U) & 0xFFU);
    vBuild = (uint16_t)(version & 0xFFFFUL);

    memcpy(tString, type, typeLen);
    tString[typeLen++] = '0' + (vMajor / 10);
    tString[typeLen++] = '0' + (vMajor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vMinor / 10);
    tString[typeLen++] = '0' + (vMinor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vPatch / 10);
    tString[typeLen++] = '0' + (vPatch % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vBuild / 1000);
    tString[typeLen++] = '0' + ((vBuild % 1000) / 100);
    tString[typeLen++] = '0' + ((vBuild % 100) / 10);
    tString[typeLen++] = '0' + (vBuild % 10);
    tString[typeLen++] = '\r';
    tString[typeLen++] = '\n';
    tString[typeLen] = 0;

    Cy_Debug_AddToLog(1,"%s", tString);
}

/**
 * \name Cy_USB_VbusDetGpio_ISR
 * \brief Interrupt handler for the Vbus detect GPIO transition detection.
 * \retval None
 */
static void Cy_USB_VbusDetGpio_ISR(void)
{
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;

    /* Send VBus changed message to the task thread. */
    xMsg.type = CY_USB_UVC_VBUS_CHANGE_INTR;
    xQueueSendFromISR(appCtxt.usbMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

    /* Remember that VBus change has happened and disable the interrupt. */
    appCtxt.vbusChangeIntr = true;
    Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 0);
}

/**
 * \name Cy_USB_USBHSInit
 * \brief Initialize USBHS block and attempt device enumeration.
 * \retval None
 */
void Cy_USB_USBHSInit(void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_sysint_t intrCfg;

    /* Do all the relevant clock configuration */
    Cy_Fx2g3_InitPeripheralClocks(true);

    /* Unlock and then disable the watchdog. */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();

#if CY_CPU_CORTEX_M4
    __set_PRIMASK(0);
#endif /* CY_CPU_CORTEX_M4 */

    /* Enable interrupts. */
    __enable_irq();

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure VBus detect GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    pinCfg.intEdge   = CY_GPIO_INTR_BOTH;
    pinCfg.intMask   = 0x01UL;
    Cy_GPIO_Pin_Init(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, &pinCfg);

    /* CM0+: Interrupt source to NVIC Mux map
       LVDS - NVIC Mux #0
       VBUS(GPIO#4) - NVIC Mux #2
       SIP0 DMA - NVIC Mux #4
       USBHS Active & DeepSleep - NVIC Mux #5
       DataWire 0 - NVIC Mux #6
       DataWire 1 - NVIC Mux #1
    */
    /* Register edge detect interrupt for Vbus detect GPIO. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 7;
#else
    intrCfg.cm0pSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrSrc = NvicMux2_IRQn;
    intrCfg.intrPriority = 3;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_VbusDetGpio_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /*
     * Enable the DMA buffer RAM
     * need to be enabled before we try to do High BandWidth DMA initialization.
     */
    MAIN_REG->CTRL |= MAIN_REG_CTRL_IP_ENABLED_Msk;

    /* Register the LVDS ISR and enable the interrupt for LVDS. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_int_o_IRQn;
    intrCfg.intrPriority = 6;
#else
    intrCfg.intrSrc = NvicMux0_IRQn;
    intrCfg.intrPriority = 2;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_LVDS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the ISR and enable the interrupt for SIP0 DMA adapter. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux4_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_LvdsPortDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBHS Interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrSrc      = NvicMux5_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_HS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBHS Interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_dpslp_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc      = usbhsdev_interrupt_u2d_dpslp_o_IRQn;
    intrCfg.intrSrc      = NvicMux5_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_HS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
}

/**
 * \name Cy_LA_HbDmaInit
 * \brief Initialize HBDMA block.
 * \retval None
 */
bool Cy_LA_HbDmaInit(void)
{
    cy_en_hbdma_status_t drvstat;
    cy_en_hbdma_mgr_status_t mgrstat;

    /* Initialize the HBW DMA driver layer. */
    drvstat = Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    if (drvstat != CY_HBDMA_SUCCESS)
    {
        return false;
    }

    /* Setup a HBW DMA descriptor list. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList, 256U);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the DMA buffer manager. We will use 512 KB of space from 0x1C030000 onwards. */
    mgrstat = Cy_HBDma_BufMgr_Create(&HBW_BufMgr, (uint32_t *)0x1C030000UL, 0x80000UL);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the HBW DMA channel manager. */
    mgrstat = Cy_HBDma_Mgr_Init(&HBW_MgrCtxt, &HBW_DrvCtxt, &HBW_DscrList, &HBW_BufMgr);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Register the USB stack context with the HBW DMA manager. */
    Cy_HBDma_Mgr_RegisterUsbContext(&HBW_MgrCtxt, &usbdCtxt);

    return true;
}

/**
 * \name Cy_USB_EnableUsbHSConnection
 * \brief Enable USBHS connection
 * \retval None
 */
bool Cy_USB_EnableUsbHSConnection (cy_stc_usb_app_ctxt_t *pAppCtxt)
{

    DBG_APP_INFO("USB: Enable connection\r\n");
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, CY_USBD_USB_DEV_HS);
    pAppCtxt->usbConnected = true;
    return true;
}

/**
 * \name Cy_USB_DisableUsbHSConnection
 * \brief Disable USBHS connection
 * \retval None
 */
void Cy_USB_DisableUsbHSConnection (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    Cy_USBD_DisconnectDevice(pAppCtxt->pUsbdCtxt);
    pAppCtxt->usbConnected = false;
    pAppCtxt->devState     = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
}

/**
 * \name main
 * \brief Entry to the application.
 * \retval Does not return.
 */
int main(void)
{
    pCpuDmacBase = ((DMAC_Type *)DMAC_BASE);
    pCpuDw0Base = ((DW_Type *)DW0_BASE);
    pCpuDw1Base = ((DW_Type *)DW1_BASE);

    /* Initialize the PDL driver library and set the clock variables. */
    /* Note: All FX devices, share a common configuration structure. */
    Cy_PDL_Init(&cy_deviceIpBlockCfgFX3G2);

    /* Initialize the device and board peripherals */
    cybsp_init();

    /* Initialize the PDL and register ISR for USB block. */
    Cy_USB_USBHSInit();

#if DEBUG_INFRA_EN
#if !USBFS_LOGS_ENABLE
    /* Initialize the UART for logging. */
    InitUart(LOGGING_SCB_IDX);
#endif /* USBFS_LOGS_ENABLE */

    /*
     * Initialize the logger module. We are using a blocking print option which will
     * output the messages immediately without buffering.
     */
    Cy_Debug_LogInit(&dbgCfg);
    Cy_SysLib_Delay(500);

    Cy_Debug_AddToLog(1, "***** FX2G3: Logic Analyzer Application (Sigrok Pulseview/Test and Measurement Class) *****\r\n");

    /* Print application, USBD stack and HBDMA version information. */
    Cy_PrintVersionInfo("APP_VERSION: ", APP_VERSION_NUM);
    Cy_PrintVersionInfo("USBD_VERSION: ", USBD_VERSION_NUM);
    Cy_PrintVersionInfo("HBDMA_VERSION: ", HBDMA_VERSION_NUM);

    /* Create task for printing logs and check status. */
    xTaskCreate(PrintTaskHandler, "PrintLogTask", 512, NULL, 5, &printLogTaskHandle);
#endif /* DEBUG_INFRA_EN */

    memset((uint8_t *)&appCtxt, 0, sizeof(appCtxt));
    memset((uint8_t *)&hsCalCtxt, 0, sizeof(hsCalCtxt));
    memset((uint8_t *)&usbdCtxt, 0, sizeof(usbdCtxt));

    /* Store IP base address in CAL context. */
    hsCalCtxt.pCalBase = MXS40USBHSDEV_USBHSDEV;
    hsCalCtxt.pPhyBase = MXS40USBHSDEV_USBHSPHY;

    /* Initialize the HbDma IP and DMA Manager */
    Cy_LA_HbDmaInit();
    DBG_APP_INFO("HbDmaInit done \r\n");

    /* Initialize the USBD layer */
    Cy_USB_USBD_Init(&appCtxt, &usbdCtxt, pCpuDmacBase, &hsCalCtxt,NULL, &HBW_MgrCtxt);
    DBG_APP_INFO("USBD_Init done\r\n");

    /* Enable stall cycles between back-to-back AHB accesses to high bandwidth RAM. */
    MAIN_REG->CTRL = (MAIN_REG->CTRL & 0xF00FFFFFUL) | 0x09900000UL;

    /* Register USB descriptors with the stack. */
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_HS_BOS_DSCR, 0, (uint8_t *)CyFxUSBBOSDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_DEVICE_QUAL_DSCR, 0, (uint8_t *)CyFxUSBDeviceQualDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_HS_CONFIG_DSCR, 0, (uint8_t *)CyFxUSBHSConfigDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_FS_CONFIG_DSCR, 0, (uint8_t *)CyFxUSBFSConfigDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_STRING_DSCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_STRING_DSCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_STRING_DSCR, 2, (uint8_t *)CyFxUSBProductDscr);
    Cy_USBD_SetDscr(&usbdCtxt, CY_USB_SET_STRING_DSCR, 3, (uint8_t *)CyFxUSBSNumberDscr);

    /* Initialize the application and create echo device thread. */
    Cy_USB_AppInit(&appCtxt, &usbdCtxt, pCpuDmacBase, pCpuDw0Base, pCpuDw1Base, &HBW_MgrCtxt);

    /* Invokes scheduler: Not expected to return. */
    DBG_APP_INFO("Starting RTOS scheduler\r\n");
    vTaskStartScheduler();

    while (1)
    {
        Cy_SysLib_Delay(10000);
        DBG_APP_INFO("Task Idle\r\n");
    }

    return 0;
}

/**
 * \name Cy_OnResetUser
 * \brief Init function which is executed before the load regions in RAM are updated.
 * \details
 * The High BandWidth subsystem needs to be enable here to allow variables
 * placed in the High BandWidth SRAM to be updated.
 * \retval None
 */
void Cy_OnResetUser(void)
{
    Cy_UsbFx_OnResetInit();
}

/* [] END OF FILE */
