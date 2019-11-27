#include <errno.h>
#include <string.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/timer.h>

#include "cuart.h"

static LLIST_HEAD(g_cuart_drivers);

const struct value_string card_uart_event_vals[] = {
	OSMO_VALUE_STRING(CUART_E_RX_SINGLE),
	OSMO_VALUE_STRING(CUART_E_RX_COMPLETE),
	OSMO_VALUE_STRING(CUART_E_RX_TIMEOUT),
	OSMO_VALUE_STRING(CUART_E_TX_COMPLETE),
	{ 0, NULL }
};

static struct card_uart_driver *cuart_drv_by_name(const char *driver_name)
{
	struct card_uart_driver *drv;
	llist_for_each_entry(drv, &g_cuart_drivers, list) {
		if (!strcmp(drv->name, driver_name))
			return drv;
	}
	return NULL;
}

/* obtain the current ETU in us */
static int get_etu_in_us(struct card_uart *cuart)
{
	OSMO_ASSERT(cuart);
	OSMO_ASSERT(cuart->driver);
	OSMO_ASSERT(cuart->driver->ops);
	OSMO_ASSERT(cuart->driver->ops->ctrl);

	return 1e6 / cuart->driver->ops->ctrl(cuart, CUART_CTL_GET_BAUDRATE, 0);
}

/* software waiting-time timer has expired */
static void card_uart_wtime_cb(void *data)
{
	struct card_uart *cuart = (struct card_uart *) data;
	card_uart_notification(cuart, CUART_E_RX_TIMEOUT, NULL);
	/* should we automatically disable the receiver? */
}

void card_uart_wtime_restart(struct card_uart *cuart)
{
	int secs, usecs;

	if(!cuart->current_wtime_byte)
		return;

	/* timemout is wtime * ETU + expected number of bytes * (12ETU+1 slack)ETU */
	usecs = get_etu_in_us(cuart) * cuart->wtime_etu +
			get_etu_in_us(cuart) * cuart->current_wtime_byte * (12+1);
	if (usecs > 1000000) {
		secs = usecs / 1000000;
		usecs = usecs % 1000000;
	} else
		secs = 0;
	osmo_timer_schedule(&cuart->wtime_tmr, secs, usecs);
}

int card_uart_open(struct card_uart *cuart, const char *driver_name, const char *device_name)
{
	struct card_uart_driver *drv = cuart_drv_by_name(driver_name);
	int rc;

	if (!drv)
		return -ENODEV;

	cuart->wtime_etu = 9600; /* ISO 7816-3 Section 8.1 */
	cuart->rx_enabled = true;
	cuart->rx_threshold = 1;
	osmo_timer_setup(&cuart->wtime_tmr, card_uart_wtime_cb, cuart);

	rc = drv->ops->open(cuart, device_name);
	if (rc < 0)
		return rc;

	cuart->driver = drv;
	return 0;
}

int card_uart_close(struct card_uart *cuart)
{
	OSMO_ASSERT(cuart);
	OSMO_ASSERT(cuart->driver);
	OSMO_ASSERT(cuart->driver->ops);
	OSMO_ASSERT(cuart->driver->ops->close);
	return cuart->driver->ops->close(cuart);
}

int card_uart_ctrl(struct card_uart *cuart, enum card_uart_ctl ctl, int arg)
{
	int rc;
	OSMO_ASSERT(cuart);
	OSMO_ASSERT(cuart->driver);
	OSMO_ASSERT(cuart->driver->ops);
	OSMO_ASSERT(cuart->driver->ops->ctrl);

	rc = cuart->driver->ops->ctrl(cuart, ctl, arg);
	if (rc < 0)
		return rc;

	switch (ctl) {
	case CUART_CTL_WTIME:
		cuart->wtime_etu = arg;
		break;
	case CUART_CTL_RX:
		cuart->rx_enabled = arg ? true : false;
		if (!cuart->rx_enabled)
			osmo_timer_del(&cuart->wtime_tmr);
//		else
//			card_uart_wtime_restart(cuart);
		break;
	case CUART_CTL_RX_TIMER_HINT:
		cuart->current_wtime_byte = arg;
		if(arg)
			card_uart_wtime_restart(cuart);
		else
			osmo_timer_del(&cuart->wtime_tmr);
		break;
	default:
		break;
	}

	return rc;
}

int card_uart_tx(struct card_uart *cuart, const uint8_t *data, size_t len, bool rx_after_complete)
{
	OSMO_ASSERT(cuart);
	OSMO_ASSERT(cuart->driver);
	OSMO_ASSERT(cuart->driver->ops);
	OSMO_ASSERT(cuart->driver->ops->async_tx);

	OSMO_ASSERT(!cuart->tx_busy);
	cuart->tx_busy = true;
	cuart->rx_after_tx_compl = rx_after_complete;
	/* disable receiver to avoid receiving what we transmit */
	card_uart_ctrl(cuart, CUART_CTL_RX, false);

	return cuart->driver->ops->async_tx(cuart, data, len);
}

int card_uart_rx(struct card_uart *cuart, uint8_t *data, size_t len)
{
	OSMO_ASSERT(cuart);
	OSMO_ASSERT(cuart->driver);
	OSMO_ASSERT(cuart->driver->ops);
	OSMO_ASSERT(cuart->driver->ops->async_rx);
	return cuart->driver->ops->async_rx(cuart, data, len);
}

void card_uart_set_rx_threshold(struct card_uart *cuart, size_t rx_threshold)
{
	cuart->rx_threshold = rx_threshold;
}

void card_uart_notification(struct card_uart *cuart, enum card_uart_event evt, void *data)
{
	OSMO_ASSERT(cuart);
	OSMO_ASSERT(cuart->handle_event);

	switch (evt) {
	case CUART_E_TX_COMPLETE:
		cuart->tx_busy = false;
		/* re-enable receiver if we're done with transmit */
		if (cuart->rx_after_tx_compl)
			card_uart_ctrl(cuart, CUART_CTL_RX, true);
		break;
//	case CUART_E_RX_COMPLETE:
//		osmo_timer_del(&cuart->wtime_tmr);
//		break;
//	case CUART_E_RX_SINGLE:
//		card_uart_wtime_restart(cuart);
//		break;
	default:
		break;
	}

	cuart->handle_event(cuart, evt, data);
}

int card_uart_driver_register(struct card_uart_driver *drv)
{
	OSMO_ASSERT(!cuart_drv_by_name(drv->name));
	OSMO_ASSERT(drv->name);
	OSMO_ASSERT(drv->ops);
	llist_add_tail(&drv->list, &g_cuart_drivers);
	return 0;
}
