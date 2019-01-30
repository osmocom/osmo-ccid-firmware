/*
 * Copyright (C) 2019 sysmocom -s.f.m.c. GmbH, Author: Kevin Redon <kredon@sysmocom.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "atmel_start.h"
#include "atmel_start_pins.h"

volatile static bool data_arrived = false;

static void tx_cb_UART_debug(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
	//gpio_toggle_pin_level(LED_system);
}

static void rx_cb_UART_debug(const struct usart_async_descriptor *const io_descr)
{
	/* Receive completed */
	gpio_toggle_pin_level(USER_LED);
	data_arrived = true;
}

int main(void)
{
	atmel_start_init();

	usart_async_register_callback(&UART_debug, USART_ASYNC_TXC_CB, tx_cb_UART_debug);
	usart_async_register_callback(&UART_debug, USART_ASYNC_RXC_CB, rx_cb_UART_debug);
	usart_async_enable(&UART_debug);

	usb_start();

	while (true) { // main loop
		if (data_arrived) { // input on UART debug
			data_arrived = false; // clear flag
			uint8_t recv_char; // to store the input
			while (io_read(&UART_debug.io, &recv_char, 1) == 1) { // read input
				while (io_write(&UART_debug.io, &recv_char, 1) != 1); // echo back to output
			}
		}
	}
}
