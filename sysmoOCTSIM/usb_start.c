/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file or main.c
 * to avoid loosing it when reconfiguring.
 */
#include "atmel_start.h"
#include "usb_start.h"
#include "usb_descriptors.h"

#define CDCD_ECHO_BUF_SIZ CONF_USB_CDCD_ACM_DATA_BULKIN_MAXPKSZ

/** Buffers to receive and echo the communication bytes. */
static uint32_t usbd_cdc_buffer[CDCD_ECHO_BUF_SIZ / 4];

/** Ctrl endpoint buffer */
static uint8_t ctrl_buffer[64];

/**
 * \brief Callback invoked when bulk OUT data received
 */
static bool usb_device_cb_bulk_out(const uint8_t ep, const enum usb_xfer_code rc, const uint32_t count)
{
	cdcdf_acm_write((uint8_t *)usbd_cdc_buffer, count);

	/* No error. */
	return false;
}

/**
 * \brief Callback invoked when bulk IN data received
 */
static bool usb_device_cb_bulk_in(const uint8_t ep, const enum usb_xfer_code rc, const uint32_t count)
{
	/* Echo data. */
	cdcdf_acm_read((uint8_t *)usbd_cdc_buffer, sizeof(usbd_cdc_buffer));

	/* No error. */
	return false;
}

/**
 * \brief Callback invoked when Line State Change
 */
static bool usb_device_cb_state_c(usb_cdc_control_signal_t state)
{
	if (state.rs232.DTR) {
		/* Callbacks must be registered after endpoint allocation */
		cdcdf_acm_register_callback(CDCDF_ACM_CB_READ, (FUNC_PTR)usb_device_cb_bulk_out);
		cdcdf_acm_register_callback(CDCDF_ACM_CB_WRITE, (FUNC_PTR)usb_device_cb_bulk_in);
		/* Start Rx */
		cdcdf_acm_read((uint8_t *)usbd_cdc_buffer, sizeof(usbd_cdc_buffer));
	}

	/* No error. */
	return false;
}

extern const struct usbd_descriptors usb_descs[];

/* transmit given string descriptor */
static bool send_str_desc(uint8_t ep, const struct usb_req *req, enum usb_ctrl_stage stage,
			  const uint8_t *desc)
{
	uint16_t len_req = LE16(req->wLength);
	uint16_t len_desc = desc[0];
	uint16_t len_tx;
	bool need_zlp = !(len_req & (CONF_USB_CDCD_ACM_BMAXPKSZ0 - 1));

	if (len_req <= len_desc) {
		need_zlp = false;
		len_tx = len_req;
	} else {
		len_tx = len_desc;
	}

	if (ERR_NONE != usbdc_xfer(ep, (uint8_t *)desc, len_tx, need_zlp)) {
		return true;
	}

	return false;
}

extern uint8_t sernr_buf_descr[];
extern uint8_t product_buf_descr[];
/* call-back for every control EP request */
static int32_t string_req_cb(uint8_t ep, struct usb_req *req, enum usb_ctrl_stage stage)
{
	uint8_t index, type;

	if (stage != USB_SETUP_STAGE)
		return ERR_NOT_FOUND;

	if ((req->bmRequestType & (USB_REQT_TYPE_MASK | USB_REQT_DIR_IN)) !=
	    (USB_REQT_TYPE_STANDARD | USB_REQT_DIR_IN))
		return ERR_NOT_FOUND;

	/* abort if it's not a GET DESCRIPTOR request */
	if (req->bRequest != USB_REQ_GET_DESC)
		return ERR_NOT_FOUND;

	/* abort if it's not about a string descriptor */
	type = req->wValue >> 8;
	if (type != USB_DT_STRING)
		return ERR_NOT_FOUND;
#if 0
	printf("ep=%02x, bmReqT=%04x, bReq=%02x, wValue=%04x, stage=%d\r\n",
		ep, req->bmRequestType, req->bRequest, req->wValue, stage);
#endif
	/* abort if it's not a standard GET request */
	index = req->wValue & 0x00FF;
	switch (index) {
	case STR_DESC_SERIAL:
		return send_str_desc(ep, req, stage, sernr_buf_descr);
	case STR_DESC_PRODUCT:
		return send_str_desc(ep, req, stage, product_buf_descr);
	default:
		return ERR_NOT_FOUND;
	}
}


static struct usbdc_handler string_req_h = {NULL, (FUNC_PTR)string_req_cb};

/**
 * \brief CDC ACM Init
 */
void cdc_device_acm_init(void)
{
	/* usb stack init */
	usbdc_init(ctrl_buffer);
	usbdc_register_handler(USBDC_HDL_REQ, &string_req_h);

#ifdef WITH_DEBUG_CDC
	/* usbdc_register_funcion inside */
	cdcdf_acm_init();
#endif
	dfudf_init();

	printf("usb_descs_size=%u\r\n", usb_descs[0].eod - usb_descs[0].sod);

}

/**
 * \brief Start USB stack
 */
void usb_start(void)
{

#ifdef WITH_DEBUG_CDC
	while (!cdcdf_acm_is_enabled()) {
		// wait cdc acm to be installed
	};

	cdcdf_acm_register_callback(CDCDF_ACM_CB_STATE_C, (FUNC_PTR)usb_device_cb_state_c);
#endif
	while (!ccid_df_is_enabled());
}

void usb_init(void)
{
	cdc_device_acm_init();
	ccid_df_init();
	usbdc_start((struct usbd_descriptors *) usb_descs);
	usbdc_attach();

}
