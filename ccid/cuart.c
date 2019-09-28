#include <errno.h>
#include <string.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>

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

int card_uart_open(struct card_uart *cuart, const char *driver_name, const char *device_name)
{
	struct card_uart_driver *drv = cuart_drv_by_name(driver_name);
	int rc;

	if (!drv)
		return -ENODEV;

	cuart->rx_enabled = true;
	cuart->rx_threshold = 1;

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

int card_uart_ctrl(struct card_uart *cuart, enum card_uart_ctl ctl, bool enable)
{
	int rc;
	OSMO_ASSERT(cuart);
	OSMO_ASSERT(cuart->driver);
	OSMO_ASSERT(cuart->driver->ops);
	OSMO_ASSERT(cuart->driver->ops->ctrl);
	rc = cuart->driver->ops->ctrl(cuart, ctl, enable);
	if (rc < 0)
		return rc;

	switch (ctl) {
	case CUART_CTL_RX:
		cuart->rx_enabled = enable;
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
	/* disable receiver to avoid receiving what we transmit */
	card_uart_ctrl(cuart, CUART_CTL_RX, false);

	return cuart->driver->ops->async_tx(cuart, data, len, rx_after_complete);
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
		card_uart_ctrl(cuart, CUART_CTL_RX, true);
		break;
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
