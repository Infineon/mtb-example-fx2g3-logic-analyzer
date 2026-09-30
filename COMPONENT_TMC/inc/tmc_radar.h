/***************************************************************************//**
* \file tmc_radar.h
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

#ifndef _TMC_RADAR_H_
#define _TMC_RADAR_H_

#include "types.h"

extern void Cy_TMC_Radar_Init(void);

extern scpi_result_t Cy_TMC_Radar_ParserCB_ChannelCountQ(scpi_t*);

extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01NameQ(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02NameQ(scpi_t*);

extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Mode(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Mode(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01ModeQ(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02ModeQ(scpi_t*);

extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn1Fun(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn2Fun(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn1Fun(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn2Fun(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn1FunQ(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn2FunQ(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn1FunQ(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn2FunQ(scpi_t*);

extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm1(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm2(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm1(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm2(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm1(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm2(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm1(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm2(scpi_t*);

extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm1Q(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn1Parm2Q(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm1Q(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01Eqn2Parm2Q(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm1Q(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn1Parm2Q(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm1Q(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02Eqn2Parm2Q(scpi_t*);

extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch01DataQ(scpi_t*);
extern scpi_result_t Cy_TMC_Radar_ParserCB_Ch02DataQ(scpi_t*);

#endif /* _TMC_RADAR_H_ */

#endif /* USB_TMC_EN */

