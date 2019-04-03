/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef DRIVER_INIT_INCLUDED
#define DRIVER_INIT_INCLUDED

#include "atmel_start_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <hal_atomic.h>
#include <hal_delay.h>
#include <hal_gpio.h>
#include <hal_init.h>
#include <hal_io.h>
#include <hal_sleep.h>

#include <hal_usart_sync.h>
#include <hal_usart_async.h>
#include <hal_usart_async_rings.h>

#include "hal_usb_device.h"

extern struct usart_async_descriptor SIM0;
extern struct usart_async_descriptor SIM1;
extern struct usart_async_descriptor SIM2;
extern struct usart_async_descriptor SIM3;
extern struct usart_async_descriptor SIM4;
extern struct usart_async_descriptor SIM5;
extern struct usart_async_descriptor SIM6;
extern struct usart_async_rings_descriptor UART_debug;

void SIM0_PORT_init(void);
void SIM0_CLOCK_init(void);
void SIM0_init(void);

void SIM1_PORT_init(void);
void SIM1_CLOCK_init(void);
void SIM1_init(void);

void SIM2_PORT_init(void);
void SIM2_CLOCK_init(void);
void SIM2_init(void);

void SIM3_PORT_init(void);
void SIM3_CLOCK_init(void);
void SIM3_init(void);

void SIM4_PORT_init(void);
void SIM4_CLOCK_init(void);
void SIM4_init(void);

void SIM5_PORT_init(void);
void SIM5_CLOCK_init(void);
void SIM5_init(void);

void SIM6_PORT_init(void);
void SIM6_CLOCK_init(void);
void SIM6_init(void);

void UART_debug_PORT_init(void);
void UART_debug_CLOCK_init(void);
void UART_debug_init(void);

void USB_DEVICE_INSTANCE_CLOCK_init(void);
void USB_DEVICE_INSTANCE_init(void);

/**
 * \brief Perform system initialization, initialize pins and clocks for
 * peripherals
 */
void system_init(void);

#ifdef __cplusplus
}
#endif
#endif // DRIVER_INIT_INCLUDED
