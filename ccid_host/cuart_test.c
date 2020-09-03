/* Card UART (cuart) teset program
 *  - this tests only the CUART layer, without any FSM on top
 *
 * (C) 2019-2020 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA
 */


#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <osmocom/core/utils.h>

#include "cuart.h"

static struct card_uart g_cuart;


static void handle_event(struct card_uart *cuart, enum card_uart_event evt, void *data)
{
	printf("Handle Event '%s'\n", get_value_string(card_uart_event_vals, evt));
	switch (evt) {
	case CUART_E_RX_SINGLE:
		printf("\t%02x\n", *(uint8_t *)data);
		break;
	}

}


static int get_atr(uint8_t *atr, size_t max_len)
{
	int rc;
	int i = 0;

	card_uart_ctrl(&g_cuart, CUART_CTL_RST, true);
	usleep(100000);
	card_uart_ctrl(&g_cuart, CUART_CTL_RST, false);

	sleep(1);
	osmo_select_main(true);

	for (i = 0; i < max_len; i++) {
		rc = card_uart_rx(&g_cuart, &atr[i], 1);
		if (rc <= 0)
			break;
	}

	return i;
}

static void test_apdu(void)
{
	const uint8_t select_mf[] = "\xa0\xa4\x04\x00\x02\x3f\x00";
	card_uart_tx(&g_cuart, select_mf, 5, true);

	osmo_select_main(true);
	sleep(1);
	osmo_select_main(true);
	/* we should get an RX_SINGLE event here */
}


int main(int argc, char **argv)
{
	uint8_t atr[64];
	int rc;

	if (argc < 2) {
		fprintf(stderr, "You must specify the UART tty device as argument\n");
		exit(2);
	}

	printf("Opening UART device %s\n", argv[1]);

	g_cuart.handle_event = &handle_event;
	rc = card_uart_open(&g_cuart, "tty", argv[1]);
	if (rc < 0) {
		perror("opening UART");
		exit(1);
	}

	rc = get_atr(atr, sizeof(atr));
	if (rc < 0) {
		perror("getting ATR");
		exit(1);
	}
	printf("ATR: %s\n", osmo_hexdump(atr, rc));

	test_apdu();

	exit(0);
}
