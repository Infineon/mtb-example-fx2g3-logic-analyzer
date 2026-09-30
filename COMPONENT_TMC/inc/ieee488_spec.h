/***************************************************************************//**
* \file ieee488_spec.h
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

#ifndef _IEEE_488_SPEC_H_
#define _IEEE_488_SPEC_H_

typedef enum _IEEE488_SM_DT
{
    IEEE488_SM_DT_POF                   = 0,
    IEEE488_SM_DT_DTIS,
    IEEE488_SM_DT_DTAS,
} IEEE488_SM_DT;

typedef enum _IEEE488_SM_RL
{
    IEEE488_SM_RL_POF                   = 0,
    IEEE488_SM_RL_LOCS,
    IEEE488_SM_RL_LWLS,
    IEEE488_SM_RL_REMS,
    IEEE488_SM_RL_RWLS,
} IEEE488_SM_RL;

typedef enum _IEEE488_SM_SR
{
    IEEE488_SM_SR_POF                   = 0,
    IEEE488_SM_SR_NPRS,
    IEEE488_SM_SR_SRQS,
    IEEE488_SM_SR_APRS,
} IEEE488_SM_SR;

typedef struct _IEEE488_ASSERTION
{
    uint8_t                             OPC;
    uint8_t                             REN;
    uint8_t                             TRG;
} IEEE488_ASSERTION;

#endif /* _IEEE_488_SPEC_H_ */

#endif /* USB_TMC_EN */
