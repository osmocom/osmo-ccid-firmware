/**
 * \file
 *
 * \brief USB Device Firmware Upgrade (DFU) protocol definitions
 *
 * Copyright (c) 2018 sysmocom -s.f.m.c. GmbH, Author: Kevin Redon <kredon@sysmocom.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _USB_PROTOCOL_DFU_H_
#define _USB_PROTOCOL_DFU_H_

#include "usb_includes.h"

/*
 * \ingroup usb_protocol_group
 * \defgroup dfu_protocol_group Device Firmware Upgrade Definitions
 * \implements USB Device Firmware Upgrade Specification, Revision 1.1
 * @{
 */

/**
 * \name USB DFU Subclass IDs
 */
//@{
#define USB_DFU_CLASS 0xFE //!< Application Specific Class Code
//@}

//! \name USB DFU Subclass IDs
//@{
#define USB_DFU_SUBCLASS 0x01 //!< Device Firmware Upgrade Code
//@}

//! \name USB DFU Protocol IDs
//@{
#define USB_DFU_PROTOCOL_RUNTIME 0x01 //!< Runtime protocol
#define USB_DFU_PROTOCOL_DFU 0x02 //!< DFU mode protocol
//@}

//! \name USB DFU Attributes bits mask
//@{
#define USB_DFU_ATTRIBUTES_CAN_DOWNLOAD 0x01
#define USB_DFU_ATTRIBUTES_CAN_UPLOAD 0x02
#define USB_DFU_ATTRIBUTES_MANIFEST_TOLERANT 0x04
#define USB_DFU_ATTRIBUTES_WILL_DETACH 0x08
//@}

//! \name USB DFU Request IDs
//@{
#define USB_REQ_DFU_DETACH 0x00
#define USB_REQ_DFU_DNLOAD 0x01
#define USB_REQ_DFU_UPLOAD 0x02
#define USB_REQ_DFU_GETSTATUS 0x03
#define USB_REQ_DFU_CLRSTATUS 0x04
#define USB_REQ_DFU_GETSTATE 0x05
#define USB_REQ_DFU_ABORT 0x06
//@}

/*
 * Need to pack structures tightly, or the compiler might insert padding
 * and violate the spec-mandated layout.
 */
COMPILER_PACK_SET(1)

//! \name USB DFU Descriptors
//@{

//! DFU Functional Descriptor
typedef struct usb_dfu_func_desc {
	uint8_t bFunctionLength; /**< Size of this descriptor, in bytes (always 9) */
	uint8_t bDescriptorType; /**< DFU FUNCTIONAL descriptor type (always 0x21) */
	uint8_t bmAttributes; /**< DFU attributes bit mask */
	le16_t  wDetachTimeOut; /**< Time, in milliseconds, that the device will wait after receipt of the DFU_DETACH request */
	le16_t  wTransferSize; /**< Maximum number of bytes that the device can accept per control-write transaction */
	le16_t  bcdDFUVersion; /**< Numeric expression identifying the version of the DFU Specification release */
} usb_dfu_func_desc_t;

#define USB_DFU_FUNC_DESC_LEN 9
#define USB_DFU_FUNC_DESC_TYPE 0x21
#define USB_DFU_FUNC_DESC_BYTES(bmAttributes, wDetachTimeOut, wTransferSize, bcdDFUVersion) \
	USB_DFU_FUNC_DESC_LEN, /* bFunctionLength */ \
	USB_DFU_FUNC_DESC_TYPE, /* bDescriptorType */ \
	bmAttributes, \
	LE_BYTE0(wDetachTimeOut), LE_BYTE1(wDetachTimeOut), \
	LE_BYTE0(wTransferSize), LE_BYTE1(wTransferSize), \
	LE_BYTE0(bcdDFUVersion), LE_BYTE1(bcdDFUVersion)

COMPILER_PACK_RESET()

//! @}

//! USB DFU Request IDs
enum usb_dfu_req {
	USB_DFU_DETACH,
	USB_DFU_DNLOAD,
	USB_DFU_UPLOAD,
	USB_DFU_GETSTATUS,
	USB_DFU_CLRSTATUS,
	USB_DFU_GETSTATE,
	USB_DFU_ABORT,
};

//! USB DFU Device Status IDs
enum usb_dfu_status {
	USB_DFU_STATUS_OK,
	USB_DFU_STATUS_ERR_TARGET,
	USB_DFU_STATUS_ERR_FILE,
	USB_DFU_STATUS_ERR_WRITE,
	USB_DFU_STATUS_ERR_ERASE,
	USB_DFU_STATUS_ERR_CHECK_ERASED,
	USB_DFU_STATUS_ERR_PROG,
	USB_DFU_STATUS_ERR_VERIFY,
	USB_DFU_STATUS_ERR_ADDRESS,
	USB_DFU_STATUS_ERR_NOTDONE,
	USB_DFU_STATUS_ERR_FIRMWARE,
	USB_DFU_STATUS_ERR_VENDOR,
	USB_DFU_STATUS_ERR_USBR,
	USB_DFU_STATUS_ERR_POR,
	USB_DFU_STATUS_ERR_UNKNOWN,
	USB_DFU_STATUS_ERR_STALLEDPKT,
};

//! USB DFU Device State IDs
enum usb_dfu_state {
	USB_DFU_STATE_APP_IDLE,
	USB_DFU_STATE_APP_DETACH,
	USB_DFU_STATE_DFU_IDLE,
	USB_DFU_STATE_DFU_DNLOAD_SYNC,
	USB_DFU_STATE_DFU_DNBUSY,
	USB_DFU_STATE_DFU_DNLOAD_IDLE,
	USB_DFU_STATE_DFU_MANIFEST_SYNC,
	USB_DFU_STATE_DFU_MANIFEST,
	USB_DFU_STATE_DFU_MANIFEST_WAIT_RESET,
	USB_DFU_STATE_DFU_UPLOAD_IDLE,
	USB_DFU_STATE_DFU_ERROR,
};

#endif // _USB_PROTOCOL_DFU_H_
