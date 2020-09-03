/* libosmocore logging integration
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


#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include "logging.h"

static const struct log_info_cat log_info_cat[] = {
	[DUSB] = {
		.name = "USB",
		.description = "USB Transport",
		.enabled = 1,
		.loglevel = LOGL_NOTICE,
	},
	[DCCID] = {
		.name = "CCID",
		.description = "USB-CCID Protocol",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DISO7816] = {
		.name = "ISO7816",
		.description = "ISO7816-3 State machines",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DATR] = {
		.name = "ATR",
		.description = "ATR (Answer To Reset) FSM",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DTPDU] = {
		.name = "TPDU",
		.description = "TPDU FSM",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DPPS] = {
		.name = "PPS",
		.description = "PPS (Protocol and Parameter Selection) FSM",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
};

const struct log_info log_info = {
	.cat = log_info_cat,
	.num_cat = ARRAY_SIZE(log_info_cat),
};
