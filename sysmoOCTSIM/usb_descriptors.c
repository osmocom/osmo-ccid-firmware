/*
 * Copyright (C) 2019 sysmocom -s.f.m.c. GmbH, Author: Eric Wild <ewild@sysmocom.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "usbd_config.h"
#include "usbdc.h"
#include "usb_protocol.h"
#include "usb_protocol_cdc.h"
#include "ccid_proto.h"
#include "cdcdf_acm_desc.h"
#include "usb_descriptors.h"



const struct usb_desc_collection usb_fs_descs = {
	.dev = {
		.bLength = sizeof(struct usb_dev_desc),
		.bDescriptorType = USB_DT_DEVICE,
		.bcdUSB = USB_V2_0,
		.bDeviceClass = 0x00,
		.bDeviceSubClass = 0,
		.bDeviceProtocol = 0,
		.bMaxPacketSize0 = CONF_USB_CDCD_ACM_BMAXPKSZ0,
		.idVendor = CONF_USB_CDCD_ACM_IDVENDER,
		.idProduct = CONF_USB_CDCD_ACM_IDPRODUCT,
		.iManufacturer = STR_DESC_MANUF,
		.iProduct = STR_DESC_PRODUCT,
		.iSerialNumber = STR_DESC_SERIAL,
		.bNumConfigurations = 1,
	},
	.cfg = {
		.bLength = sizeof(struct usb_config_desc),
		.bDescriptorType = USB_DT_CONFIG,
		.wTotalLength = sizeof(usb_fs_descs.cfg) +
#ifdef WITH_DEBUG_CDC
				sizeof(usb_fs_descs.cdc) +
#endif
				sizeof(usb_fs_descs.ccid) +
				sizeof(usb_fs_descs.dfu_rt) +
				sizeof(usb_fs_descs.func_dfu),
#ifdef WITH_DEBUG_CDC
		.bNumInterfaces = 4,
#else
		.bNumInterfaces = 2,
#endif
		.bConfigurationValue = CONF_USB_CDCD_ACM_BCONFIGVAL,
		.iConfiguration = STR_DESC_CONFIG,
		.bmAttributes = CONF_USB_CDCD_ACM_BMATTRI,
		/* FIXME: The device should offer at least one 100mA configuration. */
		.bMaxPower = 250, /* request 500mA */
	},
#ifdef WITH_DEBUG_CDC
	.cdc = {
		.comm = {
			.iface = {
				.bLength = sizeof(struct usb_iface_desc),
				.bDescriptorType = USB_DT_INTERFACE,
				.bInterfaceNumber = CONF_USB_CDCD_ACM_COMM_BIFCNUM,
				.bAlternateSetting = CONF_USB_CDCD_ACM_COMM_BALTSET,
				.bNumEndpoints = 1,
				.bInterfaceClass = CDC_CLASS_COMM,
				.bInterfaceSubClass = CDC_SUBCLASS_ACM,
				.bInterfaceProtocol = 0x00,
				.iInterface = STR_DESC_INTF_ACM_COMM,
			},
			.cdc_hdr = {
				.bFunctionLength = sizeof(struct usb_cdc_hdr_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_HEADER,
				.bcdCDC = LE16(0x1001),
			},
			.cdc_call_mgmt = {
				.bFunctionLength = sizeof(struct usb_cdc_call_mgmt_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_CALL_MGMT,
				.bmCapabilities = 0x01,
				.bDataInterface = 0x00,
			},
			.cdc_acm = {
				.bFunctionLength = sizeof(struct usb_cdc_acm_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_ACM,
				.bmCapabilities = 0x02,
			},
			.cdc_union = {
				.bFunctionLength = sizeof(struct usb_cdc_union_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_UNION,
				.bMasterInterface = CONF_USB_CDCD_ACM_COMM_BIFCNUM,
				.bSlaveInterface0 = CONF_USB_CDCD_ACM_DATA_BIFCNUM,
			},
			.ep = {
				{
					.bLength = sizeof(struct usb_ep_desc),
					.bDescriptorType = USB_DT_ENDPOINT,
					.bEndpointAddress = CONF_USB_CDCD_ACM_COMM_INT_EPADDR,
					.bmAttributes = USB_EP_TYPE_INTERRUPT,
					.wMaxPacketSize = CONF_USB_CDCD_ACM_COMM_INT_MAXPKSZ,
					.bInterval = CONF_USB_CDCD_ACM_COMM_INT_INTERVAL,
				},
			},
		},
		.data = {
			.iface = {
				.bLength = sizeof(struct usb_iface_desc),
				.bDescriptorType = USB_DT_INTERFACE,
				.bInterfaceNumber = CONF_USB_CDCD_ACM_DATA_BIFCNUM,
				.bAlternateSetting = CONF_USB_CDCD_ACM_DATA_BALTSET,
				.bNumEndpoints = 2,
				.bInterfaceClass = CDC_CLASS_DATA,
				.bInterfaceSubClass = 0x00,
				.bInterfaceProtocol = 0x00,
				.iInterface = STR_DESC_INTF_ACM_DATA,
			},
			.ep = {
				{
					.bLength = sizeof(struct usb_ep_desc),
					.bDescriptorType = USB_DT_ENDPOINT,
					.bEndpointAddress = CONF_USB_CDCD_ACM_DATA_BULKOUT_EPADDR,
					.bmAttributes = USB_EP_TYPE_BULK,
					.wMaxPacketSize = CONF_USB_CDCD_ACM_DATA_BULKOUT_MAXPKSZ,
					.bInterval = 0,
				},
				{
					.bLength = sizeof(struct usb_ep_desc),
					.bDescriptorType = USB_DT_ENDPOINT,
					.bEndpointAddress = CONF_USB_CDCD_ACM_DATA_BULKIN_EPADDR,
					.bmAttributes = USB_EP_TYPE_BULK,
					.wMaxPacketSize = CONF_USB_CDCD_ACM_DATA_BULKIN_MAXPKSZ,
					.bInterval = 0,
				},
			},
		},
	},
#endif
	.ccid = {
		.iface = {
			.bLength = sizeof(struct usb_iface_desc),
			.bDescriptorType = USB_DT_INTERFACE,
			.bInterfaceNumber = 0,
			.bAlternateSetting = 0,
			.bNumEndpoints = 3,
			.bInterfaceClass = 11,
			.bInterfaceSubClass = 0,
			.bInterfaceProtocol = 0,
			.iInterface = STR_DESC_INTF_CCID,
		},
		.class = {
			.bLength = sizeof(struct usb_ccid_class_descriptor),
			.bDescriptorType = 33,
			.bcdCCID = LE16(0x0110),
			.bMaxSlotIndex = 7,
			.bVoltageSupport = 0x07, /* 5/3/1.8V */
			.dwProtocols = 0x01, /* only t0 */
			.dwDefaultClock = LE32(2500),
			.dwMaximumClock = LE32(20000),
			.bNumClockSupported = CCID_NUM_CLK_SUPPORTED,
			.dwDataRate = LE32(6720), /* default clock 2.5M/372 */
			.dwMaxDataRate = LE32(921600),
			.bNumDataRatesSupported = 0,
			.dwMaxIFSD = LE32(0),
			.dwSynchProtocols = LE32(0),
			.dwMechanical = LE32(0),
			/* 0x10000 TPDU level exchanges with CCID
			 * 0x80 Automatic PPS made by the CCID according to the active parameters
			 * 0x20 Automatic baud rate change according to active parameters 
			 * provided by the Host or self determined
			 * 0x10 Automatic ICC clock frequency change according to active parameters
			 *  provided by the Host or self determined */
			.dwFeatures = LE32(0x10 | 0x20 | 0x80 | 0x00010000),
			.dwMaxCCIDMessageLength = 272,
			.bClassGetResponse = 0xff,
			.bClassEnvelope = 0xff,
			.wLcdLayout = LE16(0),
			.bPINSupport = 0,
			.bMaxCCIDBusySlots = 8,
		},
		.ep = {
			{	/* Bulk-OUT descriptor */
				.bLength = sizeof(struct usb_ep_desc),
				.bDescriptorType = USB_DT_ENDPOINT,
				.bEndpointAddress = 0x02,
				.bmAttributes = USB_EP_TYPE_BULK,
				.wMaxPacketSize = 64,
				.bInterval = 0,
			},
			{ 	/* Bulk-IN descriptor */
				.bLength = sizeof(struct usb_ep_desc),
				.bDescriptorType = USB_DT_ENDPOINT,
				.bEndpointAddress = 0x83,
				.bmAttributes = USB_EP_TYPE_BULK,
				.wMaxPacketSize = 64,
				.bInterval = 0,
			},
			{	/* Interrupt dscriptor */
				.bLength = sizeof(struct usb_ep_desc),
				.bDescriptorType = USB_DT_ENDPOINT,
				.bEndpointAddress = 0x84,
				.bmAttributes = USB_EP_TYPE_INTERRUPT,
				.wMaxPacketSize = 64,
				.bInterval = 0x10,
			},
		},
	},
#ifdef WITH_DEBUG_CDC
	DFURT_IF_DESCRIPTOR(3, STR_DESC_INTF_DFURT),
#else
	DFURT_IF_DESCRIPTOR(1, STR_DESC_INTF_DFURT),
#endif
	.str = {
#if 0
		CDCD_ACM_STR_DESCES
#else
		 4,  3, 0x09, 0x04,
		50,  3, 's',0, 'y',0, 's',0, 'm',0, 'o',0, 'c',0, 'o',0, 'm',0, ' ',0, '-',0, ' ',0, 's',0, '.',0, 'f',0, '.',0, 'm',0, '.',0, 'c',0, '.',0, ' ',0, 'G',0, 'm',0, 'b',0, 'H',0,
		24,  3, 's',0, 'y',0, 's',0, 'm',0, 'o',0, 'O',0, 'C',0, 'T',0, 'S',0, 'I',0, 'M',0,
		 4,  3, 'A', 0,
		22,  3, 'd',0, 'e',0, 'b',0, 'u',0, 'g',0, ' ',0, 'U',0, 'A',0, 'R',0, 'T',0,
		22,  3, 'd',0, 'e',0, 'b',0, 'u',0, 'g',0, ' ',0, 'U',0, 'A',0, 'R',0, 'T',0,
		10,  3, 'C',0, 'C',0, 'I',0, 'D',0,
		12,  3, 'F',0, 'I',0, 'X',0, 'M',0, 'E',0,
		52,  3, 's',0, 'y',0, 's',0, 'm',0, 'o',0, 'O',0, 'C',0, 'T',0, 'S',0, 'I',0, 'M',0, ' ',0, 'D',0, 'F',0, 'U',0, ' ',0, '(',0, 'R',0, 'u',0, 'n',0, 't',0, 'i',0, 'm',0, 'e',0 ,')',0,
#endif
	}
};

const struct usbd_descriptors usb_descs[]
    = {{ (uint8_t *)&usb_fs_descs, (uint8_t *)&usb_fs_descs + sizeof(usb_fs_descs) }};
