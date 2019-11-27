#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/timer.h>

#include <osmocom/core/select.h>
#include "utils_ringbuffer.h"

struct usart_async_descriptor;

enum card_uart_event {
	/* a single byte was received, it's present at the (uint8_t *) data location */
	CUART_E_RX_SINGLE,
	/* an entire block of data was received */
	CUART_E_RX_COMPLETE,
	CUART_E_RX_TIMEOUT,
	/* an entire block of data was written, as instructed in prior card_uart_tx() call */
	CUART_E_TX_COMPLETE,
};

extern const struct value_string card_uart_event_vals[];

enum card_uart_ctl {
	CUART_CTL_RX,		/* enable/disable receiver */
	CUART_CTL_NO_RXTX,		/* enable/disable receiver */
	CUART_CTL_POWER,	/* enable/disable ICC power */
	CUART_CTL_CLOCK,	/* enable/disable ICC clock */
	CUART_CTL_SET_CLOCK_FREQ, /* set ICC clock frequency (hz)*/
	CUART_CTL_RST,		/* enable/disable ICC reset */
	CUART_CTL_WTIME,	/* set the waiting time (in etu) */
	CUART_CTL_SET_FD,
};

struct card_uart;

struct card_uart_ops {
	int (*open)(struct card_uart *cuart, const char *device_name);
	int (*close)(struct card_uart *cuart);
	int (*async_tx)(struct card_uart *cuart, const uint8_t *data, size_t len);
	int (*async_rx)(struct card_uart *cuart, uint8_t *data, size_t len);

	int (*ctrl)(struct card_uart *cuart, enum card_uart_ctl ctl, int arg);
};

/* Card UART driver */
struct card_uart_driver {
	/* global list of card UART drivers */
	struct llist_head list;
	/* human-readable name of driver */
	const char *name;
	/* operations implementing the driver */
	const struct card_uart_ops *ops;
};

struct card_uart {
	/* member in global list of UARTs */
	struct llist_head list;
	/* driver serving this UART */
	const struct card_uart_driver *driver;
	/* event-handler function */
	void (*handle_event)(struct card_uart *cuart, enum card_uart_event evt, void *data);
	/* opaque pointer for user */
	void *priv;

	/* is the transmitter currently busy (true) or not (false)? */
	bool tx_busy;
	/* is the receiver currently enabled or not? */
	bool rx_enabled;
	/* should the receiver automatically be nabled after TX completion? */
	bool rx_after_tx_compl;

	/*! after how many bytes should we notify the user? If this is '1', we will
	 *  issue CUART_E_RX_SINGLE; if it is > 1, we will issue CUART_E_RX_COMPLETE */
	uint32_t rx_threshold;

	uint32_t wtime_etu;
	struct osmo_timer_list wtime_tmr;

	/* driver-specific private data */
	union {
		struct {
			/* ringbuffer on receive side */
			uint8_t rx_buf[256];
			struct ringbuffer rx_ringbuf;

			/* pointer to (user-allocated) transmit buffer and length */
			const uint8_t *tx_buf;
			size_t tx_buf_len;
			/* index: offset of next to be transmitted byte in tx_buf */
			size_t tx_index;
			/* number of bytes we have received echoed back during transmit */
			uint32_t rx_count_during_tx;

			struct osmo_fd ofd;
			unsigned int baudrate;
		} tty;
		struct {
			struct usart_async_descriptor *usa_pd;
			uint8_t slot_nr;
			/* in us, required, no delay breaks _rx_ */
			uint32_t extrawait_after_rx;
		} asf4;
	} u;
};

/*! Open the Card UART */
int card_uart_open(struct card_uart *cuart, const char *driver_name, const char *device_name);

/*! Close the Card UART */
int card_uart_close(struct card_uart *cuart);

/*! Schedule (asynchronous) transmit data via UART; optionally enable Rx after completion */
int card_uart_tx(struct card_uart *cuart, const uint8_t *data, size_t len, bool rx_after_complete);

/*! Schedule (asynchronous) receive data via UART (after CUART_E_RX_COMPLETE) */
int card_uart_rx(struct card_uart *cuart, uint8_t *data, size_t len);

int card_uart_ctrl(struct card_uart *cuart, enum card_uart_ctl ctl, int arg);

/*! Set the Rx notification threshold in number of bytes received */
void card_uart_set_rx_threshold(struct card_uart *cuart, size_t rx_threshold);

/* (re)start the software WTIME timer */
void card_uart_wtime_restart(struct card_uart *cuart);

void card_uart_notification(struct card_uart *cuart, enum card_uart_event evt, void *data);

int card_uart_driver_register(struct card_uart_driver *drv);
