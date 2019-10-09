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
