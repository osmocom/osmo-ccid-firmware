/*
 * Copyright (C) 2019 Harald Welte <laforge@gnumonks.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <parts.h>

#include "atmel_start.h"
#include "atmel_start_pins.h"

#include "i2c_bitbang.h"
#include "octsim_i2c.h"
#include "ncn8025.h"

#include "command.h"

enum testmode_test {
	TEST_USER_LED,
	/* test the per-slot LED by blinking it shortly */
	TEST_LED,
	/* test the voltages of the SIMVCC */
	TEST_VOLTAGE,
	/* test the clock rates of the SIMCLK pin */
	TEST_CLOCK,
	/* test the RST line by asserting it low and then back high */
	TEST_RST,
	/* test the RST line by asserting it low and then back high */
	TEST_IO,
	_NUM_TESTS
};
static const char *test_names[_NUM_TESTS] = {
	[TEST_USER_LED]	= "USER_LED",
	[TEST_LED]	= "LED",
	[TEST_VOLTAGE]	= "VOLTAGE",
	[TEST_CLOCK]	= "CLOCK",
	[TEST_RST]	= "RST",
	[TEST_IO]	= "IO",
};

struct testmode_state {
	uint8_t slot;
	enum testmode_test test_nr;
	int test_int;
	struct ncn8025_settings ncn;
};
static struct testmode_state g_tms;

#define BLINK_MS 500

static void set_slot(uint8_t slot)
{
	printf("changing slot to %u\r\n", slot);
	g_tms.slot = slot;
	g_tms.ncn = (struct ncn8025_settings) {
		.rstin = false,
		.cmdvcc = false,
		.led = false,
		.clkdiv = SIM_CLKDIV_8,
		.vsel = SIM_VOLT_3V0,
	};
	ncn8025_set(g_tms.slot, &g_tms.ncn);
	ncn8025_get(g_tms.slot, &g_tms.ncn);
}

static void next_test(void)
{
	g_tms.test_nr = (g_tms.test_nr + 1) % _NUM_TESTS;
	g_tms.test_int = 0;
	printf("changing test to %s\r\n", test_names[g_tms.test_nr]);
}

static void test_user_led(void)
{
	printf("blinking User LED\r\n");

	gpio_set_pin_function(PIN_PC26, GPIO_PIN_FUNCTION_OFF);
	gpio_set_pin_direction(PIN_PC26, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PIN_PC26, true);
	delay_ms(BLINK_MS);
	gpio_set_pin_level(PIN_PC26, false);
}

static void test_led(void)
{
	printf("blinking Slot LED\r\n");

	g_tms.ncn.led = true;
	ncn8025_set(g_tms.slot, &g_tms.ncn);
	delay_ms(BLINK_MS);
	g_tms.ncn.led = false;
	ncn8025_set(g_tms.slot, &g_tms.ncn);
}

static enum ncn8025_sim_voltage voltage[3] = { SIM_VOLT_1V8, SIM_VOLT_3V0, SIM_VOLT_5V0 };
static const char *voltage_name[3] = { "1.8", "3.0", "5.0" };

static void ncn_change_voltage(enum ncn8025_sim_voltage vsel)
{
	/* first disable the output; VSEL changes require output to be disabled */
	g_tms.ncn.cmdvcc = false;
	ncn8025_set(g_tms.slot, &g_tms.ncn);

	/* then re-enable it with the new voltage setting */
	g_tms.ncn.vsel = vsel;
	g_tms.ncn.cmdvcc = true;
	ncn8025_set(g_tms.slot, &g_tms.ncn);
}

static void test_voltage(void)
{
	printf("Testing Voltage %s\r\n", voltage_name[g_tms.test_int]);

	ncn_change_voltage(voltage[g_tms.test_int]);
	g_tms.test_int = (g_tms.test_int+1) % 3;
}

static enum ncn8025_sim_clkdiv clk_div[4] = { SIM_CLKDIV_8, SIM_CLKDIV_4, SIM_CLKDIV_2, SIM_CLKDIV_1 };
static const uint8_t clk_div_val[4] = { 8, 4, 2, 1 };

static void test_clock(void)
{
	printf("Testing Clock Divider %u\r\n", clk_div_val[g_tms.test_int]);
	g_tms.ncn.cmdvcc = true;
	g_tms.ncn.clkdiv = clk_div[g_tms.test_int];
	ncn8025_set(g_tms.slot, &g_tms.ncn);
	g_tms.test_int = (g_tms.test_int+1) % 4;
}

static void test_rst(void)
{
	printf("blinking RST\r\n");

	/* well-defined voltage for LED brightness */
	ncn_change_voltage(SIM_VOLT_3V0);

	g_tms.ncn.cmdvcc = true;
	g_tms.ncn.rstin = true;
	ncn8025_set(g_tms.slot, &g_tms.ncn);

	delay_ms(BLINK_MS);

	g_tms.ncn.rstin = false;
	ncn8025_set(g_tms.slot, &g_tms.ncn);
}

#ifndef SIM7_IO
#define SIM7_IO	PIN_PB21
#endif
static const enum gpio_port sim_io_gpio[] = { SIM0_IO, SIM1_IO, SIM2_IO, SIM3_IO,
					      SIM4_IO, SIM5_IO, SIM6_IO, SIM7_IO };

static void test_io(void)
{
	enum gpio_port gpio = sim_io_gpio[g_tms.slot];
	printf("blinking I/O\r\n");

	/* well-defined voltage for LED brightness */
	ncn_change_voltage(SIM_VOLT_3V0);

	gpio_set_pin_function(gpio, GPIO_PIN_FUNCTION_OFF);
	gpio_set_pin_direction(gpio, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(gpio, false);
	delay_ms(BLINK_MS);
	gpio_set_pin_level(gpio, true);

	/* FIXME: restore tack to original function! */
	//gpio_set_pin_function(sim_io_gpio[g_tms.slot], GPIO_PIN_FUNCTION_OFF);
}

typedef void (*test_fn)(void);
static const test_fn test_functions[_NUM_TESTS] = {
	[TEST_USER_LED] = test_user_led,
	[TEST_LED] = test_led,
	[TEST_VOLTAGE] = test_voltage,
	[TEST_CLOCK] = test_clock,
	[TEST_RST] = test_rst,
	[TEST_IO] = test_io,
};

static void execute_test(void)
{
	printf("(%u) %-10s: ", g_tms.slot, test_names[g_tms.test_nr]);
	test_functions[g_tms.test_nr]();
}

static int wait_for_key_and_process(void)
{
	int c;

	do {
	} while (!usart_async_rings_is_rx_not_empty(&UART_debug));

	c = getchar();
	if (c < 0)
		return -1;

	switch (c) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		set_slot(c - '0');
		execute_test();
		break;
	case 'n':
	case 'N':
		next_test();
		execute_test();
		break;
	case 'Q':
	case 'q':
		printf("Leaving Test Mode\r\n");
		return -1;
	case ' ':
		execute_test();
		break;
	}
	return 0;
}

DEFUN(testmode_fn, cmd_testmode,
	"testmode", "Enter board testing mode (Use `Q` to exit)")
{
	printf("Manual test mode. 'Q': exit, 'N': next test, SPACE: re-run, '0'-'7': slot\r\n");

	printf("SPACE will start the current test (%s)\r\n", test_names[g_tms.test_nr]);
	while (1) {
		if (wait_for_key_and_process() < 0)
			break;
	}
}

void testmode_init(void)
{
	command_register(&cmd_testmode);
}
