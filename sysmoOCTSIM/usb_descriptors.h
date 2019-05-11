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

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "ccid_device.h"

#define CCID_NUM_CLK_SUPPORTED 4

/* aggregate descriptors for the combined CDC-ACM + CCID device that we expose
 * from sysmoQMOD */

enum str_desc_num {
	STR_DESC_MANUF = 1,
	STR_DESC_PRODUCT,
	STR_DESC_CONFIG,
	STR_DESC_INTF_ACM_COMM,
	STR_DESC_INTF_ACM_DATA,
	STR_DESC_INTF_CCID,
	STR_DESC_SERIAL,
};

/* a struct of structs representing the concatenated collection of USB descriptors */
struct usb_desc_collection {
	struct usb_dev_desc dev;
	struct usb_config_desc cfg;

	/* CDC-ACM: Two interfaces, one with IRQ EP and one with BULK IN + OUT */
	struct {
		struct {
			struct usb_iface_desc iface;
			struct usb_cdc_hdr_desc cdc_hdr;
			struct usb_cdc_call_mgmt_desc cdc_call_mgmt;
			struct usb_cdc_acm_desc cdc_acm;
			struct usb_cdc_union_desc cdc_union;
			struct usb_ep_desc ep[1];
		} comm;
		struct {
			struct usb_iface_desc iface;
			struct usb_ep_desc ep[2];
		} data;
	} cdc;

	/* CCID: One interface with CCID class descriptor and three endpoints */
	struct {
		struct usb_iface_desc iface;
		struct usb_ccid_class_descriptor class;
		struct usb_ep_desc ep[3];
	} ccid;
	uint8_t str[148];
} __attribute__((packed));

#endif /* USB_DESCRIPTORS_H_ */
