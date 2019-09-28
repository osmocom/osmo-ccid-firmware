/* Card (ICC) UART driver for the Atmel ASF4 asynchronous USART */

#include <errno.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>

#include <hal_usart_async.h>
#include <utils_ringbuffer.h>
#include "driver_init.h"

#include "ncn8025.h"

#include "cuart.h"

static struct usart_async_descriptor* SIM_peripheral_descriptors[] = {&SIM0, &SIM1, &SIM2, &SIM3, &SIM4, &SIM5, &SIM6, NULL};

extern struct card_uart *cuart4slot_nr(uint8_t slot_nr);

/***********************************************************************
 * low-level helper routines
 ***********************************************************************/

static void _SIM_rx_cb(const struct usart_async_descriptor *const io_descr, uint8_t slot_nr)
{
	struct card_uart *cuart = cuart4slot_nr(slot_nr);
	int rc;
	OSMO_ASSERT(cuart);

	if (cuart->rx_threshold == 1) {
		/* bypass ringbuffer and report byte directly */
		uint8_t rx[1];
		rc = io_read((struct io_descriptor * const)&io_descr->io, rx, sizeof(rx));
		OSMO_ASSERT(rc == sizeof(rx));
		card_uart_notification(cuart, CUART_E_RX_SINGLE, rx);
	} else {
		/* go via ringbuffer and notify only after threshold */
		if (ringbuffer_num(&io_descr->rx) >= cuart->rx_threshold)
			card_uart_notification(cuart, CUART_E_RX_COMPLETE, NULL);
	}
}

static void _SIM_tx_cb(const struct usart_async_descriptor *const io_descr, uint8_t slot_nr)
{
	struct card_uart *cuart = cuart4slot_nr(slot_nr);
	OSMO_ASSERT(cuart);
	card_uart_notification(cuart, CUART_E_TX_COMPLETE, io_descr->tx_buffer);
}


/* the below ugli-ness is required as the usart_async_descriptor doesn't have
 * some kind of 'private' member that could provide the call-back anty kind of
 * context */
static void SIM0_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 0);
}
static void SIM1_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 1);
}
static void SIM2_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 2);
}
static void SIM3_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 3);
}
static void SIM4_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 4);
}
static void SIM5_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 5);
}
static void SIM6_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 6);
}
static void SIM7_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 7);
}
static usart_cb_t SIM_rx_cb[8] = {
	SIM0_rx_cb, SIM1_rx_cb, SIM2_rx_cb, SIM3_rx_cb,
	SIM4_rx_cb, SIM5_rx_cb, SIM6_rx_cb, SIM7_rx_cb,
};
static void SIM0_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 0);
}
static void SIM1_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 1);
}
static void SIM2_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 2);
}
static void SIM3_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 3);
}
static void SIM4_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 4);
}
static void SIM5_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 5);
}
static void SIM6_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 6);
}
static void SIM7_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 7);
}
static usart_cb_t SIM_tx_cb[8] = {
	SIM0_tx_cb, SIM1_tx_cb, SIM2_tx_cb, SIM3_tx_cb,
	SIM4_tx_cb, SIM5_tx_cb, SIM6_tx_cb, SIM7_tx_cb,
};

/***********************************************************************
 * Interface with card_uart (cuart) core
 ***********************************************************************/

/* forward-declaration */
static struct card_uart_driver asf4_usart_driver;
static int asf4_usart_close(struct card_uart *cuart);

static int asf4_usart_open(struct card_uart *cuart, const char *device_name)
{
	struct usart_async_descriptor *usa_pd;
	int slot_nr = atoi(device_name);

	if (slot_nr >= ARRAY_SIZE(SIM_peripheral_descriptors))
		return -ENODEV;
	usa_pd = SIM_peripheral_descriptors[slot_nr];
	if (!usa_pd)
		return -ENODEV;

	cuart->u.asf4.usa_pd = usa_pd;
	cuart->u.asf4.slot_nr = slot_nr;

	usart_async_register_callback(usa_pd, USART_ASYNC_RXC_CB, SIM_rx_cb[slot_nr]);
	usart_async_register_callback(usa_pd, USART_ASYNC_TXC_CB, SIM_tx_cb[slot_nr]);
	usart_async_enable(usa_pd);

        return 0;
}

static int asf4_usart_close(struct card_uart *cuart)
{
	struct usart_async_descriptor *usa_pd = cuart->u.asf4.usa_pd;

	OSMO_ASSERT(cuart->driver == &asf4_usart_driver);

	usart_async_disable(usa_pd);

	return 0;
}

static int asf4_usart_async_tx(struct card_uart *cuart, const uint8_t *data, size_t len)
{
	struct usart_async_descriptor *usa_pd = cuart->u.asf4.usa_pd;
	int rc;

	OSMO_ASSERT(cuart->driver == &asf4_usart_driver);
	OSMO_ASSERT(usart_async_is_tx_empty(usa_pd));

	rc = io_write(&usa_pd->io, data, len);
	if (rc < 0)
		return rc;

	cuart->tx_busy = true;

	return rc;
}

static int asf4_usart_async_rx(struct card_uart *cuart, uint8_t *data, size_t len)
{
	struct usart_async_descriptor *usa_pd = cuart->u.asf4.usa_pd;

	OSMO_ASSERT(cuart->driver == &asf4_usart_driver);

	return io_read(&usa_pd->io, data, len);
}

static int asf4_usart_ctrl(struct card_uart *cuart, enum card_uart_ctl ctl, int arg)
{
	struct ncn8025_settings settings;
	Sercom *sercom = cuart->u.asf4.usa_pd->device.hw;

	switch (ctl) {
	case CUART_CTL_RX:
		if (arg)
			sercom->USART.CTRLB.bit.RXEN = 1;
		else
			sercom->USART.CTRLB.bit.RXEN = 0;
		break;
	case CUART_CTL_RST:
		ncn8025_get(cuart->u.asf4.slot_nr, &settings);
		settings.rstin = arg ? true : false;
		ncn8025_set(cuart->u.asf4.slot_nr, &settings);
		break;
	case CUART_CTL_POWER:
		ncn8025_get(cuart->u.asf4.slot_nr, &settings);
		settings.cmdvcc = arg ? true : false;
		ncn8025_set(cuart->u.asf4.slot_nr, &settings);
		break;
	case CUART_CTL_WTIME:
		/* no driver-specific handling of this */
		break;
	case CUART_CTL_CLOCK:
		/* FIXME */
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct card_uart_ops asf4_usart_ops = {
	.open = asf4_usart_open,
	.close = asf4_usart_close,
	.async_tx = asf4_usart_async_tx,
	.async_rx = asf4_usart_async_rx,
	.ctrl = asf4_usart_ctrl,
};

static struct card_uart_driver asf4_usart_driver = {
	.name = "asf4",
	.ops = &asf4_usart_ops,
};

static __attribute__((constructor)) void on_dso_load_cuart_asf4(void)
{
	card_uart_driver_register(&asf4_usart_driver);
}
