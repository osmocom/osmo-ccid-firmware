/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */

#include "driver_examples.h"
#include "driver_init.h"
#include "utils.h"

/**
 * Example of using UART_debug to write "Hello World" using the IO abstraction.
 */
void UART_debug_example(void)
{
	struct io_descriptor *io;
	usart_sync_get_io_descriptor(&UART_debug, &io);
	usart_sync_enable(&UART_debug);

	io_write(io, (uint8_t *)"Hello World!", 12);
}
