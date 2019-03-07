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

#include <stdlib.h>
#include <stdio.h>
#include <parts.h>
#include <hal_cache.h>
#include <hri_port_e54.h>

#include "atmel_start.h"
#include "atmel_start_pins.h"

#include "i2c_bitbang.h"
#include "octsim_i2c.h"
#include "ncn8025.h"

#include "command.h"

// TODO put declaration in more global file
// TODO for now SIM7 is not present because used for debug
static struct usart_async_descriptor* SIM_peripheral_descriptors[] = {&SIM0, &SIM1, &SIM2, &SIM3, &SIM4, &SIM5, &SIM6, NULL};

static void SIM_rx_cb(const struct usart_async_descriptor *const io_descr)
{
}

static void board_init()
{
	int i;

	for (i = 0; i < 4; i++)
		i2c_init(&i2c[i]);

	for (i = 0; i < 8; i++)
		ncn8025_init(i);

	cache_init();
	cache_enable(CMCC);

	/* increase drive strength of 20Mhz SIM clock output to 8mA
	 * (there are 8 inputs + traces to drive!) */
	hri_port_set_PINCFG_DRVSTR_bit(PORT, 0, 11);

	// enable SIM interfaces
	for (uint8_t i = 0; i < ARRAY_SIZE(SIM_peripheral_descriptors); i++) {
		if (NULL == SIM_peripheral_descriptors[i]) {
			continue;
		}
		usart_async_register_callback(SIM_peripheral_descriptors[i], USART_ASYNC_RXC_CB, SIM_rx_cb); // required for RX to work, even if the callback does nothing
		usart_async_enable(SIM_peripheral_descriptors[i]);
	}
}

static int validate_slotnr(int argc, char **argv, int idx)
{
	int slotnr;
	if (argc < idx+1) {
		printf("You have to specify the slot number (0..7)\r\n");
		return -1;
	}
	slotnr = atoi(argv[idx]);
	if (slotnr < 0 || slotnr > 7) {
		printf("You have to specify the slot number (0..7)\r\n");
		return -1;
	}
	return slotnr;
}

DEFUN(sim_status, cmd_sim_status, "sim-status", "Get state of specified NCN8025")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	if (slotnr < 0)
		return;
	ncn8025_get(slotnr, &settings);
	printf("SIM%d: ", slotnr);
	ncn8025_dump(&settings);
	printf("\r\n");
}

DEFUN(sim_power, cmd_sim_power, "sim-power", "Enable/disable SIM card power")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	int enable;

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify 0=disable or 1=enable\r\n");
		return;
	}
	enable = atoi(argv[2]);
	ncn8025_get(slotnr, &settings);
	if (enable)
		settings.cmdvcc = true;
	else
		settings.cmdvcc = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_reset, cmd_sim_reset, "sim-reset", "Enable/disable SIM reset")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	int enable;

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify 0=disable or 1=enable\r\n");
		return;
	}
	enable = atoi(argv[2]);
	ncn8025_get(slotnr, &settings);
	if (enable)
		settings.rstin = true;
	else
		settings.rstin = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_clkdiv, cmd_sim_clkdiv, "sim-clkdiv", "Set SIM clock divider (1,2,4,8)")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	int clkdiv;

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify a valid divider (1,2,4,8)\r\n");
		return;
	}
	clkdiv = atoi(argv[2]);
	if (clkdiv != 1 && clkdiv != 2 && clkdiv != 4 && clkdiv != 8) {
		printf("You have to specify a valid divider (1,2,4,8)\r\n");
		return;
	}
	ncn8025_get(slotnr, &settings);
	switch (clkdiv) {
	case 1:
		settings.clkdiv = SIM_CLKDIV_1;
		break;
	case 2:
		settings.clkdiv = SIM_CLKDIV_2;
		break;
	case 4:
		settings.clkdiv = SIM_CLKDIV_4;
		break;
	case 8:
		settings.clkdiv = SIM_CLKDIV_8;
		break;
	}
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_voltage, cmd_sim_voltage, "sim-voltage", "Set SIM voltage (5/3/1.8)")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify a valid voltage (5/3/1.8)\r\n");
		return;
	}
	ncn8025_get(slotnr, &settings);
	if (!strcmp(argv[2], "5"))
		settings.vsel = SIM_VOLT_5V0;
	else if (!strcmp(argv[2], "3"))
		settings.vsel = SIM_VOLT_3V0;
	else if (!strcmp(argv[2], "1.8"))
		settings.vsel = SIM_VOLT_1V8;
	else {
		printf("You have to specify a valid voltage (5/3/1.8)\r\n");
		return;
	}
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_led, cmd_sim_led, "sim-led", "Set SIM LED (1=on, 0=off)")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify 0=disable or 1=enable\r\n");
		return;
	}
	ncn8025_get(slotnr, &settings);
	if (atoi(argv[2]))
		settings.led = true;
	else
		settings.led = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_atr, cmd_sim_atr, "sim-atr", "Read ATR from SIM card")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);

	if (slotnr < 0 || slotnr >= ARRAY_SIZE(SIM_peripheral_descriptors) || NULL == SIM_peripheral_descriptors[slotnr]) {
		return;
	}

	// check if card is present (and read current settings)
	ncn8025_get(slotnr, &settings);
	if (!settings.simpres) {
		printf("no card present in slot %d, aborting\r\n", slotnr);
		return;
	}

	// switch card off (assert reset and disable power)
	// note: ISO/IEC 7816-3:2006 section 6.4 provides the deactivation sequence, but not the minimum corresponding times
	settings.rstin = true;
	settings.cmdvcc = false;
	settings.led = true;
	ncn8025_set(slotnr, &settings);

	// TODO wait some time for card to be completely deactivated
	usart_async_flush_rx_buffer(SIM_peripheral_descriptors[slotnr]); // flush RX buffer to start from scratch

	//usart_async_set_baud_rate(SIM_peripheral_descriptors[slotnr], 2500000 / (372 / 1)); // set USART baud rate to match the interface (f = 2.5 MHz) and card default settings (Fd = 372, Dd = 1)
	// set clock to lowest frequency (20 MHz / 8 = 2.5 MHz)
	// note: according to ISO/IEC 7816-3:2006 section 5.2.3 the minimum value is 1 MHz, and maximum is 5 MHz during activation
	settings.clkdiv = SIM_CLKDIV_8;
	// set card voltage to 3.0 V (the most supported)
	// note: according to ISO/IEC 7816-3:2006 no voltage should damage the card, and you should cycle from low to high
	settings.vsel = SIM_VOLT_3V0;
	// provide power (the NCN8025 should perform the activation according to spec)
	// note: activation sequence is documented in ISO/IEC 7816-3:2006 section 6.2
	settings.cmdvcc = true;
	ncn8025_set(slotnr, &settings);

	// wait for Tb=400 cycles before re-asserting reset
	delay_us(400 * 10000 / 2500); // 400 cycles * 1000 for us, 2.5 MHz / 1000 for us

	// de-assert reset to switch card back on
	settings.rstin = false;
	ncn8025_set(slotnr, &settings);

	// wait for Tc=40000 cycles for transmission to start
	uint32_t cycles = 40000;
	while (cycles && !usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		delay_us(10);
		cycles -= 25; // 10 us = 25 cycles at 2.5 MHz
	}
	if (!usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		delay_us(12 * 372 / 1 / 2); // wait more than one byte (approximate freq down to 2 MHz)
	}
	// verify if one byte has been received
	if (!usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		printf("card in slot %d is not responding, aborting\r\n", slotnr);
		return;
	}

	// read ATR (just do it until there is no traffic anymore)
	// TODO the ATR should be parsed to read the right number of bytes
	printf("(%d) ATR: ", slotnr);
	uint8_t atr_byte;
	while (usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		if (1 == io_read(&SIM_peripheral_descriptors[slotnr]->io, &atr_byte, 1)) {
			printf("%02x ", atr_byte);
		}
		uint16_t wt = 9600; // waiting time in ETU
		while (wt && !usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
			delay_us(149); // wait for 1 ETU (372 / 1 / 2.5 MHz = 148.8 us)
			wt--;
		}
	}
	printf("\r\n");

	/* disable VCC and LED, re-enable RST */
	settings.cmdvcc = false;
	settings.rstin = true;
	settings.led = false;
	ncn8025_set(slotnr, &settings);
}

extern void testmode_init(void);

int main(void)
{
	atmel_start_init();

	usb_start();

	board_init();
	command_init("sysmoOCTSIM> ");
	command_register(&cmd_sim_status);
	command_register(&cmd_sim_power);
	command_register(&cmd_sim_reset);
	command_register(&cmd_sim_clkdiv);
	command_register(&cmd_sim_voltage);
	command_register(&cmd_sim_led);
	command_register(&cmd_sim_atr);
	testmode_init();

	printf("\r\n\r\nsysmocom sysmoOCTSIM\r\n");

	command_print_prompt();
	while (true) { // main loop
		command_try_recv();
	}
}
