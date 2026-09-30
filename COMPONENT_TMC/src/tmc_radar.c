/***************************************************************************//**
* \file tmc_radar.c
* \version 1.0
*
* \brief Implements interface of TMC with radar application.
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

#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "usb_app.h"
#include "tmc_radar.h"
#include "tmc_radar_def.h"
#include "tmc_radar_tbl.h"

/* Debug log interface for RADAR function.
 * Not implemented by default. Can be mapped to Cy_Debug_AddToLog function.
 */
#define DEBUG_MSG_RADAR_PARSER(...)
#define DEBUG_MSG_RADAR_CALC(...)
#define DEBUG_MSG_RADAR_FORMULA(_ch_)

typedef enum cy_en_tmc_radar_ch_type
{
    CYFX_TMC_RADAR_CH_TYPE_ANT    = 0,
    CYFX_TMC_RADAR_CH_TYPE_BEE,
    CYFX_TMC_RADAR_CH_TYPE_COUNT,
} cy_en_tmc_radar_ch_type;

typedef struct cy_stc_tmc_radar_ch_ctx
{
    char                         m_Name[4];
    cy_stc_tmc_radar_ch_formula  m_Formula;
    const float*                 m_pConvTable[CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT];
    int                          m_CurrentAngle;
    uint8_t                      m_bResetParam;
    uint8_t                      m_Running;
    uint8_t                      rsvd[2];
} cy_stc_tmc_radar_ch_ctx;

typedef struct cy_stc_tmc_radar_ctx
{
    cy_stc_tmc_radar_ch_ctx m_ChannelCtxArray[CYFX_TMC_RADAR_CH_TYPE_COUNT];
    char                    m_ReportBuffer[128];
} cy_stc_tmc_radar_ctx;

/* Global RADAR context structure */
static cy_stc_tmc_radar_ctx g_TmcRadarCtx __attribute__((aligned(32)));

static void
Cy_TMC_Radar_UpdateFunctionRoutine (
    uint8_t ChannelIndex,
    uint8_t EquationIndex
    )
{
    cy_stc_tmc_radar_ch_ctx *pChannelCtx;

    if ((ChannelIndex < CYFX_TMC_RADAR_CH_TYPE_COUNT) && (EquationIndex < CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT))
    {
        pChannelCtx = g_TmcRadarCtx.m_ChannelCtxArray + ChannelIndex;

        if (pChannelCtx->m_Formula.m_EquationArray[EquationIndex].m_FunctionType == CYFX_TMC_RADAR_CH_FUN_TYPE_COS)
        {
            pChannelCtx->m_pConvTable[EquationIndex] = g_CosConvTable;
        }
        else
        {
            pChannelCtx->m_pConvTable[EquationIndex] = g_SinConvTable;
        }

        DEBUG_MSG_RADAR_FORMULA(ChannelIndex);
        pChannelCtx->m_bResetParam = 1;
    }
}


static scpi_result_t
Cy_TMC_Radar_UpdateFunctionType (
    scpi_t* context,
    uint8_t ChannelIndex,
    uint8_t EquationIndex
    )
{
    cy_stc_tmc_radar_ch_ctx* pChannelCtx = g_TmcRadarCtx.m_ChannelCtxArray + ChannelIndex;
    int32_t FunctionIndex = 0;

    SCPI_ParamInt(context, &FunctionIndex, TRUE);
    if ((FunctionIndex < 0) || (FunctionIndex >= CYFX_TMC_RADAR_CH_FUN_TYPE_COUNT))
    {
        FunctionIndex = 0;
    }

    pChannelCtx->m_Formula.m_EquationArray[EquationIndex].m_FunctionType = (cy_en_tmc_radar_ch_fun_type_t)FunctionIndex;

    if (FunctionIndex == CYFX_TMC_RADAR_CH_FUN_TYPE_COS)
    {
        pChannelCtx->m_pConvTable[EquationIndex] = g_CosConvTable;
    }
    else
    {
        pChannelCtx->m_pConvTable[EquationIndex] = g_SinConvTable;
    }

    DEBUG_MSG_RADAR_FORMULA(ChannelIndex);
    pChannelCtx->m_bResetParam = 1;

    return SCPI_RES_OK;
}


static scpi_result_t
Cy_TMC_Radar_UpdateEquationParameter (
    scpi_t* context,
    uint8_t ChannelIndex,
    uint8_t EquationIndex,
    uint8_t ParameterIndex
    )
{
    cy_stc_tmc_radar_ch_ctx* pChannelCtx = g_TmcRadarCtx.m_ChannelCtxArray + ChannelIndex;
    int32_t ParameterValue = 0;

    SCPI_ParamInt(context, &ParameterValue, TRUE);
    pChannelCtx->m_Formula.m_EquationArray[EquationIndex].m_ParameterValueArray[ParameterIndex] = ParameterValue;

    DEBUG_MSG_RADAR_FORMULA(ChannelIndex);
    pChannelCtx->m_bResetParam = 1;

    return SCPI_RES_OK;
}

static int fstrlen(char *p)
{
    int i=0;
    while (*p != '\0') {
        i++;
        p++;
    }
    return i;
}

static int normalize(float *val) {
    int exponent = 0;
    float value = *val;

    while (value >= 1.0) {
        value /= 10.0;
        ++exponent;
    }

    while (value < 0.1) {
        value *= 10.0;
        --exponent;
    }
    *val = value;
    return exponent;
}

#define FLOAT_PRECISION (8)

/* Carries out a fixed conversion of a float value to a string, with a precision of 9 decimal digits.
 * Values with absolute values less than 0.000001 are rounded to 0.0
 * Note: this blindly assumes that the buffer will be large enough to hold the largest possible result.
 * The largest value we expect is an IEEE 754 float precision real, with maximum magnitude of approximately
 * e+38. We need a buffer of 64 bytes to store the converted value.
 */
static void float_to_string (char *buffer, float value) {

    int exponent = 0;
    int places = 0;

    if (value == 0.0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    if (value < 0.0) {
        *buffer++ = '-';
        value = -value;
    }

    exponent = normalize(&value);

    while (exponent > 0) {
        int digit = value * 10;
        *buffer++ = digit + '0';
        value = value * 10 - digit;
        ++places;
        --exponent;
    }

    if (places == 0)
        *buffer++ = '0';

    *buffer++ = '.';
    if (exponent >= 0 && places >= FLOAT_PRECISION) {
        *buffer++ = '0';
    }

    while (exponent < 0 && places < FLOAT_PRECISION) {
        *buffer++ = '0';
        ++exponent;
        ++places;
    }

    while (places < FLOAT_PRECISION) {
        int digit = value * 10.0;
        *buffer++ = digit + '0';
        value = value * 10.0 - digit;
        ++places;
    }

    *buffer = '\0';
}

extern size_t Cy_TMC_Scpi_WriteRsp (const char* data, size_t len);

static scpi_result_t
Cy_TMC_Radar_CalculateChannel (
    scpi_t* context,
    uint8_t ch
    )
{
    cy_stc_tmc_radar_ch_result Result;
    cy_stc_tmc_radar_ch_ctx* pChannelCtx;
    cy_stc_tmc_radar_ch_equation* pEquation;
    int angle = 0;
    int buflen = 0;
    uint8_t eqn;

    pChannelCtx = g_TmcRadarCtx.m_ChannelCtxArray + ch;

    if (pChannelCtx->m_Running != 0)
    {
        if (pChannelCtx->m_bResetParam)
        {
            pChannelCtx->m_bResetParam = 0;

            pChannelCtx->m_CurrentAngle = 0;
        }

        for (eqn = 0, pEquation = pChannelCtx->m_Formula.m_EquationArray; eqn < CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT;
                eqn++, pEquation++)
        {
            angle = (pEquation->m_ParameterValueArray[1] * pChannelCtx->m_CurrentAngle) % 360;
            Result.m_ResultArray[eqn] = (float)pEquation->m_ParameterValueArray[0] *
                pChannelCtx->m_pConvTable[eqn][angle];
        }

        pChannelCtx->m_CurrentAngle += 3;
    }
    else
    {
        for (eqn = 0; eqn < CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT; eqn++)
        {
            Result.m_ResultArray[eqn] = 0;
        }
    }

    float_to_string(g_TmcRadarCtx.m_ReportBuffer, Result.m_ResultArray[0]);
    buflen = fstrlen(g_TmcRadarCtx.m_ReportBuffer);
    g_TmcRadarCtx.m_ReportBuffer[buflen]=',';
    buflen++;

    float_to_string(g_TmcRadarCtx.m_ReportBuffer + buflen, Result.m_ResultArray[1]);
    buflen += fstrlen(g_TmcRadarCtx.m_ReportBuffer + buflen);

    Cy_TMC_Scpi_WriteRsp((const char*)g_TmcRadarCtx.m_ReportBuffer, (size_t)buflen);
    return SCPI_RES_OK;
}

void
Cy_TMC_Radar_Init (
    void
    )
{
    cy_stc_tmc_radar_ch_ctx* pChannelCtx;
    cy_stc_tmc_radar_ch_equation* pRadarEquation;
    uint8_t ch;
    uint8_t eqn;

    memset((void *)&g_TmcRadarCtx, 0, sizeof(g_TmcRadarCtx));

    /* ant */
    pChannelCtx = g_TmcRadarCtx.m_ChannelCtxArray + CYFX_TMC_RADAR_CH_TYPE_ANT;

    pChannelCtx->m_Name[0] = 'A';
    pChannelCtx->m_Name[1] = 'N';
    pChannelCtx->m_Name[2] = 'T';

    pRadarEquation = pChannelCtx->m_Formula.m_EquationArray;

    pRadarEquation->m_FunctionType = CYFX_TMC_RADAR_CH_FUN_TYPE_SIN;
    pRadarEquation->m_ParameterValueArray[0] = 3.f;
    pRadarEquation->m_ParameterValueArray[1] = 6.f;

    pRadarEquation++;

    pRadarEquation->m_FunctionType = CYFX_TMC_RADAR_CH_FUN_TYPE_COS;
    pRadarEquation->m_ParameterValueArray[0] = 4.f;
    pRadarEquation->m_ParameterValueArray[1] = 1.f;

    /* bee */
    pChannelCtx = g_TmcRadarCtx.m_ChannelCtxArray + CYFX_TMC_RADAR_CH_TYPE_BEE;

    pChannelCtx->m_Name[0] = 'B';
    pChannelCtx->m_Name[1] = 'E';
    pChannelCtx->m_Name[2] = 'E';

    pRadarEquation = pChannelCtx->m_Formula.m_EquationArray;

    pRadarEquation->m_FunctionType = CYFX_TMC_RADAR_CH_FUN_TYPE_SIN;
    pRadarEquation->m_ParameterValueArray[0] = 6.f;
    pRadarEquation->m_ParameterValueArray[1] = 2.f;

    pRadarEquation++;

    pRadarEquation->m_FunctionType = CYFX_TMC_RADAR_CH_FUN_TYPE_COS;
    pRadarEquation->m_ParameterValueArray[0] = 2.f;
    pRadarEquation->m_ParameterValueArray[1] = 3.f;

    for (ch = 0; ch < CYFX_TMC_RADAR_CH_TYPE_COUNT; ch++)
    {
        for (eqn = 0; eqn < CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT; eqn++)
        {
            Cy_TMC_Radar_UpdateFunctionRoutine(ch, eqn);
        }
    }

    LOG_COLOR("\r\n=================Cy_TMC_Radar_Init Done===================\r\n");
}


scpi_result_t
Cy_TMC_Radar_ParserCB_ChannelCountQ (
        scpi_t* context
        )
{
    DEBUG_MSG_RADAR_PARSER ("\r\n  %s", FX_RADAR_CHANNEL_COUNT_Q);

    SCPI_ResultInt32 (context, ARRAYSIZE(g_TmcRadarCtx.m_ChannelCtxArray));

    return SCPI_RES_OK;
}


scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01NameQ (
        scpi_t* context
        )
{
    DEBUG_MSG_RADAR_PARSER ("\r\n  %s", FX_RADAR_CH01_NAME_Q);

    SCPI_ResultText (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Name);

    return SCPI_RES_OK;
}


scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02NameQ (
        scpi_t* context
        )
{
    DEBUG_MSG_RADAR_PARSER ("\r\n  %s", FX_RADAR_CH02_NAME_Q);

    SCPI_ResultText (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Name);

    return SCPI_RES_OK;
}


scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Mode (
        scpi_t* context
        )
{
    scpi_bool_t new_VAL = 0;

    DEBUG_MSG_RADAR_PARSER ("\r\n  %s", FX_RADAR_CH01_MODE);

    SCPI_ParamBool(context, &new_VAL, TRUE);

    g_TmcRadarCtx.m_ChannelCtxArray[0].m_Running = new_VAL;
    g_TmcRadarCtx.m_ChannelCtxArray[0].m_bResetParam = 1;

    return SCPI_RES_OK;
}


scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Mode (
        scpi_t* context
        )
{
    scpi_bool_t new_VAL;

    DEBUG_MSG_RADAR_PARSER ("\r\n  %s", FX_RADAR_CH02_MODE);

    SCPI_ParamBool(context, &new_VAL, TRUE);

    g_TmcRadarCtx.m_ChannelCtxArray[1].m_Running = new_VAL;
    g_TmcRadarCtx.m_ChannelCtxArray[1].m_bResetParam = 1;

    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01ModeQ (
        scpi_t* context)
{
    SCPI_ResultBool (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Running);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02ModeQ (
        scpi_t* context)
{
    SCPI_ResultBool (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Running);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn1Fun (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateFunctionType (context, 0, 0);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn2Fun (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateFunctionType (context, 0, 1);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn1Fun (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateFunctionType (context, 1, 0);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn2Fun (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateFunctionType (context, 1, 1);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn1FunQ (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Formula.m_EquationArray[0].m_FunctionType);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn2FunQ (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Formula.m_EquationArray[1].m_FunctionType);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn1FunQ (scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Formula.m_EquationArray[0].m_FunctionType);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn2FunQ (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Formula.m_EquationArray[1].m_FunctionType);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm1 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 0, 0, 0);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm2 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 0, 0, 1);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm1 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 0, 1, 0);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm2 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 0, 1, 1);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm1 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 1, 0, 0);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm2 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 1, 0, 1);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm1 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 1, 1, 0);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm2 (
        scpi_t* context)
{
    return Cy_TMC_Radar_UpdateEquationParameter(context, 1, 1, 1);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm1Q (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Formula.m_EquationArray[0].m_ParameterValueArray[0]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm2Q (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Formula.m_EquationArray[0].m_ParameterValueArray[1]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm1Q (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Formula.m_EquationArray[1].m_ParameterValueArray[0]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm2Q (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[0].m_Formula.m_EquationArray[1].m_ParameterValueArray[1]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm1Q (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Formula.m_EquationArray[0].m_ParameterValueArray[0]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm2Q (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Formula.m_EquationArray[0].m_ParameterValueArray[1]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm1Q (scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Formula.m_EquationArray[1].m_ParameterValueArray[0]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm2Q (
        scpi_t* context)
{
    SCPI_ResultInt32 (context, g_TmcRadarCtx.m_ChannelCtxArray[1].m_Formula.m_EquationArray[1].m_ParameterValueArray[1]);
    return SCPI_RES_OK;
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch01DataQ (
        scpi_t* context)
{
    return Cy_TMC_Radar_CalculateChannel(context, 0);
}

scpi_result_t
Cy_TMC_Radar_ParserCB_Ch02DataQ (
        scpi_t* context)
{
    return Cy_TMC_Radar_CalculateChannel(context, 1);
}

#endif /* USB_TMC_EN */

