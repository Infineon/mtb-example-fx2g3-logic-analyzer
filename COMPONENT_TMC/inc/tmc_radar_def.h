/***************************************************************************//**
* \file tmc_radar_def.h
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

#if USB_TMC_EN

#ifndef _TMC_RADAR_DEF_H_
#define _TMC_RADAR_DEF_H_

typedef enum
{
    CYFX_TMC_RADAR_CH_EQN_TYPE_X            = 0,
    CYFX_TMC_RADAR_CH_EQN_TYPE_Y,
    CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT
} cy_en_tmc_radar_ch_eqn_type_t;

typedef enum
{
    CYFX_TMC_RADAR_CH_FUN_TYPE_SIN          = 0,
    CYFX_TMC_RADAR_CH_FUN_TYPE_COS,
    CYFX_TMC_RADAR_CH_FUN_TYPE_COUNT
} cy_en_tmc_radar_ch_fun_type_t;

typedef struct cy_stc_tmc_radar_ch_equation
{
    cy_en_tmc_radar_ch_fun_type_t m_FunctionType;
    int                           m_ParameterValueArray[2];
} cy_stc_tmc_radar_ch_equation;

typedef struct cy_stc_tmc_radar_ch_formula
{
    cy_stc_tmc_radar_ch_equation m_EquationArray[CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT];
} cy_stc_tmc_radar_ch_formula;

typedef struct cy_stc_tmc_radar_ch_result
{
    float m_ResultArray[CYFX_TMC_RADAR_CH_EQN_TYPE_COUNT];
} cy_stc_tmc_radar_ch_result;

#define FX_RADAR_CHANNEL_COUNT_Q           "RADAr:CHANnel:COUNt?"
#define FX_RADAR_CHXX_NAME_Q               "RADAr:CH%02d:NAME?"

#define FX_RADAR_CH01_NAME_Q               "RADAr:CH01:NAME?"
#define FX_RADAR_CH02_NAME_Q               "RADAr:CH02:NAME?"

#define FX_RADAR_CHXX_MODE                 "RADAr:CH%02d:MODE %s"
#define FX_RADAR_CH01_MODE                 "RADAr:CH01:MODE"
#define FX_RADAR_CH02_MODE                 "RADAr:CH02:MODE"

#define FX_RADAR_CHXX_MODE_Q               "RADAr:CH%02d:MODE?"
#define FX_RADAR_CH01_MODE_Q               "RADAr:CH01:MODE?"
#define FX_RADAR_CH02_MODE_Q               "RADAr:CH02:MODE?"

#define FX_RADAR_CHXX_EQNX_FUN             "RADAr:CH%02d:EQN%d:FUN %d"
#define FX_RADAR_CH01_EQN1_FUN             "RADAr:CH01:EQN1:FUN"
#define FX_RADAR_CH01_EQN2_FUN             "RADAr:CH01:EQN2:FUN"
#define FX_RADAR_CH02_EQN1_FUN             "RADAr:CH02:EQN1:FUN"
#define FX_RADAR_CH02_EQN2_FUN             "RADAr:CH02:EQN2:FUN"

#define FX_RADAR_CHXX_EQNX_FUN_Q           "RADAr:CH%02d:EQN%d:FUN?"
#define FX_RADAR_CH01_EQN1_FUN_Q           "RADAr:CH01:EQN1:FUN?"
#define FX_RADAR_CH01_EQN2_FUN_Q           "RADAr:CH01:EQN2:FUN?"
#define FX_RADAR_CH02_EQN1_FUN_Q           "RADAr:CH02:EQN1:FUN?"
#define FX_RADAR_CH02_EQN2_FUN_Q           "RADAr:CH02:EQN2:FUN?"

#define FX_RADAR_CHXX_EQNX_PARX            "RADAr:CH%02d:EQN%d:PAR%d %d"
#define FX_RADAR_CH01_EQN1_PAR1            "RADAr:CH01:EQN1:PAR1"
#define FX_RADAR_CH01_EQN1_PAR2            "RADAr:CH01:EQN1:PAR2"
#define FX_RADAR_CH01_EQN2_PAR1            "RADAr:CH01:EQN2:PAR1"
#define FX_RADAR_CH01_EQN2_PAR2            "RADAr:CH01:EQN2:PAR2"
#define FX_RADAR_CH02_EQN1_PAR1            "RADAr:CH02:EQN1:PAR1"
#define FX_RADAR_CH02_EQN1_PAR2            "RADAr:CH02:EQN1:PAR2"
#define FX_RADAR_CH02_EQN2_PAR1            "RADAr:CH02:EQN2:PAR1"
#define FX_RADAR_CH02_EQN2_PAR2            "RADAr:CH02:EQN2:PAR2"

#define FX_RADAR_CHXX_EQNX_PARX_Q          "RADAr:CH%02d:EQN%d:PAR%d?"
#define FX_RADAR_CH01_EQN1_PAR1_Q          "RADAr:CH01:EQN1:PAR1?"
#define FX_RADAR_CH01_EQN1_PAR2_Q          "RADAr:CH01:EQN1:PAR2?"
#define FX_RADAR_CH01_EQN2_PAR1_Q          "RADAr:CH01:EQN2:PAR1?"
#define FX_RADAR_CH01_EQN2_PAR2_Q          "RADAr:CH01:EQN2:PAR2?"
#define FX_RADAR_CH02_EQN1_PAR1_Q          "RADAr:CH02:EQN1:PAR1?"
#define FX_RADAR_CH02_EQN1_PAR2_Q          "RADAr:CH02:EQN1:PAR2?"
#define FX_RADAR_CH02_EQN2_PAR1_Q          "RADAr:CH02:EQN2:PAR1?"
#define FX_RADAR_CH02_EQN2_PAR2_Q          "RADAr:CH02:EQN2:PAR2?"

#define FX_RADAR_CHXX_DATA_Q               "RADAr:CH%02d:DATA?"
#define FX_RADAR_CH01_DATA_Q               "RADAr:CH01:DATA?"
#define FX_RADAR_CH02_DATA_Q               "RADAr:CH02:DATA?"

#endif /* _TMC_RADAR_DEF_H_ */

#endif /* USB_TMC_EN */

