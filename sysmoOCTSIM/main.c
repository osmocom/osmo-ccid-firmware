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

int main(void)
{
	atmel_start_init();

	usart_sync_enable(&UART_debug);

	usb_start();

	board_init();
	command_init("sysmoOCTSIM> ");
	command_register(&cmd_hello);

	printf("\r\n\r\nsysmocom sysmoOCTSIM\r\n");
	while (true) { // main loop
		command_try_recv();
	}
}
