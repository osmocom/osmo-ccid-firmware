/* CCID Control program
 * - quick hack used to send CCID control transfers to a CCID device
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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>

static libusb_context *g_uctx;
static uint16_t g_vendor_id = 0x1d50;
static uint16_t g_product_id = 0x615f;
static uint8_t g_interface = 0;

#define GET_CLOCK_FREQS	0x02
#define GET_DATA_RATES	0x03

static int usb_ctrl_get_dwords(libusb_device_handle *devh, uint8_t req_t, uint8_t rq,
				uint16_t val, uint16_t index, unsigned int num_dword)
{
	uint32_t buf[num_dword];
	unsigned int i;
	int rc;

	rc = libusb_control_transfer(devh, req_t, rq, val, index, (uint8_t *)buf, sizeof(buf), 1000);
	if (rc < 0) {
		printf("control_transfer: %s\n", libusb_strerror(rc));
		return rc;
	}

	for (i = 0; i < num_dword; i++)
		printf("\t%u\n", buf[i]);

	return 0;
}


int main(int argc, char **argv)
{
	libusb_device_handle *devh;
	int rc;

	rc = libusb_init(&g_uctx);
	if (rc < 0) {
		fprintf(stderr, "Cannot init libusb\n");
		exit(1);
	}

	devh = libusb_open_device_with_vid_pid(g_uctx, g_vendor_id, g_product_id);
	if (!devh) {
		fprintf(stderr, "Cannot open usb device %04x:%04x\n", g_vendor_id, g_product_id);
		exit(1);
	}

	printf("Clock Frequencies:\n");
	usb_ctrl_get_dwords(devh, 0xA1, GET_CLOCK_FREQS, 0, g_interface, 1);
	printf("\n");

	printf("Data Rates\n");
	usb_ctrl_get_dwords(devh, 0xA1, GET_DATA_RATES, 0, g_interface, 1);
	printf("\n");

	libusb_close(devh);
	exit(0);
}

