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

volatile static uint32_t data_arrived = 0;

static void tx_cb_UART_debug(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
	gpio_toggle_pin_level(LED_system);
}

static void rx_cb_UART_debug(const struct usart_async_descriptor *const io_descr)
{
	/* Receive completed */
	gpio_toggle_pin_level(LED_system);
	data_arrived = 1;
}

int main(void)
{
	atmel_start_init();

	usart_async_register_callback(&UART_debug, USART_ASYNC_TXC_CB, tx_cb_UART_debug);
	usart_async_register_callback(&UART_debug, USART_ASYNC_RXC_CB, rx_cb_UART_debug);
	usart_async_enable(&UART_debug);

	cdcd_acm_example();
}
