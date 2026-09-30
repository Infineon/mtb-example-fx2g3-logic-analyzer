/***************************************************************************//**
* \file usbtmc_spec.h
* \version 1.0
*
* \brief Implements the USB USB Protocol Analyzer application.
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

#ifndef _USBTMC_SPEC_H_
#define _USBTMC_SPEC_H_

#define USBTMC_BCD_VERSION                      0x0100
#define USB488_BCD_VERSION                      0x0100

/* TMC Class specific requests */
typedef enum _USBTMC_REQ
{
    USBTMC_REQ_INITIATE_ABORT_BULK_OUT          = 0x01,
    USBTMC_REQ_CHECK_ABORT_BULK_OUT_STATUS      = 0x02,
    USBTMC_REQ_INITIATE_ABORT_BULK_IN           = 0x03,
    USBTMC_REQ_CHECK_ABORT_BULK_IN_STATUS       = 0x04,
    USBTMC_REQ_INITIATE_CLEAR                   = 0x05,
    USBTMC_REQ_CHECK_CLEAR_STATUS               = 0x06,
    USBTMC_REQ_GET_CAPABILITIES                 = 0x07,
    USBTMC_REQ_INDICATOR_PULSE                  = 0x40,     /* 64         */
    USBTMC_REQ_READ_STATUS_BYTE                 = 0x80,     /* 128 (GPIB) */
    USBTMC_REQ_REN_CONTROL                      = 0xA0,     /* 160 (GPIB) */
    USBTMC_REQ_GO_TO_LOCAL                      = 0xA1,     /* 161 (GPIB) */
    USBTMC_REQ_LOCAL_LOCKOUT                    = 0xA2,     /* 162 (GPIB) */
} USBTMC_REQ;

/* TMC Class bulk-out messages */
typedef enum _USBTMC_MSGID
{
    USBTMC_MSGID_DEV_DEP_MSG_OUT                = 0x01,
    USBTMC_MSGID_REQUEST_DEV_DEP_MSG_IN         = 0x02,
    USBTMC_MSGID_RESPONSE_DEV_DEP_MSG_IN        = USBTMC_MSGID_REQUEST_DEV_DEP_MSG_IN,
    USBTMC_MSGID_VENDOR_SPECIFIC_OUT            = 0x7E,     /* 126 */
    USBTMC_MSGID_REQUEST_VENDOR_SPECIFIC_IN     = 0x7F,     /* 127 */
    USBTMC_MSGID_RESPONSE_VENDOR_SPECIFIC_IN    = USBTMC_MSGID_REQUEST_VENDOR_SPECIFIC_IN,
    USBTMC_MSGID_TRIGGER                        = 0x80,     /* 128 */
} USBTMC_MSGID;

/* TMS status codes */
typedef enum _USBTMC_STATUS
{
    USBTMC_STATUS_SUCCESS                       = 0x01,
    USBTMC_STATUS_FAILED                        = 0x80,
    USBTMC_STATUS_TRANSFER_NOT_IN_PROGRESS      = 0x81,
    USBTMC_STATUS_SPLIT_NOT_IN_PROGRESS         = 0x82,
    USBTMC_STATUS_SPLIT_IN_PROGRESS             = 0x83,
} USBTMC_STATUS;

/* USBTMC interface capabilities */
#define USBTMC_USBTMCINTF_CAP_LISTEN_ONLY       0x01
#define USBTMC_USBTMCINTF_CAP_TALK_ONLY         0x02
#define USBTMC_USBTMCINTF_CAP_INDICATOR_PULSE   0x04

/* USBTMC device capabilities */
#define USBTMC_USBTMCDEV_CAP_TERM_CHAR          0x01

/* USB488 interface capabilities */
#define USBTMC_USB488INTF_CAP_TRIGGER           0x01
#define USBTMC_USB488INTF_CAP_CONTROLS          0x02
#define USBTMC_USB488INTF_CAP_IEEE488           0x04

/* USB488 device capabilities */
#define USBTMC_USB488DEV_CAP_DT                 0x01
#define USBTMC_USB488DEV_CAP_RL                 0x02
#define USBTMC_USB488DEV_CAP_SR                 0x04
#define USBTMC_USB488DEV_CAP_SCPI               0x08

/* USBTMC transfer attributes */
#define USBTMC_ATTRIBUTE_NONE                   0x00
#define USBTMC_ATTRIBUTE_EOM                    0x01
#define USBTMC_ATTRIBUTE_TERM_CHAR_ENABLED      0x02

/* TMC interface capabilities structure. */
typedef struct T_USBTMC_CAPABILITIES
{
    USBTMC_STATUS  m_Status;                       /* D0 */
    uint8_t        rsvd_01h;                       /* D1 */
    uint16_t       m_bcdUSBTMC;                    /* D2~D3 */

    uint8_t        m_USBTMCInterfaceCapabilities;  /* D4 */
    uint8_t        m_USBTMCDeviceCapabilities;     /* D5 */
    uint8_t        rsvd_06h_0Bh[6];                /* D6~D11 */

#if (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488)
    uint16_t       m_bcdUSB488;                    /* D12~D13 */
    uint8_t        m_USB488InterfaceCapabilities;  /* D14 */
    uint8_t        m_USB488DeviceCapabilities;     /* D15 */

    uint8_t        rsvd_10h_17h[8];                /* D16~D23 */
#else
    uint8_t        rsvd_0Ch_17h[12];               /* D12~D23 */
#endif
} USBTMC_CAPABILITIES;

C_ASSERT (sizeof(USBTMC_CAPABILITIES) == 0x18);

/* Common part of TMC message header. */
typedef struct T_USBTMC_MSG_COMMON_HDR
{
    USBTMC_MSGID    m_MsgID;                        /* D0 */
    uint8_t         m_bTag;                         /* D1 */
    uint8_t         m_bTagInverse;                  /* D2 */
    uint8_t         rsvd_03h;                       /* D3 */

    uint8_t         rsvd_04h_0Bh[8];                /* D4~D11 */
} USBTMC_MSG_COMMON_HDR;

C_ASSERT(sizeof(USBTMC_MSG_COMMON_HDR) == 0x0C);

/* TMC out message header format. */
typedef struct T_USBTMC_DEV_DEP_MSG_OUT_HDR
{
    USBTMC_MSGID    m_MsgID;                        /* D0 */
    uint8_t         m_bTag;                         /* D1 */
    uint8_t         m_bTagInverse;                  /* D2 */
    uint8_t         rsvd_03h;                       /* D3 */

    uint32_t        m_TransferSize;                 /* D4~D7 */

    uint8_t         m_bmTransferAttributes;         /* D8 */
    uint8_t         rsvd_09h_0Bh[3];                /* D9~D11 */
} USBTMC_DEV_DEP_MSG_OUT_HDR;

C_ASSERT(sizeof(USBTMC_DEV_DEP_MSG_OUT_HDR) == 0x0C);

/* TMC in message header format. */
typedef struct T_USBTMC_REQUEST_DEV_DEP_MSG_IN_HDR
{
    USBTMC_MSGID    m_MsgID;                        /* D0 */
    uint8_t         m_bTag;                         /* D1 */
    uint8_t         m_bTagInverse;                  /* D2 */
    uint8_t         rsvd_03h;                       /* D3 */

    uint32_t        m_TransferSize;                 /* D4~D7 */

    uint8_t         m_bmTransferAttributes;         /* D8 */
    uint8_t         m_TermChar;                     /* D9 */
    uint8_t         rsvd_0Ah_0Bh[2];                /* D10~D11 */
} USBTMC_REQUEST_DEV_DEP_MSG_IN_HDR;

C_ASSERT(sizeof(USBTMC_REQUEST_DEV_DEP_MSG_IN_HDR) == 0x0C);

/* TMC in response header format. */
typedef struct T_USBTMC_RESPONSE_DEV_DEP_MSG_IN_HDR
{
    USBTMC_MSGID    m_MsgID;                        /* D0 */
    uint8_t         m_bTag;                         /* D1 */
    uint8_t         m_bTagInverse;                  /* D2 */
    uint8_t         rsvd_03h;                       /* D3 */

    uint32_t        m_TransferSize;                 /* D4~D7 */

    uint8_t         m_bmTransferAttributes;         /* D8 */
    uint8_t         m_TermChar;                     /* D9 */
    uint8_t         rsvd_0Ah_0Bh[2];                /* D10~D11 */
} USBTMC_RESPONSE_DEV_DEP_MSG_IN_HDR;

C_ASSERT(sizeof(USBTMC_RESPONSE_DEV_DEP_MSG_IN_HDR) == 0x0C);

/* TMC notification format. */
typedef struct T_USBTMC_NOTIFY_DATA
{
    uint8_t         m_Notify1;                      /* D0 */
    uint8_t         m_Notify2;                      /* D1 */
} USBTMC_NOTIFY_DATA;

C_ASSERT(sizeof(USBTMC_NOTIFY_DATA) == 2);

#endif /* _USBTMC_SPEC_H_ */

#endif /* USB_TMC_EN */

