#ifndef _USB_DEV_DFU_H
#define _USB_DEV_DFU_H

#include <stdint.h>

#include "usb_protocol.h"
#include "usb_dfu.h"

/* USB DFU functional descriptor */
#define DFU_FUNC_DESC  {						\
	.bLength		= USB_DT_DFU_SIZE,			\
	.bDescriptorType	= USB_DT_DFU,				\
	.bmAttributes		= USB_DFU_CAN_UPLOAD | USB_DFU_CAN_DOWNLOAD, \
	.wDetachTimeOut		= 5000,					\
	.wTransferSize		= FLASH_PAGE_SIZE,			\
	.bcdDFUVersion		= 0x0100,				\
}

/* Number of DFU interface during runtime mode */
#define DFURT_NUM_IF		1

/* to be used by the runtime as part of its USB descriptor structure
 * declaration */
#define DFURT_IF_DESCRIPTOR_STRUCT		\
	struct usb_iface_desc		dfu_rt;		\
	struct usb_dfu_func_descriptor	func_dfu;

/* to be used by the runtime as part of its USB Dsecriptor structure
 * definition */
#define DFURT_IF_DESCRIPTOR(dfuIF, dfuSTR)					\
	.dfu_rt = {								\
		.bLength 		= sizeof(struct usb_iface_desc),	\
		.bDescriptorType	= USB_DT_INTERFACE,			\
		.bInterfaceNumber	= dfuIF,				\
		.bAlternateSetting	= 0,					\
		.bNumEndpoints		= 0,					\
		.bInterfaceClass	= 0xFE,					\
		.bInterfaceSubClass	= 0x01,					\
		.bInterfaceProtocol	= 0x01,					\
		.iInterface		= dfuSTR,				\
	},									\
	.func_dfu = DFU_FUNC_DESC						\

#endif
