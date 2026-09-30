/***************************************************************************//**
* \file tmc_scpi.c
* \version 1.0
*
* \brief Implements the TMC and SCPI Interface.
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

#include "types.h"
#include "parser.h"
#include "ieee488.h"
#include "minimal.h"
#include "units.h"

#include "usb_app.h"

#include "tmc_scpi.h"
#include "tmc_radar.h"
#include "tmc_radar_def.h"

#define DEBUG_MSG_SCPI_WRITE(...)
#define DEBUG_BUF_SCPI_WRITE(...)

#define SCPI_IDN_1      "IFX"
#define SCPI_IDN_2      "USB-TMC"
#define SCPI_IDN_3      "1234"
#define SCPI_IDN_4      "0.1"

static scpi_result_t Cy_TMC_Scpi_ParserCB_IdnQ(scpi_t*);

static scpi_result_t Cy_TMC_Scpi_ParserCB_Trg(scpi_t*);

static int Cy_TMC_Scpi_CoreCB_Error(scpi_t* context, int_fast16_t err);

static size_t Cy_TMC_Scpi_CoreCB_Write(scpi_t* context, const char* data, size_t len);

static scpi_result_t Cy_TMC_Scpi_CoreCB_Reset(scpi_t*);

static const scpi_command_t g_ScpiCommandArray[] =
{
    /* IEEE-488.2 Mandatory Commands */
    {.pattern = "*CLS",     .callback = SCPI_CoreCls },
    {.pattern = "*ESE",     .callback = SCPI_CoreEse },
    {.pattern = "*ESE?",    .callback = SCPI_CoreEseQ },
    {.pattern = "*ESR?",    .callback = SCPI_CoreEsrQ },
    {.pattern = "*IDN?",    .callback = Cy_TMC_Scpi_ParserCB_IdnQ },
    {.pattern = "*OPC",     .callback = SCPI_CoreOpc },
    {.pattern = "*OPC?",    .callback = SCPI_CoreOpcQ },
    {.pattern = "*RST",     .callback = SCPI_CoreRst },
    {.pattern = "*SRE",     .callback = SCPI_CoreSre },
    {.pattern = "*SRE?",    .callback = SCPI_CoreSreQ },
    {.pattern = "*STB?",    .callback = SCPI_CoreStbQ },
    {.pattern = "*TRG",     .callback = Cy_TMC_Scpi_ParserCB_Trg },
    {.pattern = "*TST?",    .callback = SCPI_CoreTstQ },
    {.pattern = "*WAI",     .callback = SCPI_CoreWai },

    /* SCPI-1999 Mandatory Commands */
    {.pattern = "SYSTem:ERRor[:NEXT]?",         .callback = SCPI_SystemErrorNextQ },
    {.pattern = "SYSTem:ERRor:COUNt?",          .callback = SCPI_SystemErrorCountQ },
    {.pattern = "SYSTem:VERSion?",              .callback = SCPI_SystemVersionQ },

    /* Virtual Instrument (Radar) Commands */
    {.pattern = FX_RADAR_CHANNEL_COUNT_Q,      .callback = Cy_TMC_Radar_ParserCB_ChannelCountQ },

    {.pattern = FX_RADAR_CH01_NAME_Q,          .callback = Cy_TMC_Radar_ParserCB_Ch01NameQ },
    {.pattern = FX_RADAR_CH02_NAME_Q,          .callback = Cy_TMC_Radar_ParserCB_Ch02NameQ },

    {.pattern = FX_RADAR_CH01_MODE,            .callback = Cy_TMC_Radar_ParserCB_Ch01Mode },
    {.pattern = FX_RADAR_CH02_MODE,            .callback = Cy_TMC_Radar_ParserCB_Ch02Mode },
    {.pattern = FX_RADAR_CH01_MODE_Q,          .callback = Cy_TMC_Radar_ParserCB_Ch01ModeQ },
    {.pattern = FX_RADAR_CH02_MODE_Q,          .callback = Cy_TMC_Radar_ParserCB_Ch02ModeQ },

    {.pattern = FX_RADAR_CH01_EQN1_FUN,        .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn1Fun },
    {.pattern = FX_RADAR_CH01_EQN2_FUN,        .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn2Fun },
    {.pattern = FX_RADAR_CH02_EQN1_FUN,        .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn1Fun },
    {.pattern = FX_RADAR_CH02_EQN2_FUN,        .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn2Fun },
    {.pattern = FX_RADAR_CH01_EQN1_FUN_Q,      .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn1FunQ },
    {.pattern = FX_RADAR_CH01_EQN2_FUN_Q,      .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn2FunQ },
    {.pattern = FX_RADAR_CH02_EQN1_FUN_Q,      .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn1FunQ },
    {.pattern = FX_RADAR_CH02_EQN2_FUN_Q,      .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn2FunQ },

    {.pattern = FX_RADAR_CH01_EQN1_PAR1,       .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm1 },
    {.pattern = FX_RADAR_CH01_EQN1_PAR2,       .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm2 },
    {.pattern = FX_RADAR_CH01_EQN2_PAR1,       .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm1 },
    {.pattern = FX_RADAR_CH01_EQN2_PAR2,       .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm2 },
    {.pattern = FX_RADAR_CH02_EQN1_PAR1,       .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm1 },
    {.pattern = FX_RADAR_CH02_EQN1_PAR2,       .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm2 },
    {.pattern = FX_RADAR_CH02_EQN2_PAR1,       .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm1 },
    {.pattern = FX_RADAR_CH02_EQN2_PAR2,       .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm2 },

    {.pattern = FX_RADAR_CH01_EQN1_PAR1_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm1Q },
    {.pattern = FX_RADAR_CH01_EQN1_PAR2_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm2Q },
    {.pattern = FX_RADAR_CH01_EQN2_PAR1_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm1Q },
    {.pattern = FX_RADAR_CH01_EQN2_PAR2_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm2Q },
    {.pattern = FX_RADAR_CH02_EQN1_PAR1_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm1Q },
    {.pattern = FX_RADAR_CH02_EQN1_PAR2_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm2Q },
    {.pattern = FX_RADAR_CH02_EQN2_PAR1_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm1Q },
    {.pattern = FX_RADAR_CH02_EQN2_PAR2_Q,     .callback = Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm2Q },

    {.pattern = FX_RADAR_CH01_DATA_Q,          .callback = Cy_TMC_Radar_ParserCB_Ch01DataQ },
    {.pattern = FX_RADAR_CH02_DATA_Q,          .callback = Cy_TMC_Radar_ParserCB_Ch02DataQ },

    SCPI_CMD_LIST_END
};

typedef struct cy_stc_tmc_scpi_ctx
{
    uint8_t                             m_MsgResponseBuffer[1024];
    char                                m_ScpiCoreBuffer[2048];
    scpi_t                              m_ScpiCoreCtx;
    scpi_interface_t                    m_ScpiInterface;
    scpi_error_t                        m_ScpiErrorQueue[32];
    uint32_t                            m_MsgResponseSize;
} cy_stc_tmc_scpi_ctx;

static cy_stc_tmc_scpi_ctx g_TmcScpiCtx __attribute__((aligned(32)));

static int
Cy_TMC_Scpi_CoreCB_Error (
    scpi_t* context,
    int_fast16_t err
    )
{
    LOG_ERROR("TMC: CoreCB Error");
    return 0;
}

static size_t
Cy_TMC_Scpi_CoreCB_Write (
    scpi_t* context,
    const char* data,
    size_t len
    )
{
    if (len > 0)
    {
        memcpy(g_TmcScpiCtx.m_MsgResponseBuffer + g_TmcScpiCtx.m_MsgResponseSize, (uint8_t*)data, len);
        g_TmcScpiCtx.m_MsgResponseSize += len;
    }

    return len;
}

static scpi_result_t
Cy_TMC_Scpi_CoreCB_Reset (
    scpi_t* context
    )
{
    return SCPI_RES_OK;
}


static scpi_result_t
Cy_TMC_Scpi_ParserCB_IdnQ(
    scpi_t* context
    )
{
    const uint8_t* pSrc;
    uint8_t* pDst;
    uint8_t i;
    uint8_t iMax;

    pDst = g_TmcScpiCtx.m_MsgResponseBuffer;

    /* manufacturer */
    for (i = 0, iMax = CyFxUSBManufactureDscr[0] - 2, pSrc = CyFxUSBManufactureDscr + 2; i < iMax; i += 2)
    {
        *pDst++ = *pSrc++;
        pSrc++;
    }

    *pDst++ = ',';

    /* model */
    for (i = 0, iMax = CyFxUSBProductDscr[0] - 2, pSrc = CyFxUSBProductDscr + 2; i < iMax; i += 2)
    {
        *pDst++ = *pSrc++;
        pSrc++;
    }

    *pDst++ = ',';

    /* serial number */
    for (i = 0, iMax = CyFxUSBSNumberDscr[0] - 2, pSrc = CyFxUSBSNumberDscr + 2; i < iMax; i += 2)
    {
        *pDst++ = *pSrc++;
        pSrc++;
    }

    *pDst++ = ',';

    /* version */
    *pDst++ = '0' + CY_FX_USB_MAJOR_VER;
    *pDst++ = '.';
    *pDst++ = '0' + CY_FX_USB_MINOR_VER;

    *pDst = 0;
    *pDst++ = '\n';

    g_TmcScpiCtx.m_MsgResponseSize = (uint32_t)(pDst - g_TmcScpiCtx.m_MsgResponseBuffer);

    return SCPI_RES_OK;
}


static scpi_result_t
Cy_TMC_Scpi_ParserCB_Trg (
    scpi_t * context
)
{
    return SCPI_RES_OK;
}


CyFxReturnStatus_t
Cy_TMC_Scpi_Init (
    void
    )
{
    scpi_interface_t* pScpiInterface;

    memset((void *)&g_TmcScpiCtx, 0, sizeof(g_TmcScpiCtx));

    pScpiInterface = &g_TmcScpiCtx.m_ScpiInterface;

    pScpiInterface->error = Cy_TMC_Scpi_CoreCB_Error;
    pScpiInterface->write = Cy_TMC_Scpi_CoreCB_Write;
    pScpiInterface->reset = Cy_TMC_Scpi_CoreCB_Reset;

    SCPI_Init(
        &g_TmcScpiCtx.m_ScpiCoreCtx,
        g_ScpiCommandArray,
        pScpiInterface,
        scpi_units_def,
        SCPI_IDN_1, SCPI_IDN_2, SCPI_IDN_3, SCPI_IDN_4,
        g_TmcScpiCtx.m_ScpiCoreBuffer, sizeof (g_TmcScpiCtx.m_ScpiCoreBuffer),
        g_TmcScpiCtx.m_ScpiErrorQueue, (int16_t)ARRAYSIZE(g_TmcScpiCtx.m_ScpiErrorQueue));

    Cy_TMC_Radar_Init();

    return CY_FX_USB_SUCCESS;
}


CyFxReturnStatus_t
Cy_TMC_Scpi_ParserCommand (
    const char* pszCommand,
    uint32_t CmdSize
)
{
    CyFxReturnStatus_t status;

    //LOG_COLOR("TMC: Command: %s\r\n", pszCommand);

    g_TmcScpiCtx.m_MsgResponseSize = 0;

    if (strncmp(pszCommand, "*IDN?", 5) == 0)
    {
        status = Cy_TMC_Scpi_ParserCB_IdnQ(&g_TmcScpiCtx.m_ScpiCoreCtx);
    }
    else
    {
        if (SCPI_Parse(&g_TmcScpiCtx.m_ScpiCoreCtx, (char*)pszCommand, CmdSize) == SCPI_RES_OK)
        {
            status = CY_FX_USB_SUCCESS;
        }
        else
        {
            LOG_ERROR("TMC: Parse Error %s", pszCommand);
            status = CY_FX_ERROR_FAILURE;
        }
    }
    return status;
}

void
Cy_TMC_Scpi_ResetParserResponse (
    void
    )
{
    g_TmcScpiCtx.m_MsgResponseSize = 0;
}


void *
Cy_TMC_Scpi_GetParserResponse (
        uint32_t* pResponseSize)
{
    *pResponseSize = g_TmcScpiCtx.m_MsgResponseSize;
    return g_TmcScpiCtx.m_MsgResponseBuffer;
}

uint8_t
Cy_TMC_Scpi_GetSTB (
    void
    )
{
    return SCPI_RegGet(&g_TmcScpiCtx.m_ScpiCoreCtx, SCPI_REG_STB);
}

size_t
Cy_TMC_Scpi_WriteRsp (
    const char* data,
    size_t len
    )
{
    DEBUG_BUF_SCPI_WRITE (data, len, 2, (uint32_t)data);

    if (len > 0 && len < 1024)
    {
        memcpy(g_TmcScpiCtx.m_MsgResponseBuffer + g_TmcScpiCtx.m_MsgResponseSize, (uint8_t*)data, len);
        g_TmcScpiCtx.m_MsgResponseSize += len;
    }
    else
    {
        LOG_ERROR("TMC: Response length is out of range [%d]", len);
    }

    return len;
}

#endif /* USB_TMC_EN */

