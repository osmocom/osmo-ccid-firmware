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
 * Example of using SIM0 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SIM0[12] = "Hello World!";

static void tx_cb_SIM0(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void SIM0_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&SIM0, USART_ASYNC_TXC_CB, tx_cb_SIM0);
	/*usart_async_register_callback(&SIM0, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&SIM0, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&SIM0, &io);
	usart_async_enable(&SIM0);

	io_write(io, example_SIM0, 12);
}

/**
 * Example of using SIM1 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SIM1[12] = "Hello World!";

static void tx_cb_SIM1(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void SIM1_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&SIM1, USART_ASYNC_TXC_CB, tx_cb_SIM1);
	/*usart_async_register_callback(&SIM1, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&SIM1, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&SIM1, &io);
	usart_async_enable(&SIM1);

	io_write(io, example_SIM1, 12);
}

/**
 * Example of using SIM2 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SIM2[12] = "Hello World!";

static void tx_cb_SIM2(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void SIM2_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&SIM2, USART_ASYNC_TXC_CB, tx_cb_SIM2);
	/*usart_async_register_callback(&SIM2, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&SIM2, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&SIM2, &io);
	usart_async_enable(&SIM2);

	io_write(io, example_SIM2, 12);
}

/**
 * Example of using SIM3 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SIM3[12] = "Hello World!";

static void tx_cb_SIM3(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void SIM3_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&SIM3, USART_ASYNC_TXC_CB, tx_cb_SIM3);
	/*usart_async_register_callback(&SIM3, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&SIM3, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&SIM3, &io);
	usart_async_enable(&SIM3);

	io_write(io, example_SIM3, 12);
}

/**
 * Example of using SIM4 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SIM4[12] = "Hello World!";

static void tx_cb_SIM4(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void SIM4_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&SIM4, USART_ASYNC_TXC_CB, tx_cb_SIM4);
	/*usart_async_register_callback(&SIM4, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&SIM4, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&SIM4, &io);
	usart_async_enable(&SIM4);

	io_write(io, example_SIM4, 12);
}

/**
 * Example of using SIM5 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SIM5[12] = "Hello World!";

static void tx_cb_SIM5(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void SIM5_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&SIM5, USART_ASYNC_TXC_CB, tx_cb_SIM5);
	/*usart_async_register_callback(&SIM5, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&SIM5, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&SIM5, &io);
	usart_async_enable(&SIM5);

	io_write(io, example_SIM5, 12);
}

/**
 * Example of using SIM6 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SIM6[12] = "Hello World!";

static void tx_cb_SIM6(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void SIM6_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&SIM6, USART_ASYNC_TXC_CB, tx_cb_SIM6);
	/*usart_async_register_callback(&SIM6, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&SIM6, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&SIM6, &io);
	usart_async_enable(&SIM6);

	io_write(io, example_SIM6, 12);
}

/**
 * Example of using UART_debug to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_UART_debug[12] = "Hello World!";

static void tx_cb_UART_debug(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
}

void UART_debug_example(void)
{
	struct io_descriptor *io;

	usart_async_register_callback(&UART_debug, USART_ASYNC_TXC_CB, tx_cb_UART_debug);
	/*usart_async_register_callback(&UART_debug, USART_ASYNC_RXC_CB, rx_cb);
	usart_async_register_callback(&UART_debug, USART_ASYNC_ERROR_CB, err_cb);*/
	usart_async_get_io_descriptor(&UART_debug, &io);
	usart_async_enable(&UART_debug);

	io_write(io, example_UART_debug, 12);
}
