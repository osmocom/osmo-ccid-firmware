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
#include "ccid_proto.h"
#include "usb_includes.h"

#include "cdcdf_acm_desc.h"
#include "usb_descriptors.h"

#ifndef USB_CLASS_CCID
#define	USB_CLASS_CCID	11
#endif

struct ccid_df_func_data {
	uint8_t func_iface;	/*!< interface number */
	uint8_t func_ep_in;	/*!< IN endpoint number */
	uint8_t func_ep_out;	/*!< OUT endpoint number */
	uint8_t func_ep_irq;	/*!< IRQ endpoint number */
	bool enabled;		/*!< is this driver/function enabled? */
	const struct usb_ccid_class_descriptor *ccid_cd;
};

static struct usbdf_driver _ccid_df;
static struct ccid_df_func_data _ccid_df_funcd;

extern const struct usb_desc_collection usb_fs_descs;

/* FIXME: make those configurable, ensure they're sized according to
 * bNumClockSupported / bNumDataRatesSupported */
static uint32_t ccid_clock_frequencies[CCID_NUM_CLK_SUPPORTED] = { LE32(2500),LE32(5000),LE32(10000),LE32(20000) };
static uint32_t ccid_baud_rates[] = { LE32(9600) };

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
			if ((ep_desc.bmAttributes & USB_EP_XTYPE_MASK) == USB_EP_XTYPE_INTERRUPT)
				func_data->func_ep_irq = ep_desc.bEndpointAddress;
			else
				func_data->func_ep_in = ep_desc.bEndpointAddress;
		} else {
			func_data->func_ep_out = ep_desc.bEndpointAddress;
		}
		usb_d_ep_enable(ep_desc.bEndpointAddress);
		desc->sod = ep;
		ep = usb_find_ep_desc(usb_desc_next(desc->sod), desc->eod);
	}

	ASSERT(func_data->func_ep_irq != 0xff);
	ASSERT(func_data->func_ep_in != 0xff);
	ASSERT(func_data->func_ep_out != 0xff);

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

	_ccid_df_funcd.enabled = false;
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

/* Section 5.3.1: ABORT */
static int32_t ccid_df_ctrl_req_ccid_abort(uint8_t ep, struct usb_req *req,
					   enum usb_ctrl_stage stage)
{
	const struct usb_ccid_class_descriptor *ccid_cd = _ccid_df_funcd.ccid_cd;
	uint8_t slot_nr = req->wValue & 0xff;

	if (slot_nr > ccid_cd->bMaxSlotIndex)
		return ERR_INVALID_ARG;
	slot_nr = req->wValue;

	/* FIXME: Implement Abort handling, particularly in combination with
	 * the PC_to_RDR_Abort on the OUT EP */
	return ERR_NONE;
}

/* Section 5.3.2: return array of DWORD containing clock frequencies in kHz */
static int32_t ccid_df_ctrl_req_ccid_get_clock_freq(uint8_t ep, struct usb_req *req,
						    enum usb_ctrl_stage stage)
{
	const struct usb_ccid_class_descriptor *ccid_cd = _ccid_df_funcd.ccid_cd;

	if (stage != USB_DATA_STAGE)
		return ERR_NONE;

	if (req->wLength != ccid_cd->bNumClockSupported * sizeof(uint32_t))
		return ERR_INVALID_DATA;

	return usbdc_xfer(ep, (uint8_t *)ccid_clock_frequencies, req->wLength, false);
}

/* Section 5.3.3: return array of DWORD containing data rates in bps */
static int32_t ccid_df_ctrl_req_ccid_get_data_rates(uint8_t ep, struct usb_req *req,
						    enum usb_ctrl_stage stage)
{
	const struct usb_ccid_class_descriptor *ccid_cd = _ccid_df_funcd.ccid_cd;

	if (stage != USB_DATA_STAGE)
		return ERR_NONE;

	if (req->wLength != ccid_cd->bNumDataRatesSupported * sizeof(uint32_t))
		return ERR_INVALID_DATA;

	return usbdc_xfer(ep, (uint8_t *)ccid_baud_rates, req->wLength, false);
}


/* process a control endpoint request */
static int32_t ccid_df_ctrl_req(uint8_t ep, struct usb_req *req, enum usb_ctrl_stage stage)
{
	/* verify this is a class-specific request */
	if (0x01 != ((req->bmRequestType >> 5) & 0x03))
                return ERR_NOT_FOUND;

	/* Verify req->wIndex == interface */
	if (req->wIndex != _ccid_df_funcd.func_iface)
                return ERR_NOT_FOUND;

	switch (req->bRequest) {
	case CLASS_SPEC_CCID_ABORT:
		if (req->bmRequestType & USB_EP_DIR_IN)
			return ERR_INVALID_ARG;
		return ccid_df_ctrl_req_ccid_abort(ep, req, stage);
	case CLASS_SPEC_CCID_GET_CLOCK_FREQ:
		if (!(req->bmRequestType & USB_EP_DIR_IN))
			return ERR_INVALID_ARG;
		return ccid_df_ctrl_req_ccid_get_clock_freq(ep, req, stage);
	case CLASS_SPEC_CCID_GET_DATA_RATES:
		if (!(req->bmRequestType & USB_EP_DIR_IN))
			return ERR_INVALID_ARG;
		return ccid_df_ctrl_req_ccid_get_data_rates(ep, req, stage);
	default:
		return ERR_NOT_FOUND;
	}
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

int32_t ccid_df_read_out(uint8_t *buf, uint32_t size)
{
	if (!ccid_df_is_enabled())
		return ERR_DENIED;
	return usbdc_xfer(_ccid_df_funcd.func_ep_out, buf, size, false);
}

int32_t ccid_df_write_in(uint8_t *buf, uint32_t size)
{
	if (!ccid_df_is_enabled())
		return ERR_DENIED;
	return usbdc_xfer(_ccid_df_funcd.func_ep_in, buf, size, true);
}

int32_t ccid_df_write_irq(uint8_t *buf, uint32_t size)
{
	if (!ccid_df_is_enabled())
		return ERR_DENIED;
	return usbdc_xfer(_ccid_df_funcd.func_ep_irq, buf, size, true);
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

bool ccid_df_is_enabled(void)
{
	return _ccid_df_funcd.enabled;
}
