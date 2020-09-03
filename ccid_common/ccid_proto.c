/* CCID Protocol related Definitions
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
#include "ccid_proto.h"

const struct value_string ccid_msg_type_vals[] = {
	OSMO_VALUE_STRING(RDR_to_PC_NotifySlotChange),
	OSMO_VALUE_STRING(RDR_to_PC_HardwareError),
	OSMO_VALUE_STRING(PC_to_RDR_IccPowerOn),
	OSMO_VALUE_STRING(PC_to_RDR_IccPowerOff),
	OSMO_VALUE_STRING(PC_to_RDR_GetSlotStatus),
	OSMO_VALUE_STRING(PC_to_RDR_XfrBlock),
	OSMO_VALUE_STRING(PC_to_RDR_GetParameters),
	OSMO_VALUE_STRING(PC_to_RDR_ResetParameters),
	OSMO_VALUE_STRING(PC_to_RDR_SetParameters),
	OSMO_VALUE_STRING(PC_to_RDR_Escape),
	OSMO_VALUE_STRING(PC_to_RDR_IccClock),
	OSMO_VALUE_STRING(PC_to_RDR_T0APDU),
	OSMO_VALUE_STRING(PC_to_RDR_Secure),
	OSMO_VALUE_STRING(PC_to_RDR_Mechanical),
	OSMO_VALUE_STRING(PC_to_RDR_Abort),
	OSMO_VALUE_STRING(PC_to_RDR_SetDataRateAndClockFrequency),
	OSMO_VALUE_STRING(RDR_to_PC_DataBlock),
	OSMO_VALUE_STRING(RDR_to_PC_SlotStatus),
	OSMO_VALUE_STRING(RDR_to_PC_Parameters),
	OSMO_VALUE_STRING(RDR_to_PC_Escape),
	OSMO_VALUE_STRING(RDR_to_PC_DataRateAndClockFrequency),
	{ 0, NULL }
};

const struct value_string ccid_class_spec_req_vals[] = {
	{ CLASS_SPEC_CCID_ABORT, "CCID_ABORT" },
	{ CLASS_SPEC_CCID_GET_CLOCK_FREQ, "GET_CLOCK_FREQ" },
	{ CLASS_SPEC_CCID_GET_DATA_RATES, "GET_DATA_RATES" },
	{ 0, NULL }
};

const struct value_string ccid_power_select_vals[] = {
	{ CCID_PWRSEL_AUTO, "AUTO" },
	{ CCID_PWRSEL_5V0, "5.0V" },
	{ CCID_PWRSEL_3V0, "3.0V" },
	{ CCID_PWRSEL_1V8, "1.8V" },
	{ 0, NULL }
};

const struct value_string ccid_clock_command_vals[] = {
	{ CCID_CLOCK_CMD_RESTART, "RESTART" },
	{ CCID_CLOCK_CMD_STOP, "STOP" },
	{ 0, NULL }
};

const struct value_string ccid_error_code_vals[] = {
	{ CCID_ERR_CMD_ABORTED, 		"CMD_ABORTED" },
	{ CCID_ERR_ICC_MUTE,			"ICC_MUTE" },
	{ CCID_ERR_XFR_PARITY_ERROR,		"XFR_PARITY_ERROR" },
	{ CCID_ERR_XFR_OVERRUN,			"XFR_OVERRUN" },
	{ CCID_ERR_HW_ERROR,			"HW_ERROR" },
	{ CCID_ERR_BAD_ATR_TS,			"BAD_ATR_TS" },
	{ CCID_ERR_BAD_ATR_TCK,			"BAD_ATR_TCK" },
	{ CCID_ERR_ICC_PROTOCOL_NOT_SUPPORTED,	"ICC_PROTOCOL_NOT_SUPPORTED" },
	{ CCID_ERR_ICC_CLASS_NOT_SUPPORTED,	"ICC_CLASS_NOT_SUPPORTED" },
	{ CCID_ERR_PROCEDURE_BYTE_CONFLICT,	"PROCEDURE_BYTE_CONFLICT" },
	{ CCID_ERR_DEACTIVATED_PROTOCOL,	"DEACTIVATED_PROTOCOL" },
	{ CCID_ERR_BUSY_WITH_AUTO_SEQUENCE,	"BUSY_WITH_AUTO_SEQUENCE" },
	{ CCID_ERR_PIN_TIMEOUT,			"PIN_TIMEOUT" },
	{ CCID_ERR_PIN_CANCELLED,		"PIN_CANCELLED" },
	{ CCID_ERR_CMD_SLOT_BUSY,		"CMD_SLOT_BUSY" },
	{ CCID_ERR_CMD_NOT_SUPPORTED,		"CMD_NOT_SUPPORTED" },
	{ 0, NULL }
};
