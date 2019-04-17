/**
 * \file
 *
 * \brief USB Device Stack CCID Function Implementation.
 *
 * Copyroght (c) 2019 by Harald Welte <laforge@gnumonks.org>
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

#include "ccid_df.h"

#ifndef USB_CLASS_CCID
#define	USB_CLASS_CCID	11
#endif

struct ccid_df_func_data {
	uint8_t func_iface;	/*!< interface number */
	uint8_t func_ep_in;	/*!< IN endpoint number */
	uint8_t func_ep_out;	/*!< OUT endpoint number */
	uint8_t func_ep_irq;	/*!< IRQ endpoint number */
	bool enabled;		/*!< is this driver/function enabled? */
};

static struct usbdf_driver _ccid_df;
static struct ccid_df_func_data _ccid_df_funcd;

static int32_t ccid_df_enable(struct usbdf_driver *drv, struct usbd_descriptors *desc)
{
	struct ccid_df_func_data *func_data = (struct ccid_df_func_data *)(drv->func_data);
	usb_iface_desc_t ifc_desc;
	uint8_t *ifc, *ep;

	ifc = desc->sod;
	/* FIXME: iterate over multiple interfaces? */

	if (!ifc)
		return ERR_NOT_FOUND;

	ifc_desc.bInterfaceNumber = ifc[2];
	ifc_desc.bInterfaceClass = ifc[5];

	if (ifc_desc.bInterfaceClass != USB_CLASS_CCID)
		return ERR_NOT_FOUND;

	if (func_data->func_iface == ifc_desc.bInterfaceNumber)
		return ERR_ALREADY_INITIALIZED;
	else if (func_data->func_iface != 0xff)
		return ERR_NO_RESOURCE;

	func_data->func_iface = ifc_desc.bInterfaceNumber;

	ep = usb_find_desc(ifc, desc->eod, USB_DT_ENDPOINT);
	while (NULL != ep) {
		usb_ep_desc_t ep_desc;
		ep_desc.bEndpointAddress = ep[2];
		ep_desc.bmAttributes = ep[3];
		ep_desc.wMaxPacketSize = usb_get_u16(ep + 4);
		if (usb_d_ep_init(ep_desc.bEndpointAddress, ep_desc.bmAttributes, ep_desc.wMaxPacketSize))
			return ERR_NOT_INITIALIZED;
		if (ep_desc.bEndpointAddress & USB_EP_DIR_IN) {
			func_data->func_ep_in = ep_desc.bEndpointAddress;
			/* FIXME: interrupt? */
		} else {
			func_data->func_ep_out = ep_desc.bEndpointAddress;
		}
		usb_d_ep_enable(ep_desc.bEndpointAddress);
		desc->sod = ep;
		ep = usb_find_ep_desc(usb_desc_next(desc->sod), desc->eod);
	}

	_ccid_df_funcd.enabled = true;
	return ERR_NONE;
}

static int32_t ccid_df_disable(struct usbdf_driver *drv, struct usbd_descriptors *desc)
{
	struct ccid_df_func_data *func_data = (struct ccid_df_func_data *)(drv->func_data);
	usb_iface_desc_t ifc_desc;

	if (desc) {
		ifc_desc.bInterfaceClass = desc->sod[5];
		if (ifc_desc.bInterfaceClass != USB_CLASS_CCID)
			return ERR_NOT_FOUND;
	}

	func_data->func_iface = 0xff;
	if (func_data->func_ep_in != 0xff) {
		func_data->func_ep_in = 0xff;
		usb_d_ep_deinit(func_data->func_ep_in);
	}
	if (func_data->func_ep_out != 0xff) {
		func_data->func_ep_out = 0xff;
		usb_d_ep_deinit(func_data->func_ep_out);
	}
	if (func_data->func_ep_irq != 0xff) {
		func_data->func_ep_irq = 0xff;
		usb_d_ep_deinit(func_data->func_ep_irq);
	}

	_ccid_df_funcd.enabled = true;
	return ERR_NONE;
}

/*! \brief CCID Control Function (callback with USB core) */
static int32_t ccid_df_ctrl(struct usbdf_driver *drv, enum usbdf_control ctrl, void *param)
{
	switch (ctrl) {
	case USBDF_ENABLE:
		return ccid_df_enable(drv, (struct usbd_descriptors *)param);
	case USBDF_DISABLE:
		return ccid_df_disable(drv, (struct usbd_descriptors *)param);
	case USBDF_GET_IFACE:
		return ERR_UNSUPPORTED_OP;
	default:
		return ERR_INVALID_ARG;
	}
}


/* process a control endpoint request */
static int32_t ccid_df_ctrl_req(uint8_t ep, struct usb_req *req, enum usb_ctrl_stage stage)
{
	return ERR_NOT_FOUND;
}

static struct usbdc_handler ccid_df_req_h = { NULL, (FUNC_PTR) ccid_df_ctrl_req };

int32_t ccid_df_init(void)
{
	if (usbdc_get_state() > USBD_S_POWER)
		return ERR_DENIED;

	_ccid_df.ctrl = ccid_df_ctrl;
	_ccid_df.func_data = &_ccid_df_funcd;

	/* register the actual USB Function */
	usbdc_register_function(&_ccid_df);
	/* register the call-back for control endpoint handling */
	usbdc_register_handler(USBDC_HDL_REQ, &ccid_df_req_h);

	return ERR_NONE;
}

void ccid_df_deinit(void)
{
	usb_d_ep_deinit(_ccid_df_funcd.func_ep_in);
	usb_d_ep_deinit(_ccid_df_funcd.func_ep_out);
	usb_d_ep_deinit(_ccid_df_funcd.func_ep_irq);
}

int32_t ccid_df_register_callback(enum ccid_df_cb_type cb_type, FUNC_PTR func)
{
	switch (cb_type) {
	case CCID_DF_CB_READ_OUT:
		usb_d_ep_register_callback(_ccid_df_funcd.func_ep_out, USB_D_EP_CB_XFER, func);
		break;
	case CCID_DF_CB_WRITE_IN:
		usb_d_ep_register_callback(_ccid_df_funcd.func_ep_in, USB_D_EP_CB_XFER, func);
		break;
	case CCID_DF_CB_WRITE_IRQ:
		usb_d_ep_register_callback(_ccid_df_funcd.func_ep_irq, USB_D_EP_CB_XFER, func);
		break;
	default:
		return ERR_INVALID_ARG;
	}
	return ERR_NONE;
}
