#ifndef _USB_DFU_H
#define _USB_DFU_H
/* USB Device Firmware Update Implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * Protocol definitions for USB DFU
 *
 * This ought to be compliant to the USB DFU Spec 1.0 as available from
 * http://www.usb.org/developers/devclass_docs/usbdfu10.pdf
 *
 */

#include <stdint.h>
#define USB_DFU_CLASS 0xfe

#define USB_DT_DFU			0x21

struct usb_dfu_func_descriptor {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bmAttributes;
#define USB_DFU_CAN_DOWNLOAD	(1 << 0)
#define USB_DFU_CAN_UPLOAD	(1 << 1)
#define USB_DFU_MANIFEST_TOL	(1 << 2)
#define USB_DFU_WILL_DETACH	(1 << 3)
	uint16_t		wDetachTimeOut;
	uint16_t		wTransferSize;
	uint16_t		bcdDFUVersion;
} __attribute__ ((packed));

#define USB_DT_DFU_SIZE			9

/* DFU class-specific requests (Section 3, DFU Rev 1.1) */
enum usb_dfu_req {
	USB_DFU_DETACH,
	USB_DFU_DNLOAD,
	USB_DFU_UPLOAD,
	USB_DFU_GETSTATUS,
	USB_DFU_CLRSTATUS,
	USB_DFU_GETSTATE,
	USB_DFU_ABORT,
};


struct dfu_status {
	uint8_t bStatus;
	uint8_t bwPollTimeout[3];
	uint8_t bState;
	uint8_t iString;
} __attribute__((packed));

enum usb_dfu_status {
	DFU_STATUS_OK			=0x00,
	DFU_STATUS_errTARGET	=0x01,
	DFU_STATUS_errFILE		=0x02,
	DFU_STATUS_errWRITE		=0x03,
	DFU_STATUS_errERASE		=0x04,
	DFU_STATUS_errCHECK_ERASED	=0x05,
	DFU_STATUS_errPROG		=0x06,
	DFU_STATUS_errVERIFY		=0x07,
	DFU_STATUS_errADDRESS		=0x08,
	DFU_STATUS_errNOTDONE		=0x09,
	DFU_STATUS_errFIRMWARE		=0x0a,
	DFU_STATUS_errVENDOR		=0x0b,
	DFU_STATUS_errUSBR		=0x0c,
	DFU_STATUS_errPOR		=0x0d,
	DFU_STATUS_errUNKNOWN		=0x0e,
	DFU_STATUS_errSTALLEDPKT	=0x0f,
};

enum dfu_state {
	DFU_STATE_appIDLE		= 0,
	DFU_STATE_appDETACH		= 1,
	DFU_STATE_dfuIDLE		= 2,
	DFU_STATE_dfuDNLOAD_SYNC	= 3,
	DFU_STATE_dfuDNBUSY		= 4,
	DFU_STATE_dfuDNLOAD_IDLE	= 5,
	DFU_STATE_dfuMANIFEST_SYNC	= 6,
	DFU_STATE_dfuMANIFEST		= 7,
	DFU_STATE_dfuMANIFEST_WAIT_RST	= 8,
	DFU_STATE_dfuUPLOAD_IDLE	= 9,
	DFU_STATE_dfuERROR		= 10,
};

#endif /* _USB_DFU_H */
