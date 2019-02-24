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


static void board_init()
{
	int i;

	for (i = 0; i < 4; i++)
		i2c_init(&i2c[i]);

	/* only 7 slots, as last slot is debug uart! */
	for (i = 0; i < 7; i++)
		ncn8025_init(i);

	cache_init();
	cache_enable(CMCC);

	/* increase drive strength of 20Mhz SIM clock output to 8mA
	 * (there are 8 inputs + traces to drive!) */
	hri_port_set_PINCFG_DRVSTR_bit(PORT, 0, 11);
}

DEFUN(hello_fn, cmd_hello,
	"hello", "Hello World example command")
{
	printf("Hello World!\r\n");
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





int main(void)
{
	atmel_start_init();

	usart_sync_enable(&UART_debug);

	usb_start();

	board_init();
	command_init("sysmoOCTSIM> ");
	command_register(&cmd_hello);
	command_register(&cmd_sim_status);
	command_register(&cmd_sim_power);
	command_register(&cmd_sim_reset);
	command_register(&cmd_sim_clkdiv);
	command_register(&cmd_sim_voltage);
	command_register(&cmd_sim_led);

	printf("\r\n\r\nsysmocom sysmoOCTSIM\r\n");
	while (true) { // main loop
		command_try_recv();
	}
}
