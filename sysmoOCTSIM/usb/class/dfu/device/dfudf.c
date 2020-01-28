/**
 * \file
 *
 * \brief USB Device Stack DFU Function Implementation.
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

#include "dfudf.h"
#include "usb_dfu.h"


/** USB Device DFU Function Specific Data */
struct dfudf_func_data {
	/** DFU Interface information */
	uint8_t func_iface;
	/** DFU Enable Flag */
	bool enabled;
};

static struct usbdf_driver _dfudf;
static struct dfudf_func_data _dfudf_funcd;

enum dfu_state dfu_state = DFU_STATE_appIDLE;
enum usb_dfu_status dfu_status = DFU_STATUS_OK;

uint8_t dfu_download_data[512];
uint16_t dfu_download_length = 0;
size_t dfu_download_offset = 0;
bool dfu_manifestation_complete = false;

/**
 * \brief Enable DFU Function
 * \param[in] drv Pointer to USB device function driver
 * \param[in] desc Pointer to USB interface descriptor
 * \return Operation status.
 */
static int32_t dfudf_enable(struct usbdf_driver *drv, struct usbd_descriptors *desc)
{
	struct dfudf_func_data *func_data = (struct dfudf_func_data *)(drv->func_data);

	usb_iface_desc_t ifc_desc;
	uint8_t *        ifc;

	ifc = desc->sod;
	if (NULL == ifc) {
		return ERR_NOT_FOUND;
	}

	ifc_desc.bInterfaceNumber = ifc[2];
	ifc_desc.bInterfaceClass  = ifc[5];

	if (USB_DFU_CLASS == ifc_desc.bInterfaceClass) {
		if (func_data->func_iface == ifc_desc.bInterfaceNumber) { // Initialized
			return ERR_ALREADY_INITIALIZED;
		} else if (func_data->func_iface != 0xFF) { // Occupied
			return ERR_NO_RESOURCE;
		} else {
			func_data->func_iface = ifc_desc.bInterfaceNumber;
		}
	} else { // Not supported by this function driver
		return ERR_NOT_FOUND;
	}

	// there are no endpoint to install since DFU uses only the control endpoint

	ifc = usb_find_desc(usb_desc_next(desc->sod), desc->eod, USB_DT_INTERFACE);

	// Installed
	_dfudf_funcd.enabled = true;
	return ERR_NONE;
}

/**
 * \brief Disable DFU Function
 * \param[in] drv Pointer to USB device function driver
 * \param[in] desc Pointer to USB device descriptor
 * \return Operation status.
 */
static int32_t dfudf_disable(struct usbdf_driver *drv, struct usbd_descriptors *desc)
{
	struct dfudf_func_data *func_data = (struct dfudf_func_data *)(drv->func_data);

	usb_iface_desc_t ifc_desc;

	if (desc) {
		ifc_desc.bInterfaceClass = desc->sod[5];
		// Check interface
		if (ifc_desc.bInterfaceClass != USB_DFU_CLASS) {
			return ERR_NOT_FOUND;
		}
	}

	func_data->func_iface = 0xFF;

	_dfudf_funcd.enabled = false;
	return ERR_NONE;
}

/**
 * \brief DFU Control Function
 * \param[in] drv Pointer to USB device function driver
 * \param[in] ctrl USB device general function control type
 * \param[in] param Parameter pointer
 * \return Operation status.
 */
static int32_t dfudf_ctrl(struct usbdf_driver *drv, enum usbdf_control ctrl, void *param)
{
	switch (ctrl) {
	case USBDF_ENABLE:
		return dfudf_enable(drv, (struct usbd_descriptors *)param);

	case USBDF_DISABLE:
		return dfudf_disable(drv, (struct usbd_descriptors *)param);

	case USBDF_GET_IFACE:
		return ERR_UNSUPPORTED_OP;

	default:
		return ERR_INVALID_ARG;
	}
}

/**
 * \brief Process the DFU IN request
 * \param[in] ep Endpoint address.
 * \param[in] req Pointer to the request.
 * \param[in] stage Stage of the request.
 * \return Operation status.
 */
static int32_t dfudf_in_req(uint8_t ep, struct usb_req *req, enum usb_ctrl_stage stage)
{
	if (USB_DATA_STAGE == stage) { // the data stage is only for IN data, which we sent
		return ERR_NONE; // send the IN data
	}

	int32_t to_return = ERR_NONE;
	uint8_t response[6]; // buffer for the response to this request
	switch (req->bRequest) {
	case USB_DFU_UPLOAD: // upload firmware from flash not supported
		dfu_state = DFU_STATE_dfuERROR; // unsupported class request
		to_return = ERR_UNSUPPORTED_OP; // stall control pipe (don't reply to the request)
		break;
	case USB_DFU_GETSTATUS: // get status
		response[0] = dfu_status; // set status
		response[1] = 10; // set poll timeout (24 bits, in milliseconds) to small value for periodical poll
		response[2] = 0; // set poll timeout (24 bits, in milliseconds) to small value for periodical poll
		response[3] = 0; // set poll timeout (24 bits, in milliseconds) to small value for periodical poll
		response[4] = dfu_state; // set state
		response[5] = 0; // string not used
		to_return = usbdc_xfer(ep, response, 6, false); // send back status
		break;
	case USB_DFU_GETSTATE: // get state
		response[0] = dfu_state; // return state
		to_return = usbdc_xfer(ep, response, 1, false); // send back state
		break;
	default: // all other DFU class IN request
		dfu_state = DFU_STATE_dfuERROR; // unknown or unsupported class request
		to_return = ERR_INVALID_ARG; // stall control pipe (don't reply to the request)
		break;
	}

	return to_return;
}

/**
 * \brief Process the DFU OUT request
 * \param[in] ep Endpoint address.
 * \param[in] req Pointer to the request.
 * \param[in] stage Stage of the request.
 * \return Operation status.
 */
static int32_t dfudf_out_req(uint8_t ep, struct usb_req *req, enum usb_ctrl_stage stage)
{
	int32_t to_return = ERR_NONE;
	switch (req->bRequest) {
	case USB_DFU_DETACH: // detach makes only sense in DFU run-time/application mode
#if (DISABLE_DFU_DETACH != 0)
		dfu_state = DFU_STATE_dfuERROR; // unsupported class request
		to_return = ERR_UNSUPPORTED_OP; // stall control pipe (don't reply to the request)
#else
		to_return = usbdc_xfer(ep, NULL, 0, false);
		*(uint32_t*)HSRAM_ADDR = 0x44465521;
		__disable_irq();
		delay_us(10000);
		usbdc_detach();
		delay_us(100000);
		NVIC_SystemReset();
#endif
		break;
	case USB_DFU_CLRSTATUS: // clear status
		if (DFU_STATE_dfuERROR == dfu_state || DFU_STATUS_OK != dfu_status) { // only clear in case there is an error
			dfu_status = DFU_STATUS_OK; // clear error status
			dfu_state = DFU_STATE_dfuIDLE; // put back in idle state
		}
		to_return = usbdc_xfer(ep, NULL, 0, false); // send ACK
		break;
	case USB_DFU_ABORT: // abort current operation
		dfu_download_offset = 0; // reset download progress
		dfu_state = DFU_STATE_dfuIDLE; // put back in idle state (nothing else to do)
		to_return = usbdc_xfer(ep, NULL, 0, false); // send ACK
		break;
	default: // all other DFU class OUT request
		dfu_state = DFU_STATE_dfuERROR; // unknown class request
		to_return = ERR_INVALID_ARG; // stall control pipe (don't reply to the request)
		break;
	}

	return to_return;
}

/**
 * \brief Process the CDC class request
 * \param[in] ep Endpoint address.
 * \param[in] req Pointer to the request.
 * \param[in] stage Stage of the request.
 * \return Operation status.
 */
static int32_t dfudf_req(uint8_t ep, struct usb_req *req, enum usb_ctrl_stage stage)
{
	if (0x01 != ((req->bmRequestType >> 5) & 0x03)) { // class request
		return ERR_NOT_FOUND;
	}

	if ((req->wIndex == _dfudf_funcd.func_iface)) {
		if (req->bmRequestType & USB_EP_DIR_IN) {
			return dfudf_in_req(ep, req, stage);
		} else {
			return dfudf_out_req(ep, req, stage);
		}
	} else {
		return ERR_NOT_FOUND;
	}
	return ERR_NOT_FOUND;
}

/** USB Device DFU Handler Struct */
static struct usbdc_handler dfudf_req_h = {NULL, (FUNC_PTR)dfudf_req};

/**
 * \brief Initialize the USB DFU Function Driver
 */
int32_t dfudf_init(void)
{
	if (usbdc_get_state() > USBD_S_POWER) {
		return ERR_DENIED;
	}

	_dfudf.ctrl      = dfudf_ctrl;
	_dfudf.func_data = &_dfudf_funcd;

	usbdc_register_function(&_dfudf);
	usbdc_register_handler(USBDC_HDL_REQ, &dfudf_req_h);

	return ERR_NONE;
}

/**
 * \brief De-initialize the USB DFU Function Driver
 */
void dfudf_deinit(void)
{
}

/**
 * \brief Check whether DFU Function is enabled
 */
bool dfudf_is_enabled(void)
{
	return _dfudf_funcd.enabled;
}
