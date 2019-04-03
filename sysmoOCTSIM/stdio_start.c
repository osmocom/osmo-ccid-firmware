/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file or main.c
 * to avoid loosing it when reconfiguring.
 */

#include "atmel_start.h"
#include "stdio_start.h"

static void UART_debug_rx_cb(const struct usart_async_rings_descriptor *const io_descr)
{
}

void stdio_redirect_init(void)
{
	usart_async_rings_register_callback(&UART_debug, USART_ASYNC_RXC_CB, UART_debug_rx_cb); // if no callback function is registered receive won't work, even if the callback does nothing
	usart_async_rings_enable(&UART_debug);
	stdio_io_init(&UART_debug.io);
}
