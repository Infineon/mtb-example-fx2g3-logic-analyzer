/***************************************************************************//**
* \file usb_descriptors.c
* \version 1.0
*
* \brief Defines the USB descriptors used in the USB Logic Analyzer application.
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

#include "cy_pdl.h"
#include "usb_app.h"

/* Standard device descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSB20DeviceDscr[] =
{
    0x12,                               /* Descriptor size */
    0x01,                               /* Device descriptor type */
    0x00,0x02,                          /* USB 2.00 */
    0x00,                               /* Device class*/
    0x00,                               /* Device sub-class */
    0x00,                               /* Device protocol */
    0x40,                               /* Maxpacket size for EP0 : 64 bytes */
#if USB_TMC_EN
    CY_USB_GET_LSB(CY_FX_USB_VID),      /* Vendor ID */
    CY_USB_GET_MSB(CY_FX_USB_VID),
    CY_USB_GET_LSB(CY_FX_USB_PID_HS),   /* Product ID */
    CY_USB_GET_MSB(CY_FX_USB_PID_HS),
#else
    0xB4,0x04,                          /* Vendor ID */
    0x07,0x49,                          /* Product ID */
#endif /* USB_TMC_EN */
    0x00,0x00,                          /* Device release number */
    0x01,                               /* Manufacture string index */
    0x02,                               /* Product string index */
#if USB_TMC_EN
    CY_FX_USBTMC_STR_IDX_SN,            /* Serial number string index */
#else /* USB_TMC_EN */
    0,
#endif /* USB_TMC_EN */
    0x01                                /* Number of configurations */
};

/* Binary device object store descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBBOSDscr[32] =
{
    0x05,                               /* Descriptor size */
    0x0F,                               /* Device descriptor type */
    0x0C,0x00,                          /* Length of this descriptor and all sub descriptors */
    0x01,                               /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                               /* Descriptor size */
    0x10,                               /* Device capability type descriptor */
    0x02,                               /* USB 2.0 extension capability type */
    0x1E,0x64,0x00,0x00,                /* Supported device level features: LPM support, BESL supported,
                                           Baseline BESL=400 us, Deep BESL=1000 us. */
};


/* Standard high speed configuration descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBHSConfigDscr[] =
{
    /* Configuration descriptor */
    0x09,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_CFG,                       /* Configuration descriptor type */
    CY_USB_GET_LSB(USB_CFG_DESC_LEN_HS),        /* Length of this descriptor and all sub descriptors */
    CY_USB_GET_MSB(USB_CFG_DESC_LEN_HS),
    USB_TMC_EN+USB_LOGIC_ANALYZER_EN,           /* Number of interfaces */
    0x01,                                       /* Configuration number */
    0x00,                                       /* Configuration string index */
    0x80,                                       /* Config characteristics - Bus powered */
    0x32,                                       /* Max power consumption of device (in 8mA unit) : 400mA */

#if USB_TMC_EN
    /* Test and Measurement Class Interface descriptor */
    0x09,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_INTF,                      /* Interface Descriptor type */
    0,                                          /* Interface number */
    0x00,                                       /* Alternate setting number */
    TMC_ENDPOINT_COUNT,                         /* Number of endpoints */
    USBTMC_CLASS,                               /* Interface class : Test and Measurement Class (TMC) */
    USBTMC_SUBCLASS,                            /* Interface sub class */
    USBTMC_PROTOCOL,                            /* Interface protocol code */
    CY_FX_USBTMC_STR_IDX_PRODUCT,               /* Interface descriptor string index */

    /* Endpoint Descriptor(BULK-PRODUCER) */
    0x07,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_ENDP,                      /* Endpoint descriptor type */
    TMC_MSG_OUT_ENDPOINT,                       /* Endpoint address and description */
    0x02,                                       /* BULK endpoint type */
    0x00,0x02,                                  /* Max packet size = 512 bytes */
    0x00,                                       /* Servicing interval for data transfers */

    /* Endpoint Descriptor(BULK- CONSUMER) */
    0x07,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_ENDP,                      /* Endpoint descriptor type */
    0x80 | TMC_MSG_IN_ENDPOINT,                 /* Endpoint address and description */
    0x02,                                       /* Bulk endpoint type */
    0x00,0x02,                                  /* Max packet size = 512 bytes */
    0x00,                                       /* Servicing interval for data transfers */

#if TMC_SUPPORT_INTERRUPT
    /* Endpoint Descriptor(Interrupt) */
    0x07,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_ENDP,                      /* Endpoint descriptor type */
    0x80 | TMC_MSG_INTR_ENDPOINT,               /* Endpoint address and description */
    0x03,                                       /* Interrupt endpoint type */
    0x40,0x00,                                  /* Max packet size = 64 bytes */
    0x02,                                       /* Servicing interval for data transfers */
#endif /* TMC_SUPPORT_INTERRUPT */
#endif /* USB_TMC_EN */

#if USB_LOGIC_ANALYZER_EN
    /* Interface descriptor */
    0x09,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_INTF,                      /* Interface Descriptor type */
#if USB_TMC_EN
    0x01,                                       /* Interface number */
#else
    0x00,                                       /* Interface number */
#endif /* USB_TMC_EN */
    0x00,                                       /* Alternate setting number */
    0x01,                                       /* Number of end points */
    0xFF,                                       /* Interface class */
    0x00,                                       /* Interface sub class */
    0x00,                                       /* Interface protocol code */
    0x00,                                       /* Interface descriptor string index */

    /* Endpoint descriptor for consumer EP */
    0x07,                                       /* Descriptor size */
    0x05,                                       /* Endpoint descriptor type */
    0x80 | LOGIC_ANALYZER_BULK_IN_ENDPOINT_1,   /* Endpoint address and description */
    0x02,                                       /* Bulk endpoint type */
    0x00,0x02,                                  /* Max packet size = 1024 bytes */
    0x00,                                       /* Servicing interval for data transfers : 0 for Bulk */
#endif /* USB_LOGIC_ANALYZER_EN */
};

/* Standard device qualifier descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBDeviceQualDscr[] =
{
    0x0A,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_DEVICE_QUALIFIER,          /* Device qualifier descriptor type */
    0x00,0x02,                                  /* USB 2.0 */
    0x00,                                       /* Device class */
    0x00,                                       /* Device sub-class */
    0x00,                                       /* Device protocol */
    0x40,                                       /* Max packet size for EP0 : 64 bytes */
    0x01,                                       /* Number of configurations */
    0x00                                        /* Reserved */
};

/* Standard full speed configuration descriptor: full speed is not supported. */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBFSConfigDscr[] =
{
    /* Configuration descriptor */
    0x09,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_CFG,                       /* Configuration descriptor type */
    CY_USB_GET_LSB(USB_CFG_DESC_LEN_HS),        /* Length of this descriptor and all sub descriptors */
    CY_USB_GET_MSB(USB_CFG_DESC_LEN_HS),
    USB_TMC_EN+USB_LOGIC_ANALYZER_EN,           /* Number of interfaces */
    0x01,                                       /* Configuration number */
    0x00,                                       /* Configuration string index */
    0x80,                                       /* Config characteristics - Bus powered */
    0x32,                                       /* Max power consumption of device (in 8mA unit) : 400mA */

#if USB_TMC_EN
    /* Test and Measurement Class Interface descriptor */
    0x09,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_INTF,                      /* Interface Descriptor type */
    0,                                          /* Interface number */
    0x00,                                       /* Alternate setting number */
    TMC_ENDPOINT_COUNT,                         /* Number of endpoints */
    USBTMC_CLASS,                               /* Interface class : Test and Measurement Class (TMC) */
    USBTMC_SUBCLASS,                            /* Interface sub class */
    USBTMC_PROTOCOL,                            /* Interface protocol code */
    CY_FX_USBTMC_STR_IDX_PRODUCT,               /* Interface descriptor string index */

    /* Endpoint Descriptor(BULK-PRODUCER) */
    0x07,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_ENDP,                      /* Endpoint descriptor type */
    TMC_MSG_OUT_ENDPOINT,                       /* Endpoint address and description */
    0x02,                                       /* BULK endpoint type */
    0x40,0x00,                                  /* Max packet size = 64 bytes */
    0x00,                                       /* Servicing interval for data transfers */

    /* Endpoint Descriptor(BULK- CONSUMER) */
    0x07,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_ENDP,                      /* Endpoint descriptor type */
    0x80 | TMC_MSG_IN_ENDPOINT,                 /* Endpoint address and description */
    0x02,                                       /* Bulk endpoint type */
    0x40,0x00,                                  /* Max packet size = 64 bytes */
    0x00,                                       /* Servicing interval for data transfers */

#if TMC_SUPPORT_INTERRUPT
    /* Endpoint Descriptor(Interrupt) */
    0x07,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_ENDP,                      /* Endpoint descriptor type */
    0x80 | TMC_MSG_INTR_ENDPOINT,               /* Endpoint address and description */
    0x03,                                       /* Interrupt endpoint type */
    0x40,0x00,                                  /* Max packet size = 64 bytes */
    0x02,                                       /* Servicing interval for data transfers */
#endif /* TMC_SUPPORT_INTERRUPT */
#endif /* USB_TMC_EN */

#if USB_LOGIC_ANALYZER_EN
    /* Interface descriptor */
    0x09,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_INTF,                      /* Interface Descriptor type */
#if USB_TMC_EN
    0x01,                                       /* Interface number */
#else
    0x00,                                       /* Interface number */
#endif /* USB_TMC_EN */
    0x00,                                       /* Alternate setting number */
    0x01,                                       /* Number of end points */
    0xFF,                                       /* Interface class */
    0x00,                                       /* Interface sub class */
    0x00,                                       /* Interface protocol code */
    0x00,                                       /* Interface descriptor string index */

    /* Endpoint descriptor for consumer EP */
    0x07,                                       /* Descriptor size */
    0x05,                                       /* Endpoint descriptor type */
    0x80 | LOGIC_ANALYZER_BULK_IN_ENDPOINT_1,   /* Endpoint address and description */
    0x02,                                       /* Bulk endpoint type */
    0x40,0x00,                                  /* Max packet size = 64 bytes */
    0x00,                                       /* Servicing interval for data transfers : 0 for Bulk */
#endif /* USB_LOGIC_ANALYZER_EN */
};

/* Standard language ID string descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBStringLangIDDscr[] =
{
    0x04,                                       /* Descriptor size */
    0x03,                                       /* Device descriptor type */
    0x09,0x04                                   /* Language ID supported */
};

/* Standard manufacturer string descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBManufactureDscr[] =
{
    0x12,                                       /* Descriptor size */
    0x03,                                       /* Device descriptor type */
    'I', 0x00,
    'N', 0x00,
    'F', 0x00,
    'I', 0x00,
    'N', 0x00,
    'E', 0x00,
    'O', 0x00,
    'N', 0x00
};


/* Serial Number string descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBSNumberDscr[32] =
{
#if USB_TMC_EN
    0x12,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_STR,                       /* Device descriptor type */
    'T',0x00,
    'M',0x00,
    'C',0x00,
    '-',0x00,
    '1',0x00,
    '2',0x00,
    '3',0x00,
    '4',0x00,
#else
    0x1A,                                       /* Descriptor size */
    CY_USB_DSCR_TYPE_STR,                       /* Device descriptor type */
    'S',0x00,
    'N',0x00,
    ':',0x00,
    '0',0x00,
    '0',0x00,
    '0',0x00,
    '0',0x00,
    '0',0x00,
    '0',0x00,
    '0',0x00,
    '0',0x00,
    '0',0x00
#endif /* USB_TMC_EN */
};


/* Standard product string descriptor */
USB_DESC_ATTRIBUTES uint8_t CyFxUSBProductDscr[] =
{
#if USB_TMC_EN
#if (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488)
    44,     /* Descriptor size */
    CY_USB_DSCR_TYPE_STR,                       /* Device descriptor type */
    'F',0x00,
    'X',0x00,
    '2',0x00,
    'G',0x00,
    '3',0x00,
    ' ',0x00,
    'U',0x00,
    'S',0x00,
    'B',0x00,
    ' ',0x00,
    'G',0x00,
    'P',0x00,
    'I',0x00,
    'B',0x00,
    ' ',0x00,
    'D',0x00,
    'e',0x00,
    'v',0x00,
    'i',0x00,
    'c',0x00,
    'e',0x00,
#else
    42,                                         /* Descriptor size */
    CY_USB_DSCR_TYPE_STR,                       /* Device descriptor type */
    'F',0x00,
    'X',0x00,
    '2',0x00,
    'G',0x00,
    '3',0x00,
    ' ',0x00,
    'U',0x00,
    'S',0x00,
    'B',0x00,
    ' ',0x00,
    'T',0x00,
    'M',0x00,
    'C',0x00,
    ' ',0x00,
    'D',0x00,
    'e',0x00,
    'v',0x00,
    'i',0x00,
    'c',0x00,
    'e',0x00,
#endif /* (USBTMC_PROTOCOL == TMC_PROTOCOL_IEEE488) */
#else
    56,
    CY_USB_DSCR_TYPE_STR,
    'E',  0x00,
    'Z',  0x00,
    '-',  0x00,
    'U',  0x00,
    'S',  0x00,
    'B',  0x00,
    ' ',  0x00,
    'F',  0x00,
    'X',  0x00,
    '2',  0x00,
    'G',  0x00,
    '3',  0x00,
    ' ',  0x00,
    'L',  0x00,
    'O',  0x00,
    'G',  0x00,
    'I',  0x00,
    'C',  0x00,
    ' ',  0x00,
    'A',  0x00,
    'N',  0x00,
    'A',  0x00,
    'L',  0x00,
    'Y',  0x00,
    'Z',  0x00,
    'E',  0x00,
    'R',  0x00
#endif /* USB_TMC_EN */
};

/* MS OS String Descriptor */
USB_DESC_ATTRIBUTES uint8_t glOsString[] =
{
    0x12,                       /* Length. */
    0x03,                       /* Type - string. */
    'M', 0x00, 'S', 0x00, 'F', 0x00, 'T', 0x00, '1', 0x00, '0', 0x00, '0', 0x00,
                                /* Signature. */
    MS_VENDOR_CODE,             /* MS vendor code. */
    0x00                        /* Padding. */
};

USB_DESC_ATTRIBUTES uint8_t glOsCompatibilityId[] =
{
    /* Header */
    0x28, 0x00, 0x00, 0x00,                             /* Length of descriptor */
    0x00, 0x01,                                         /* BCD version */
    0x04, 0x00,                                         /* Index: 4 - Compatibility ID */
    0x01,                                               /* Number of functions: 1 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,           /* Reserved. */

    /* First Interface */
#if USB_TMC_EN
    0x01,                                               /* Interface number */
#else
    0x00,                                               /* Interface number */
#endif
    0x01,                                               /* Reserved: Need to be 1. */
    0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00,     /* Compatible ID: 'WINUSB' */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* Sub-Compatible ID: None */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00                  /* Reserved */
};

USB_DESC_ATTRIBUTES uint8_t glOsFeature[] =
{
    /* Header */
    0x8E, 0x00, 0x00, 0x00,                             /* Length. */
    0x00, 0x01,                                         /* BCD version. 1.0 as per MS */
    0x05, 0x00,                                         /* Index: 5 for OS feature descriptor */
    0x01, 0x00,                                         /* Number of properties: 1 */

    /* Property section. */
    0x84, 0x00, 0x00, 0x00,                             /* Length */
    0x01, 0x00, 0x00, 0x00,                             /* dwPropertyDataType: REG_SZ */
    0x28, 0x00,                                         /* wPropertyNameLength: (19 + 1) * 2 = 40 */

    0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00, 0x49, 0x00, 0x6E, 0x00,
    0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00, 0x61, 0x00, 0x63, 0x00, 0x65, 0x00, 0x47, 0x00,
    0x55, 0x00, 0x49, 0x00, 0x44, 0x00, 0x00, 0x00,     /* bPropertyName: DeviceInterfaceGUID */

    0x4E, 0x00, 0x00, 0x00,                             /* dwPropertyDataLength: 4E */

    '{', 0x00, '0', 0x00, '1', 0x00, '2', 0x00, '3', 0x00, '4', 0x00, '5', 0x00, '6', 0x00,
    '7', 0x00, '-', 0x00, '2', 0x00, 'A', 0x00, '4', 0x00, 'F', 0x00, '-', 0x00, '4', 0x00,
    '9', 0x00, 'E', 0x00, 'E', 0x00, '-', 0x00, '8', 0x00, 'D', 0x00, 'D', 0x00, '3', 0x00,
    '-', 0x00, 'F', 0x00, 'A', 0x00, 'D', 0x00, 'E', 0x00, 'A', 0x00, '3', 0x00, '7', 0x00,
    '7', 0x00, '2', 0x00, '3', 0x00, '4', 0x00, 'A', 0x00, '}', 0x00, 0x00, 0x00
        /* bPropertyData: {01234567-2A4F-49EE-8DD3-FADEA377234A} */
};

/*[]*/

