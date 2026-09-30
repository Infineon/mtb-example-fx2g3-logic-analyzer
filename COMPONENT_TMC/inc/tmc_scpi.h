/***************************************************************************//**
* \file tmc_scpi.h
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

#ifndef _TMC_SCPI_H_
#define _TMC_SCPI_H_

extern CyFxReturnStatus_t Cy_TMC_Scpi_Init(void);

extern CyFxReturnStatus_t Cy_TMC_Scpi_ParserCommand(const char* pszCommand, uint32_t CmdSize);

extern void Cy_TMC_Scpi_ResetParserResponse(void);

extern void* Cy_TMC_Scpi_GetParserResponse(uint32_t* pResponseSize);

extern uint8_t Cy_TMC_Scpi_GetSTB(void);

extern size_t Cy_TMC_Scpi_WriteRsp(const char* data, size_t len);

#endif /* _TMC_SCPI_H_ */

#endif /* USB_TMC_EN */

