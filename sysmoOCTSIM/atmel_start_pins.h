/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef ATMEL_START_PINS_H_INCLUDED
#define ATMEL_START_PINS_H_INCLUDED

#include <hal_gpio.h>

// SAME54 has 14 pin functions

#define GPIO_PIN_FUNCTION_A 0
#define GPIO_PIN_FUNCTION_B 1
#define GPIO_PIN_FUNCTION_C 2
#define GPIO_PIN_FUNCTION_D 3
#define GPIO_PIN_FUNCTION_E 4
#define GPIO_PIN_FUNCTION_F 5
#define GPIO_PIN_FUNCTION_G 6
#define GPIO_PIN_FUNCTION_H 7
#define GPIO_PIN_FUNCTION_I 8
#define GPIO_PIN_FUNCTION_J 9
#define GPIO_PIN_FUNCTION_K 10
#define GPIO_PIN_FUNCTION_L 11
#define GPIO_PIN_FUNCTION_M 12
#define GPIO_PIN_FUNCTION_N 13

#define SIM0_IO GPIO(GPIO_PORTA, 4)
#define SIM2_IO GPIO(GPIO_PORTA, 9)
#define SIMCLK_20MHZ GPIO(GPIO_PORTA, 11)
#define SIM1_IO GPIO(GPIO_PORTA, 16)
#define VB0 GPIO(GPIO_PORTA, 20)
#define VB1 GPIO(GPIO_PORTA, 21)
#define VB2 GPIO(GPIO_PORTA, 22)
#define VB3 GPIO(GPIO_PORTA, 23)
#define USBUP_D_N GPIO(GPIO_PORTA, 24)
#define USBUP_D_P GPIO(GPIO_PORTA, 25)
#define SIM4_IO GPIO(GPIO_PORTB, 8)
#define SIM5_IO GPIO(GPIO_PORTB, 16)
#define SIM3_IO GPIO(GPIO_PORTB, 20)
#define UART_TX GPIO(GPIO_PORTB, 30)
#define UART_RX GPIO(GPIO_PORTB, 31)
#define SWITCH GPIO(GPIO_PORTC, 14)
#define SIM6_IO GPIO(GPIO_PORTC, 16)
#define USER_LED GPIO(GPIO_PORTC, 26)

#endif // ATMEL_START_PINS_H_INCLUDED
