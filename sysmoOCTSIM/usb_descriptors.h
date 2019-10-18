/*
 * usb_descriptors.h
 *
 *  Created on: Oct 11, 2019
 *      Author: phi
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

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
	uint8_t str[116];
} __attribute__((packed));

#endif /* USB_DESCRIPTORS_H_ */
