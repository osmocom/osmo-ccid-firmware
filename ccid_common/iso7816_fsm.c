/* ISO 7816-3 Finite State Machine (reader side)
 *
 * (C) 2019 by Harald Welte <laforge@gnumonks.org>
 *
 * inspired by earlier work
 * (C) 2016-2017 by Harald Welte <hwelte@hmw-consulting.de>
 * (C) 2018 by sysmocom -s.f.m.c. GmbH, Author: Kevin Redon <kredon@sysmocom.de>
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

#include <string.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/logging.h>
#include <osmocom/sim/sim.h>

#include "logging.h"
#include "cuart.h"
#include "iso7816_fsm.h"

/* Section 8.2: the Answer-to-Reset (... a string of at most 32 bytes) */
#define MAX_ATR_SIZE 32

#define S(x)	(1 << (x))

/*! ISO 7816-3 states */
enum iso7816_3_state {
	ISO7816_S_RESET, /*!< in Reset */
	ISO7816_S_WAIT_ATR, /*!< waiting for ATR to start */
	ISO7816_S_IN_ATR, /*!< while we are receiving the ATR */
	ISO7816_S_WAIT_TPDU, /*!< waiting for start of new TPDU */
	ISO7816_S_IN_TPDU, /*!< inside a single TPDU */
	ISO7816_S_WAIT_PPS_RSP, /*!< waiting for start of the PPS response */
	ISO7816_S_IN_PPS_RSP, /*!< while we are inside the PPS request */
};

/*! Answer-To-Reset (ATR) sub-states of ISO7816_S_IN_ATR
 *  @note defined in ISO/IEC 7816-3:2006(E) section 8
 */
enum atr_state {
	ATR_S_WAIT_TS, /*!< initial byte */
	ATR_S_WAIT_T0, /*!< format byte */
	ATR_S_WAIT_TA, /*!< first sub-group interface byte */
	ATR_S_WAIT_TB, /*!< second sub-group interface byte */
	ATR_S_WAIT_TC, /*!< third sub-group interface byte */
	ATR_S_WAIT_TD, /*!< fourth sub-group interface byte */
	ATR_S_WAIT_HIST, /*!< historical byte */
	ATR_S_WAIT_TCK, /*!< check byte */
	ATR_S_DONE
};

/*! Protocol and Parameters Selection (PPS) sub-states of ISO7816_S_IN_PTS_REQ/ISO7816_S_IN_PTS_RSP
 *  @note defined in ISO/IEC 7816-3:2006(E) section 9
 */
enum pps_state {
	PPS_S_TX_PPS_REQ,  /*!< tx pps request */
	PPS_S_WAIT_PPSS, /*!< initial byte */
	PPS_S_WAIT_PPS0, /*!< format byte */
	PPS_S_WAIT_PPS1, /*!< first parameter byte */
	PPS_S_WAIT_PPS2, /*!< second parameter byte */
	PPS_S_WAIT_PPS3, /*!< third parameter byte */
	PPS_S_WAIT_PCK, /*!< check byte */
	PPS_S_WAIT_END, /*!< all done */
	PPS_S_DONE
};

/*! Transport Protocol Data Unit (TPDU) sub-states of ISO7816_S_IN_TPDU
 *  @note defined in ISO/IEC 7816-3:2006(E) section 10 and 12
 *  @remark APDUs are formed by one or more command+response TPDUs
 */
enum tpdu_state {
	TPDU_S_INIT, /*!< initial state */
	TPDU_S_TX_HDR, /*!< transmitting hdr, waiting for completion */
	TPDU_S_PROCEDURE, /*!< procedure byte (could also be SW1) */
	TPDU_S_TX_REMAINING, /*!< Tx remaining data bytes */
	TPDU_S_TX_SINGLE, /*!< Tx single data byte */
	TPDU_S_RX_REMAINING, /*!< Rx remaining data bytes */
	TPDU_S_RX_SINGLE, /*!< Rx single data byte */
	TPDU_S_SW1, /*!< first status word */
	TPDU_S_SW2, /*!< second status word */
	TPDU_S_DONE,
};

/* FSM timer enumeration */
enum iso7816_3_timer {
	T_WAIT_ATR = 1,
	T_GUARD,
};

/* forward declarations */
static struct osmo_fsm iso7816_3_fsm;
static struct osmo_fsm atr_fsm;
static struct osmo_fsm tpdu_fsm;
static struct osmo_fsm pps_fsm;

/* look-up table for bit-wise inversion to convert from "inverse convention" to normal */
static const uint8_t convention_convert_lut[256] = {
	0xff, 0x7f, 0xbf, 0x3f, 0xdf, 0x5f, 0x9f, 0x1f, 0xef, 0x6f, 0xaf, 0x2f, 0xcf, 0x4f, 0x8f, 0x0f,
	0xf7, 0x77, 0xb7, 0x37, 0xd7, 0x57, 0x97, 0x17, 0xe7, 0x67, 0xa7, 0x27, 0xc7, 0x47, 0x87, 0x07,
	0xfb, 0x7b, 0xbb, 0x3b, 0xdb, 0x5b, 0x9b, 0x1b, 0xeb, 0x6b, 0xab, 0x2b, 0xcb, 0x4b, 0x8b, 0x0b,
	0xf3, 0x73, 0xb3, 0x33, 0xd3, 0x53, 0x93, 0x13, 0xe3, 0x63, 0xa3, 0x23, 0xc3, 0x43, 0x83, 0x03,
	0xfd, 0x7d, 0xbd, 0x3d, 0xdd, 0x5d, 0x9d, 0x1d, 0xed, 0x6d, 0xad, 0x2d, 0xcd, 0x4d, 0x8d, 0x0d,
	0xf5, 0x75, 0xb5, 0x35, 0xd5, 0x55, 0x95, 0x15, 0xe5, 0x65, 0xa5, 0x25, 0xc5, 0x45, 0x85, 0x05,
	0xf9, 0x79, 0xb9, 0x39, 0xd9, 0x59, 0x99, 0x19, 0xe9, 0x69, 0xa9, 0x29, 0xc9, 0x49, 0x89, 0x09,
	0xf1, 0x71, 0xb1, 0x31, 0xd1, 0x51, 0x91, 0x11, 0xe1, 0x61, 0xa1, 0x21, 0xc1, 0x41, 0x81, 0x01,
	0xfe, 0x7e, 0xbe, 0x3e, 0xde, 0x5e, 0x9e, 0x1e, 0xee, 0x6e, 0xae, 0x2e, 0xce, 0x4e, 0x8e, 0x0e,
	0xf6, 0x76, 0xb6, 0x36, 0xd6, 0x56, 0x96, 0x16, 0xe6, 0x66, 0xa6, 0x26, 0xc6, 0x46, 0x86, 0x06,
	0xfa, 0x7a, 0xba, 0x3a, 0xda, 0x5a, 0x9a, 0x1a, 0xea, 0x6a, 0xaa, 0x2a, 0xca, 0x4a, 0x8a, 0x0a,
	0xf2, 0x72, 0xb2, 0x32, 0xd2, 0x52, 0x92, 0x12, 0xe2, 0x62, 0xa2, 0x22, 0xc2, 0x42, 0x82, 0x02,
	0xfc, 0x7c, 0xbc, 0x3c, 0xdc, 0x5c, 0x9c, 0x1c, 0xec, 0x6c, 0xac, 0x2c, 0xcc, 0x4c, 0x8c, 0x0c,
	0xf4, 0x74, 0xb4, 0x34, 0xd4, 0x54, 0x94, 0x14, 0xe4, 0x64, 0xa4, 0x24, 0xc4, 0x44, 0x84, 0x04,
	0xf8, 0x78, 0xb8, 0x38, 0xd8, 0x58, 0x98, 0x18, 0xe8, 0x68, 0xa8, 0x28, 0xc8, 0x48, 0x88, 0x08,
	0xf0, 0x70, 0xb0, 0x30, 0xd0, 0x50, 0x90, 0x10, 0xe0, 0x60, 0xa0, 0x20, 0xc0, 0x40, 0x80, 0x00,
};

/***********************************************************************
 * ISO7816-3 Main FSM
 ***********************************************************************/

static const struct value_string iso7816_3_event_names[] = {
	{ ISO7816_E_RX_SINGLE,		"UART_RX_SINGLE" },
	{ ISO7816_E_RX_COMPL,		"UART_RX_COMPL" },
	{ ISO7816_E_TX_COMPL,		"UART_TX_COMPL" },
	{ ISO7816_E_POWER_UP_IND,	"POWER_UP_IND" },
	{ ISO7816_E_RESET_REL_IND,	"RESET_REL_IND" },
	{ ISO7816_E_RX_ERR_IND,		"RX_ERR_IND" },
	{ ISO7816_E_TX_ERR_IND,		"TX_ERR_IND" },
	{ ISO7816_E_ATR_DONE_IND,	"ATR_DONE_IND" },
	{ ISO7816_E_ATR_ERR_IND,	"ATR_ERR_IND" },
	{ ISO7816_E_TPDU_DONE_IND,	"TPDU_DONE_IND" },
	{ ISO7816_E_XCEIVE_TPDU_CMD,	"XCEIVE_TPDU_CMD" },
	/* allstate events */
	{ ISO7816_E_WTIME_EXP,		"WAIT_TIME_EXP" },
	{ ISO7816_E_HW_ERR_IND,		"HW_ERR_IND" },
	{ ISO7816_E_SW_ERR_IND,		"SW_ERR_IND" },
	{ ISO7816_E_CARD_REMOVAL,	"CARD_REMOVAL" },
	{ ISO7816_E_POWER_DN_IND,	"POWER_DN_IND" },
	{ ISO7816_E_RESET_ACT_IND,	"RESET_ACT_IND" },
	{ ISO7816_E_ABORT_REQ,		"ABORT_REQ" },
	{ ISO7816_E_TPDU_CLEAR_REQ,	"TPDU_CLEAR_REQ" },
	{ 0, NULL }
};

struct iso7816_3_priv {
	uint8_t slot_nr;
	/* child FSM instances */
	struct osmo_fsm_inst *atr_fi;
	struct osmo_fsm_inst *pps_fi;
	struct osmo_fsm_inst *tpdu_fi;
	/* other data */
	bool convention_convert;/*!< If convention conversion is needed */
	uint16_t guard_time_ms;
	/* underlying UART */
	struct card_uart *uart;
	iso7816_user_cb user_cb;
	void *user_priv;
};

/* type-safe method to obtain iso7816_3_priv from fi */
static struct iso7816_3_priv *get_iso7816_3_priv(struct osmo_fsm_inst *fi)
{
	OSMO_ASSERT(fi);
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);
	return (struct iso7816_3_priv *) fi->priv;
}

/* convert from clock cycles of the CLK line to milli-seconds */
uint32_t fi_cycles2ms(struct osmo_fsm_inst *fi, uint32_t cyclces)
{
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);
	/* FIXME */
	return 1000;
}

/* card UART notifies us: dispatch to (main ISO7816-3) FSM */
static void tpdu_uart_notification(struct card_uart *cuart, enum card_uart_event evt, void *data)
{
	struct osmo_fsm_inst *fi = (struct osmo_fsm_inst *) cuart->priv;
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	LOGPFSML(fi, LOGL_DEBUG, "UART Notification '%s'\n",
		 get_value_string(card_uart_event_vals, evt));

	/* FIXME: Set only flags here; Main loop polls flags and dispatches events */

	switch (evt) {
	case CUART_E_RX_SINGLE:
		osmo_fsm_inst_dispatch(fi, ISO7816_E_RX_SINGLE, data);
		break;
	case CUART_E_RX_COMPLETE:
		osmo_fsm_inst_dispatch(fi, ISO7816_E_RX_COMPL, data);
		break;
	case CUART_E_RX_TIMEOUT:
		osmo_fsm_inst_dispatch(fi, ISO7816_E_WTIME_EXP, data);
		break;
	case CUART_E_TX_COMPLETE:
		osmo_fsm_inst_dispatch(fi, ISO7816_E_TX_COMPL, data);
		break;
	}
}

static void iso7816_3_reset_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	/* go back to initial state in child FSMs */
	osmo_fsm_inst_state_chg(ip->atr_fi, ATR_S_WAIT_TS, 0, 0);
	osmo_fsm_inst_state_chg(ip->pps_fi, PPS_S_TX_PPS_REQ, 0, 0);
	osmo_fsm_inst_state_chg(ip->tpdu_fi, TPDU_S_INIT, 0, 0);
}

static void iso7816_3_reset_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);
	struct msgb *msg;

	switch (event) {
	case ISO7816_E_RESET_REL_IND:
		/* TOOD: this should happen before reset is released */
		card_uart_ctrl(ip->uart, CUART_CTL_RX, true);
		osmo_fsm_inst_state_chg_ms(fi, ISO7816_S_WAIT_ATR,
					   fi_cycles2ms(fi, 40000), T_WAIT_ATR);
		break;
	case ISO7816_E_PPS_FAILED_IND:
		msg = data;
		/* notify user about PPS result */
		ip->user_cb(fi, event, 0, msg);
		break;
	case ISO7816_E_TPDU_FAILED_IND:
		msg = data;
		/* hand finished TPDU to user */
		ip->user_cb(fi, event, 0, msg);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void iso7816_3_wait_atr_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		osmo_fsm_inst_state_chg(fi, ISO7816_S_IN_ATR, 0, 0);
		osmo_fsm_inst_dispatch(ip->atr_fi, event, data);
		break;
	case ISO7816_E_WTIME_EXP:
		ip->user_cb(fi, event, 0, NULL);
		osmo_fsm_inst_state_chg(fi, ISO7816_S_RESET, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void iso7816_3_in_atr_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	struct msgb *atr;
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	switch (event) {
	case ISO7816_E_RX_SINGLE:
	case ISO7816_E_RX_ERR_IND:
	case ISO7816_E_WTIME_EXP:
		/* simply pass this through to the child FSM for the ATR */
		osmo_fsm_inst_dispatch(ip->atr_fi, event, data);
		break;
	case ISO7816_E_ATR_DONE_IND:
		atr = data;
		/* FIXME: verify ATR result: success / failure */
		osmo_fsm_inst_state_chg(fi, ISO7816_S_WAIT_TPDU, 0, 0);
		/* notify user about ATR */
		ip->user_cb(fi, event, 0, atr);
		break;
	case ISO7816_E_ATR_ERR_IND:
		osmo_fsm_inst_state_chg(fi, ISO7816_S_RESET, 0, 0);
		ip->user_cb(fi, event, 0, atr);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void iso7816_3_wait_tpdu_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);
	card_uart_ctrl(ip->uart, CUART_CTL_RX, false);
	/* reset the TPDU state machine */
	osmo_fsm_inst_dispatch(ip->tpdu_fi, ISO7816_E_TPDU_CLEAR_REQ, NULL);
}

static void iso7816_3_wait_tpdu_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	switch (event) {
	case ISO7816_E_XCEIVE_TPDU_CMD:
		/* "data" contains a msgb-wrapped TPDU */
		osmo_fsm_inst_state_chg(fi, ISO7816_S_IN_TPDU, 0, 0);
		/* pass on to sub-fsm */
		osmo_fsm_inst_dispatch(ip->tpdu_fi, event, data);
		break;
	case ISO7816_E_XCEIVE_PPS_CMD:
//		osmo_fsm_inst_state_chg(fi, ISO7816_S_IN_PPS_REQ, 0, 0);
		osmo_fsm_inst_state_chg(fi, ISO7816_S_WAIT_PPS_RSP, 0, 0);
		osmo_fsm_inst_state_chg(ip->pps_fi, PPS_S_TX_PPS_REQ, 0, 0);
		osmo_fsm_inst_dispatch(ip->pps_fi, event, data);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void iso7816_3_in_tpdu_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	struct msgb *apdu;
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	switch (event) {
	case ISO7816_E_RX_SINGLE:
	case ISO7816_E_RX_COMPL:
	case ISO7816_E_RX_ERR_IND:
	case ISO7816_E_TX_COMPL:
	case ISO7816_E_TX_ERR_IND:
		/* simply pass this through to the child FSM for the ATR */
		osmo_fsm_inst_dispatch(ip->tpdu_fi, event, data);
		break;
	case ISO7816_E_TPDU_DONE_IND:
		apdu = data;
		osmo_fsm_inst_state_chg(fi, ISO7816_S_WAIT_TPDU, 0, 0);
		/* hand finished TPDU to user */
		ip->user_cb(fi, event, 0, apdu);
		break;
	case ISO7816_E_WTIME_EXP:
		/* FIXME: power off? */
		osmo_fsm_inst_state_chg(fi, ISO7816_S_RESET, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void iso7816_3_allstate_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	switch (event) {
	case ISO7816_E_HW_ERR_IND:
	case ISO7816_E_CARD_REMOVAL:
		/* FIXME: power off? */
		osmo_fsm_inst_state_chg(fi, ISO7816_S_RESET, 0, 0);
		break;
	case ISO7816_E_POWER_DN_IND:
	case ISO7816_E_RESET_ACT_IND:
		osmo_fsm_inst_state_chg(fi, ISO7816_S_RESET, 0, 0);
		break;
	case ISO7816_E_ABORT_REQ:
		/* FIXME */
		break;
	default:
		OSMO_ASSERT(0);
		break;
	}
}


static void iso7816_3_s_wait_pps_rsp_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);
	switch (event) {
	case ISO7816_E_TX_COMPL:
		/* Rx of single byte is already enabled by previous card_uart_tx() call */
		osmo_fsm_inst_state_chg(fi, ISO7816_S_IN_PPS_RSP, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void iso7816_3_s_ins_pps_rsp_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	struct msgb *ppsrsp;
	OSMO_ASSERT(fi->fsm == &iso7816_3_fsm);

	switch (event) {
	case ISO7816_E_RX_SINGLE:
	case ISO7816_E_WTIME_EXP:
		/* simply pass this through to the child FSM for the ATR */
		osmo_fsm_inst_dispatch(ip->pps_fi, event, data);
		break;
	case ISO7816_E_PPS_DONE_IND:
	case ISO7816_E_PPS_FAILED_IND:
		ppsrsp = data;
		osmo_fsm_inst_state_chg(fi, ISO7816_S_WAIT_TPDU, 0, 0);
		/* notify user about PPS result */
		ip->user_cb(fi, event, 0, ppsrsp);
		break;
	case ISO7816_E_RX_ERR_IND:
		ppsrsp = data;
		osmo_fsm_inst_state_chg(fi, ISO7816_S_RESET, 0, 0);
		ip->user_cb(fi, event, 0, ppsrsp);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static const struct osmo_fsm_state iso7816_3_states[] = {
	[ISO7816_S_RESET] = {
		.name = "RESET",
		.in_event_mask =	S(ISO7816_E_RESET_REL_IND) |
							S(ISO7816_E_PPS_FAILED_IND)|
							S(ISO7816_E_TPDU_FAILED_IND),
		.out_state_mask =	S(ISO7816_S_WAIT_ATR) |
					S(ISO7816_S_RESET),
		.action = iso7816_3_reset_action,
		.onenter = iso7816_3_reset_onenter,
	},
	[ISO7816_S_WAIT_ATR] = {
		.name = "WAIT_ATR",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ISO7816_S_RESET) |
					S(ISO7816_S_IN_ATR),
		.action = iso7816_3_wait_atr_action,
	},
	[ISO7816_S_IN_ATR] = {
		.name = "IN_ATR",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_RX_ERR_IND) |
					S(ISO7816_E_ATR_DONE_IND) |
					S(ISO7816_E_ATR_ERR_IND) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ISO7816_S_RESET) |
					S(ISO7816_S_IN_ATR) |
					S(ISO7816_S_WAIT_TPDU),
		.action = iso7816_3_in_atr_action,
	},
	[ISO7816_S_WAIT_TPDU] = {
		.name = "WAIT_TPDU",
		.in_event_mask =	S(ISO7816_E_XCEIVE_TPDU_CMD) |
							S(ISO7816_E_XCEIVE_PPS_CMD),
		.out_state_mask =	S(ISO7816_S_RESET) |
					S(ISO7816_S_WAIT_TPDU) |
					S(ISO7816_S_IN_TPDU) |
					S(ISO7816_S_WAIT_PPS_RSP),
		.action = iso7816_3_wait_tpdu_action,
		.onenter = iso7816_3_wait_tpdu_onenter,
	},
	[ISO7816_S_IN_TPDU] = {
		.name = "IN_TPDU",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_RX_COMPL) |
					S(ISO7816_E_TX_COMPL) |
					S(ISO7816_E_RX_ERR_IND) |
					S(ISO7816_E_TX_ERR_IND) |
					S(ISO7816_E_TPDU_DONE_IND) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ISO7816_S_RESET) |
					S(ISO7816_S_WAIT_TPDU) |
					S(ISO7816_S_IN_TPDU),
		.action = iso7816_3_in_tpdu_action,
	},
	[ISO7816_S_WAIT_PPS_RSP] = {
		.name = "WAIT_PPS_RESP",
		.in_event_mask =	S(ISO7816_E_TX_COMPL) |
					S(ISO7816_E_TX_ERR_IND) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ISO7816_S_RESET) |
					S(ISO7816_S_WAIT_TPDU) |
					S(ISO7816_S_WAIT_PPS_RSP) |
					S(ISO7816_S_IN_PPS_RSP),
		.action = iso7816_3_s_wait_pps_rsp_action,
	},
	[ISO7816_S_IN_PPS_RSP] = {
		.name = "IN_PPS_RESP",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_RX_COMPL) |
					S(ISO7816_E_RX_ERR_IND) |
					S(ISO7816_E_PPS_DONE_IND) |
					S(ISO7816_E_PPS_FAILED_IND) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ISO7816_S_RESET) |
					S(ISO7816_S_WAIT_TPDU) |
					S(ISO7816_S_IN_PPS_RSP),
		.action = iso7816_3_s_ins_pps_rsp_action,
	},
};
static struct osmo_fsm iso7816_3_fsm = {
	.name = "ISO7816-3",
	.states = iso7816_3_states,
	.num_states = ARRAY_SIZE(iso7816_3_states),
	.log_subsys = DISO7816,
	.event_names = iso7816_3_event_names,
	.allstate_action = iso7816_3_allstate_action,
	.allstate_event_mask =	S(ISO7816_E_CARD_REMOVAL) |
				S(ISO7816_E_POWER_DN_IND) |
				S(ISO7816_E_RESET_ACT_IND) |
				S(ISO7816_E_HW_ERR_IND) |
				S(ISO7816_E_ABORT_REQ),
};

/***********************************************************************
 * ATR FSM
 ***********************************************************************/

struct atr_fsm_priv {
	uint8_t hist_len;	/*!< store the number of expected historical bytes */
	uint8_t y;		/*!< last mask of the upcoming TA, TB, TC, TD interface bytes */
	uint8_t i;		/*!< interface byte subgroup number */
	struct msgb *atr;	/*!< ATR data */
	uint8_t computed_checksum;
	uint16_t protocol_support;
};

/* obtain the [software] guard time in milli-seconds from the atr fsm_inst */
static uint32_t atr_fi_gt_ms(struct osmo_fsm_inst *fi)
{
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip;

	OSMO_ASSERT(fi->fsm == &atr_fsm);
	OSMO_ASSERT(parent_fi);
	ip = get_iso7816_3_priv(parent_fi);

	return ip->guard_time_ms;
}

/* obtain the 'byte' parmeter of an ISO7816_E_RX event */
static uint8_t get_rx_byte_evt(struct osmo_fsm_inst *fi, void *data)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	uint8_t byte = *(uint8_t *)data;

	/* apply inverse convention */
	if (ip->convention_convert)
		byte = convention_convert_lut[byte];

	return byte;
}

/* append a single byte to the ATR */
static int atr_append_byte(struct osmo_fsm_inst *fi, uint8_t byte)
{
	struct atr_fsm_priv *atp = fi->priv;

	if (!msgb_tailroom(atp->atr)) {
		LOGPFSML(fi, LOGL_ERROR, "ATR overflow !?!");
		osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_SW_ERR_IND, NULL);
		return -1;
	}
	msgb_put_u8(atp->atr, byte);
	return 0;
}

static void atr_wait_ts_onenter(struct osmo_fsm_inst *fi, uint32_t old_state)
{
	struct atr_fsm_priv *atp = fi->priv;

	/* reset state to its initial value */
	atp->hist_len = 0;
	atp->y = 0;
	atp->i = 0;
	if (!atp->atr)
		atp->atr = msgb_alloc_c(fi, 33, "ATR"); /* TS + 32 chars */
	else
		msgb_reset(atp->atr);
	atp->computed_checksum = 0;
	atp->protocol_support = 0;
}

static void atr_wait_ts_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct atr_fsm_priv *atp = fi->priv;
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);
	uint8_t byte;

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		OSMO_ASSERT(msgb_length(atp->atr) == 0);
restart:
		byte = get_rx_byte_evt(parent_fi, data);
		LOGPFSML(fi, LOGL_DEBUG, "RX byte '%02x'\n", byte);
		switch (byte) {
		case 0x23:
			/* direct convention used, but decoded using inverse
			 * convention (a parity error should also have occurred) */
			/* fall-through */
		case 0x30:
			/* inverse convention used, but decoded using direct
			 * convention (a parity error should also have occurred) */
			ip->convention_convert = !ip->convention_convert;
			goto restart;
			break;
		case 0x3b: /* direct convention used and correctly decoded */
			/* fall-through */
		case 0x3f: /* inverse convention used and correctly decoded */
			atr_append_byte(fi, byte);
			osmo_fsm_inst_state_chg_ms(fi, ATR_S_WAIT_T0, atr_fi_gt_ms(fi), T_GUARD);
			break;
		default:
			LOGPFSML(fi, LOGL_ERROR, "Invalid TS received: 0x%02X\n", byte);
			/* FIXME: somehow indiicate to user */
			osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_SW_ERR_IND, NULL);
			break;
		}
		atp->i = 0; /* first interface byte sub-group is coming (T0 is kind of TD0) */
		break;
	case ISO7816_E_WTIME_EXP:
		osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_ATR_ERR_IND, NULL);
		break;
	default: 
		OSMO_ASSERT(0);
	}
}

static void atr_wait_tX_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct atr_fsm_priv *atp = fi->priv;
	uint32_t guard_time_ms = atr_fi_gt_ms(fi);
	uint8_t byte;

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		byte = get_rx_byte_evt(fi->proc.parent, data);
		LOGPFSML(fi, LOGL_DEBUG, "RX byte '%02x'\n", byte);
		atr_append_byte(fi, byte);
		switch (fi->state) {
		case ATR_S_WAIT_T0: /* see ISO/IEC 7816-3:2006 section 8.2.2 */
		case ATR_S_WAIT_TD: /* see ISO/IEC 7816-3:2006 section 8.2.3 */
			if (fi->state == ATR_S_WAIT_T0) {
				/* save number of hist. bytes */
				atp->hist_len = (byte & 0x0f);
			} else {
				/* remember supported protocol to know if TCK will be present */
				atp->protocol_support |= (1<<(byte & 0x0f));
			}
			atp->y = (byte & 0xf0); /* remember incoming interface bytes */
			atp->i++;
			if (atp->y & 0x10) {
				osmo_fsm_inst_state_chg_ms(fi, ATR_S_WAIT_TA, guard_time_ms, T_GUARD);
				break;
			}
			/* fall-through */
		case ATR_S_WAIT_TA: /* see ISO/IEC 7816-3:2006 section 8.2.3 */
			if (atp->y & 0x20) {
				osmo_fsm_inst_state_chg_ms(fi, ATR_S_WAIT_TB, guard_time_ms, T_GUARD);
				break;
			}
			/* fall-through */
		case ATR_S_WAIT_TB: /* see ISO/IEC 7816-3:2006 section 8.2.3 */
			if (atp->y & 0x40) {
				osmo_fsm_inst_state_chg_ms(fi, ATR_S_WAIT_TC, guard_time_ms, T_GUARD);
				break;
			}
			/* fall-through */
		case ATR_S_WAIT_TC: /* see ISO/IEC 7816-3:2006 section 8.2.3 */
			if (atp->y & 0x80) {
				osmo_fsm_inst_state_chg_ms(fi, ATR_S_WAIT_TD, guard_time_ms, T_GUARD);
				break;
			} else if (atp->hist_len) {
				osmo_fsm_inst_state_chg_ms(fi, ATR_S_WAIT_HIST, guard_time_ms, T_GUARD);
				break;
			}
			/* fall-through */
		case ATR_S_WAIT_HIST: /* see ISO/IEC 7816-3:2006 section 8.2.4 */
			if (atp->hist_len)
				atp->hist_len--;
			if (atp->hist_len == 0) {
				if (atp->protocol_support > 1) {
					/* wait for check byte */
					osmo_fsm_inst_state_chg_ms(fi, ATR_S_WAIT_TCK,
								   guard_time_ms, T_GUARD);
					break;
				} else {
					/* no TCK present, ATR complete; notify parent */
					osmo_fsm_inst_state_chg(fi, ATR_S_DONE, 0, 0);
					osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_ATR_DONE_IND, atp->atr);
				}
			} else {
				break;
			}
			/* fall-through */
		case ATR_S_WAIT_TCK: /* see ISO/IEC 7816-3:2006 section 8.2.5 */
			/* verify checksum if present */
			if (fi->state == ATR_S_WAIT_TCK) {
				uint8_t ui;
				uint8_t *atr = msgb_data(atp->atr);
				LOGPFSML(fi, LOGL_INFO, "Complete ATR: %s\n", msgb_hexdump(atp->atr));
				for (ui = 1; ui < msgb_length(atp->atr)-1; ui++) {
					atp->computed_checksum ^= atr[ui];
				}
				if (atp->computed_checksum != byte) {
					/* checkum error. report to user? */
					LOGPFSML(fi, LOGL_ERROR,
						 "computed checksum %02x doesn't match TCK=%02x\n",
						 atp->computed_checksum, byte);
				}
				/* ATR complete; notify parent */
				osmo_fsm_inst_state_chg(fi, ATR_S_DONE, 0, 0);
				osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_ATR_DONE_IND, atp->atr);
			}
			break;
		default:
			OSMO_ASSERT(0);
		}
		break;
	case ISO7816_E_WTIME_EXP:
		switch (fi->state) {
		case ATR_S_WAIT_HIST:
		case ATR_S_WAIT_TCK:
			/* Some cards have an ATR with long indication of historical bytes */
			/* FIXME: should we check the checksum? */
			osmo_fsm_inst_state_chg(fi, ATR_S_DONE, 0, 0);
			osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_ATR_DONE_IND, atp->atr);
			break;
		default:
			osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_ATR_ERR_IND, NULL);
			break;
		}
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static const struct osmo_fsm_state atr_states[] = {
	[ATR_S_WAIT_TS] = {
		.name = "WAIT_TS",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_WAIT_T0),
		.action = atr_wait_ts_action,
		.onenter = atr_wait_ts_onenter,
	},
	[ATR_S_WAIT_T0] = {
		.name = "WAIT_T0",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_WAIT_TA) |
					S(ATR_S_WAIT_TB) |
					S(ATR_S_WAIT_TC) |
					S(ATR_S_WAIT_TD) |
					S(ATR_S_WAIT_HIST) |
					S(ATR_S_WAIT_TCK) |
					S(ATR_S_WAIT_T0),
		.action = atr_wait_tX_action,
	},
	[ATR_S_WAIT_TA] = {
		.name = "WAIT_TA",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_WAIT_TB) |
					S(ATR_S_WAIT_TC) |
					S(ATR_S_WAIT_TD) |
					S(ATR_S_WAIT_HIST) |
					S(ATR_S_WAIT_TCK) |
					S(ATR_S_WAIT_T0),
		.action = atr_wait_tX_action,
	},
	[ATR_S_WAIT_TB] = {
		.name = "WAIT_TB",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_WAIT_TC) |
					S(ATR_S_WAIT_TD) |
					S(ATR_S_WAIT_HIST) |
					S(ATR_S_WAIT_TCK) |
					S(ATR_S_WAIT_T0),
		.action = atr_wait_tX_action,
	},
	[ATR_S_WAIT_TC] = {
		.name = "WAIT_TC",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_WAIT_TD) |
					S(ATR_S_WAIT_HIST) |
					S(ATR_S_WAIT_TCK) |
					S(ATR_S_WAIT_T0),
		.action = atr_wait_tX_action,
	},
	[ATR_S_WAIT_TD] = {
		.name = "WAIT_TD",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_WAIT_TA) |
					S(ATR_S_WAIT_TB) |
					S(ATR_S_WAIT_TC) |
					S(ATR_S_WAIT_TD) |
					S(ATR_S_WAIT_HIST) |
					S(ATR_S_WAIT_TCK) |
					S(ATR_S_WAIT_T0),
		.action = atr_wait_tX_action,
	},
	[ATR_S_WAIT_HIST] = {
		.name = "WAIT_HIST",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_WAIT_TCK) |
					S(ATR_S_WAIT_T0) |
					S(ATR_S_DONE),
		.action = atr_wait_tX_action,
	},
	[ATR_S_WAIT_TCK] = {
		.name = "WAIT_TCK",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
					S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(ATR_S_WAIT_TS) |
					S(ATR_S_DONE),
		.action = atr_wait_tX_action,
	},
	[ATR_S_DONE] = {
		.name = "DONE",
		.in_event_mask =	0,
		.out_state_mask =	S(ATR_S_WAIT_TS),
		//.action = atr_done_action,
	},

};
static struct osmo_fsm atr_fsm = {
	.name = "ATR",
	.states = atr_states,
	.num_states = ARRAY_SIZE(atr_states),
	.log_subsys = DATR,
	.event_names = iso7816_3_event_names,
};

/***********************************************************************
 * PPS FSM
 ***********************************************************************/
struct pps_fsm_priv {
	struct msgb* tx_cmd;
	struct msgb* rx_cmd;
	uint8_t pps0_recv;
};

static void pps_s_wait_ppss_onenter(struct osmo_fsm_inst *fi, uint32_t old_state)
{
	struct pps_fsm_priv *atp = fi->priv;

	if (!atp->rx_cmd)
		atp->rx_cmd = msgb_alloc_c(fi, 6, "PPSRSP"); /* at most 6 */
	else
		msgb_reset(atp->rx_cmd);

	/* notify in case card got pulled out */
	if (atp->tx_cmd){
		osmo_fsm_inst_dispatch(fi->proc.parent,
				ISO7816_E_PPS_FAILED_IND, atp->tx_cmd);
		atp->tx_cmd = 0;
	}
}

static void pps_s_tx_pps_req_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct pps_fsm_priv *atp = fi->priv;
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);

	/* keep the buffer to compare it with the received response */
	atp->tx_cmd = data;

	switch (event) {
	case ISO7816_E_XCEIVE_PPS_CMD:
		osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPSS, 0, 0);
		card_uart_tx(ip->uart, msgb_data(data), msgb_length(data), true);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void pps_wait_pX_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct pps_fsm_priv *atp = fi->priv;
//	uint32_t guard_time_ms = atr_fi_gt_ms(fi);
	uint8_t byte;

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		byte = get_rx_byte_evt(fi->proc.parent, data);
		LOGPFSML(fi, LOGL_DEBUG, "RX byte '%02x'\n", byte);
		msgb_put_u8(atp->rx_cmd, byte);
		switch (fi->state) {
		case PPS_S_WAIT_PPSS:
			if (byte == 0xff)
				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPS0, 0, 0);
			break;
		case PPS_S_WAIT_PPS0:
			atp->pps0_recv = byte;
			if(atp->pps0_recv & (1 << 4)) {
				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPS1, 0, 0);
				break;
			} else if (atp->pps0_recv & (1 << 5)) {
				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPS2, 0, 0);
				break;
			} else if (atp->pps0_recv & (1 << 6)) {
				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPS3, 0, 0);
				break;
			}
			osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PCK, 0, 0);
			break;
		case PPS_S_WAIT_PPS1:
			if (atp->pps0_recv & (1 << 5)) {
				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPS2, 0, 0);
				break;
			} else if (atp->pps0_recv & (1 << 6)) {
				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPS3, 0, 0);
				break;
			}
			osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PCK, 0, 0);
			break;
		case PPS_S_WAIT_PPS2:
			if (atp->pps0_recv & (1 << 6)) {
				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PPS3, 0, 0);
				break;
			}
			osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PCK, 0, 0);
			break;
		case PPS_S_WAIT_PPS3:
			osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_PCK, 0, 0);
			break;
		case PPS_S_WAIT_PCK:
			/* verify checksum if present */
			if (fi->state == PPS_S_WAIT_PCK) {
				uint8_t *pps_received = msgb_data(atp->rx_cmd);
				uint8_t *pps_sent = msgb_data(atp->tx_cmd);

				osmo_fsm_inst_state_chg(fi, PPS_S_WAIT_END, 0, 0);

				/* pps was successful if response equals request
				 * rx buffer stays with the fsm, tx buffer gets handed back and freed
				 * by the cb */
				if (msgb_length(atp->rx_cmd) == msgb_length(atp->tx_cmd) &&
					!memcmp(pps_received, pps_sent, msgb_length(atp->rx_cmd))) {
					osmo_fsm_inst_dispatch(fi->proc.parent,
							ISO7816_E_PPS_DONE_IND, atp->tx_cmd);
				} else {
					osmo_fsm_inst_dispatch(fi->proc.parent,
							ISO7816_E_PPS_FAILED_IND, atp->tx_cmd);
				}
				/* ownership transfer */
				atp->tx_cmd = 0;
			}
			break;
		default:
			OSMO_ASSERT(0);
		}
		break;
	case ISO7816_E_WTIME_EXP:
		/* FIXME: timeout handling if no pps supported ? */
		osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_RX_ERR_IND, NULL);
		break;
	default:
		OSMO_ASSERT(0);
	}
}


static const struct osmo_fsm_state pps_states[] = {
	[PPS_S_TX_PPS_REQ] = {
		.name = "TX_PPS_REQ",
		.in_event_mask =	S(ISO7816_E_XCEIVE_PPS_CMD) |
							S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(PPS_S_TX_PPS_REQ) |
							S(PPS_S_WAIT_PPSS),
		.action = pps_s_tx_pps_req_action,
		.onenter = pps_s_wait_ppss_onenter,
	},
	[PPS_S_WAIT_PPSS] = {
		.name = "WAIT_PPSS",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
							S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(PPS_S_WAIT_PPS0) |
							S(PPS_S_WAIT_PPSS),
		.action = pps_wait_pX_action,
	},
	[PPS_S_WAIT_PPS0] = {
		.name = "WAIT_PPS0",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
							S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(PPS_S_WAIT_PPS1) |
							S(PPS_S_WAIT_PPS2) |
							S(PPS_S_WAIT_PPS3) |
							S(PPS_S_WAIT_PCK),
		.action = pps_wait_pX_action,
	},
	[PPS_S_WAIT_PPS1] = {
		.name = "WAIT_PPS1",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
							S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(PPS_S_WAIT_PPS2) |
							S(PPS_S_WAIT_PPS3) |
							S(PPS_S_WAIT_PCK),
		.action = pps_wait_pX_action,
	},
	[PPS_S_WAIT_PPS2] = {
		.name = "WAIT_PPS2",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
							S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(PPS_S_WAIT_PPS3) |
							S(PPS_S_WAIT_PCK),
		.action = pps_wait_pX_action,
	},
	[PPS_S_WAIT_PPS3] = {
		.name = "WAIT_PPS3",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
							S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(PPS_S_WAIT_PCK),
		.action = pps_wait_pX_action,
	},
	[PPS_S_WAIT_PCK] = {
		.name = "WAIT_PCK",
		.in_event_mask =	S(ISO7816_E_RX_SINGLE) |
							S(ISO7816_E_WTIME_EXP),
		.out_state_mask =	S(PPS_S_WAIT_END),
		.action = pps_wait_pX_action,
	},
	[PPS_S_WAIT_END] = {
		.name = "WAIT_END",
		.in_event_mask =	0,
		.out_state_mask =	S(PPS_S_TX_PPS_REQ) |
							S(PPS_S_WAIT_PPSS),
	},
};

static struct osmo_fsm pps_fsm = {
	.name = "PPS",
	.states = pps_states,
	.num_states = ARRAY_SIZE(pps_states),
	.log_subsys = DPPS,
	.event_names = iso7816_3_event_names,
};

/***********************************************************************
 * TPDU FSM
 ***********************************************************************/

/* In this FSM weu use the msgb for the TPDU as follows:
 *  - 5-byte TPDU header is at msg->data
 *  - COMMAND TPDU:
 *    - command bytes are provided after the header at msg->l2h
 *    - in case of incremental transmission, l3h points to next to-be-transmitted byte
 *  - RESPONSE TPDU:
 *    - any response bytes are stored after the header at msg->l2h
 */

static inline struct osim_apdu_cmd_hdr *msgb_tpdu_hdr(struct msgb *msg) {
	return (struct osim_apdu_cmd_hdr *) msgb_data(msg);
}

struct tpdu_fsm_priv {
	struct msgb *tpdu;
	bool is_command; /* is this a command TPDU (true) or a response (false) */
};

/* type-safe method to obtain iso7816_3_priv from fi */
static struct tpdu_fsm_priv *get_tpdu_fsm_priv(struct osmo_fsm_inst *fi)
{
	OSMO_ASSERT(fi);
	OSMO_ASSERT(fi->fsm == &tpdu_fsm);
	return (struct tpdu_fsm_priv *) fi->priv;
}

static void tpdu_s_init_onenter(struct osmo_fsm_inst *fi, uint32_t old_state)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);

	/* notify in case card got pulled out */
	if (tfp->tpdu){
		osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_TPDU_FAILED_IND, tfp->tpdu);
		tfp->tpdu = 0;
	}
}

static void tpdu_s_init_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);
	struct osim_apdu_cmd_hdr *tpduh;

	switch (event) {
	case ISO7816_E_XCEIVE_TPDU_CMD:
		/* start transmission of a TPDU by sending the 5-byte header */
		tfp->tpdu = (struct msgb *)data;
		OSMO_ASSERT(msgb_length(tfp->tpdu) >= sizeof(*tpduh));
		tfp->tpdu->l2h = msgb_data(tfp->tpdu) + sizeof(*tpduh);
		if (msgb_l2len(tfp->tpdu)) {
			tfp->is_command = true;
			tfp->tpdu->l3h = tfp->tpdu->l2h; /* next tx byte == first byte of body */
		} else
			tfp->is_command = false;
		tpduh = msgb_tpdu_hdr(tfp->tpdu);
		LOGPFSML(fi, LOGL_DEBUG, "Transmitting %s TPDU header %s via UART\n",
			 tfp->is_command ? "COMMAND" : "RESPONSE",
			 osmo_hexdump_nospc((uint8_t *) tpduh, sizeof(*tpduh)));
		osmo_fsm_inst_state_chg(fi, TPDU_S_TX_HDR, 0, 0);
		card_uart_tx(ip->uart, (uint8_t *) tpduh, sizeof(*tpduh), true);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void tpdu_s_tx_hdr_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	OSMO_ASSERT(fi->fsm == &tpdu_fsm);
	switch (event) {
	case ISO7816_E_TX_COMPL:
		/* Rx of single byte is already enabled by previous card_uart_tx() call */
		osmo_fsm_inst_state_chg(fi, TPDU_S_PROCEDURE, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}



static void tpdu_s_procedure_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);
	struct osim_apdu_cmd_hdr *tpduh = msgb_tpdu_hdr(tfp->tpdu);
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);
	uint8_t byte;

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		byte = get_rx_byte_evt(fi->proc.parent, data);
		LOGPFSML(fi, LOGL_DEBUG, "Received 0x%02x from UART\n", byte);
		if (byte == 0x60) {
			/* NULL: wait for another procedure byte */
			osmo_fsm_inst_state_chg(fi, TPDU_S_PROCEDURE, 0, 0);
		} else if ((byte >= 0x60 && byte <= 0x6f) || (byte >= 0x90 && byte <= 0x9f)) {
			//msgb_apdu_sw(tfp->apdu) = byte << 8;
			msgb_put(tfp->tpdu, byte);
			/* receive second SW byte (SW2) */
			osmo_fsm_inst_state_chg(fi, TPDU_S_SW2, 0, 0);
			break;
		} else if (byte == tpduh->ins) {
			if (tfp->is_command) {
				/* transmit all remaining bytes */
				card_uart_tx(ip->uart, msgb_l2(tfp->tpdu), msgb_l2len(tfp->tpdu), true);
				osmo_fsm_inst_state_chg(fi, TPDU_S_TX_REMAINING, 0, 0);
			} else {
				card_uart_set_rx_threshold(ip->uart, tpduh->p3);
				osmo_fsm_inst_state_chg(fi, TPDU_S_RX_REMAINING, 0, 0);
			}
		} else if (byte == (tpduh->ins ^ 0xFF)) {
			/* transmit/recieve single byte then wait for proc */
			if (tfp->is_command) {
				/* transmit *next*, not first byte */
				OSMO_ASSERT(msgb_l3len(tfp->tpdu) >= 0);
				card_uart_tx(ip->uart, msgb_l3(tfp->tpdu), 1, false);
				osmo_fsm_inst_state_chg(fi, TPDU_S_TX_SINGLE, 0, 0);
			} else {
				osmo_fsm_inst_state_chg(fi, TPDU_S_RX_SINGLE, 0, 0);
			}
		} else
			OSMO_ASSERT(0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

/* UART is transmitting remaining data; we wait for ISO7816_E_TX_COMPL */
static void tpdu_s_tx_remaining_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);

	switch (event) {
	case ISO7816_E_TX_COMPL:
		card_uart_set_rx_threshold(ip->uart, 1);
		osmo_fsm_inst_state_chg(fi, TPDU_S_SW1, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

/* UART is transmitting single byte of data; we wait for ISO7816_E_TX_COMPL */
static void tpdu_s_tx_single_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);

	switch (event) {
	case ISO7816_E_TX_COMPL:
		tfp->tpdu->l3h += 1;
		if (msgb_l3len(tfp->tpdu))
			osmo_fsm_inst_state_chg(fi, TPDU_S_PROCEDURE, 0, 0);
		else
			osmo_fsm_inst_state_chg(fi, TPDU_S_SW1, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

/* UART is receiving remaining data; we wait for ISO7816_E_RX_COMPL */
static void tpdu_s_rx_remaining_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);
	struct osim_apdu_cmd_hdr *tpduh = msgb_tpdu_hdr(tfp->tpdu);
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);
	int rc;

	switch (event) {
	case ISO7816_E_RX_COMPL:
		/* retrieve pending byte(s) */
		rc = card_uart_rx(ip->uart, msgb_l2(tfp->tpdu), tpduh->p3);
		OSMO_ASSERT(rc > 0);
		msgb_put(tfp->tpdu, rc);
		if (msgb_l2len(tfp->tpdu) != tpduh->p3) {
			LOGPFSML(fi, LOGL_ERROR, "expected %u bytes; read %d\n", tpduh->p3,
				 msgb_l2len(tfp->tpdu));
		}
		card_uart_set_rx_threshold(ip->uart, 1);
		osmo_fsm_inst_state_chg(fi, TPDU_S_SW1, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

/* UART is receiving single byte of data; we wait for ISO7816_E_RX_SINGLE */
static void tpdu_s_rx_single_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);
	struct osim_apdu_cmd_hdr *tpduh = msgb_tpdu_hdr(tfp->tpdu);
	uint8_t byte;

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		byte = get_rx_byte_evt(fi->proc.parent, data);
		LOGPFSML(fi, LOGL_DEBUG, "Received 0x%02x from UART\n", byte);
		msgb_put_u8(tfp->tpdu, byte);
		/* determine if number of expected bytes received */
		if (msgb_l2len(tfp->tpdu) == tpduh->p3)
			osmo_fsm_inst_state_chg(fi, TPDU_S_SW1, 0, 0);
		else
			osmo_fsm_inst_state_chg(fi, TPDU_S_PROCEDURE, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void tpdu_s_sw1_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);
	uint8_t byte;

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		byte = get_rx_byte_evt(fi->proc.parent, data);
		LOGPFSML(fi, LOGL_DEBUG, "Received 0x%02x from UART\n", byte);
		/* record byte */
		//msgb_apdu_sw(tfp->apdu) = byte << 8;
		msgb_put_u8(tfp->tpdu, byte);
		card_uart_set_rx_threshold(ip->uart, 1);
		osmo_fsm_inst_state_chg(fi, TPDU_S_SW2, 0, 0);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void tpdu_s_sw2_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct tpdu_fsm_priv *tfp = get_tpdu_fsm_priv(fi);
	struct osmo_fsm_inst *parent_fi = fi->proc.parent;
	struct iso7816_3_priv *ip = get_iso7816_3_priv(parent_fi);
	uint8_t byte;

	switch (event) {
	case ISO7816_E_RX_SINGLE:
		byte = get_rx_byte_evt(fi->proc.parent, data);
		LOGPFSML(fi, LOGL_DEBUG, "Received 0x%02x from UART\n", byte);
		/* record SW2 byte */
		//msgb_apdu_sw(tfp->apdu) &= 0xFF00;
		//msgb_apdu_sw(tfp->apdu) |= byte;
		msgb_put_u8(tfp->tpdu, byte);
		osmo_fsm_inst_state_chg(fi, TPDU_S_DONE, 0, 0);
		/* Notify parent FSM */
		osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_TPDU_DONE_IND, tfp->tpdu);

		/* ownership transfer */
		tfp->tpdu = 0;
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void tpdu_allstate_action(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	OSMO_ASSERT(fi->fsm == &tpdu_fsm);

	switch (event) {
	case ISO7816_E_RX_ERR_IND:
	case ISO7816_E_TX_ERR_IND:
		/* FIXME: handle this in some different way */
		osmo_fsm_inst_state_chg(fi, TPDU_S_DONE, 0, 0);
		osmo_fsm_inst_dispatch(fi->proc.parent, ISO7816_E_TPDU_DONE_IND, NULL);
		break;
	case ISO7816_E_TPDU_CLEAR_REQ:
		osmo_fsm_inst_state_chg(fi, TPDU_S_INIT, 0, 0);
		break;
	}
}

static const struct osmo_fsm_state tpdu_states[] = {
	[TPDU_S_INIT] = {
		.name = "INIT",
		.in_event_mask = S(ISO7816_E_XCEIVE_TPDU_CMD) |
				 S(ISO7816_E_TX_COMPL),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_TX_HDR),
		.action = tpdu_s_init_action,
		.onenter = tpdu_s_init_onenter,
	},
	[TPDU_S_TX_HDR] = {
		.name = "TX_HDR",
		.in_event_mask = S(ISO7816_E_TX_COMPL),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_PROCEDURE),
		.action = tpdu_s_tx_hdr_action,
	},
	[TPDU_S_PROCEDURE] = {
		.name = "PROCEDURE",
		.in_event_mask = S(ISO7816_E_RX_SINGLE),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_PROCEDURE) |
				  S(TPDU_S_RX_REMAINING) |
				  S(TPDU_S_RX_SINGLE) |
				  S(TPDU_S_TX_REMAINING) |
				  S(TPDU_S_TX_SINGLE) |
				  S(TPDU_S_SW2),
		.action = tpdu_s_procedure_action,
	},
	[TPDU_S_TX_REMAINING] = {
		.name = "TX_REMAINING",
		.in_event_mask = S(ISO7816_E_TX_COMPL),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_SW1),
		.action = tpdu_s_tx_remaining_action,
	},
	[TPDU_S_TX_SINGLE] = {
		.name = "TX_SINGLE",
		.in_event_mask = S(ISO7816_E_TX_COMPL),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_PROCEDURE),
		.action = tpdu_s_tx_single_action,
	},
	[TPDU_S_RX_REMAINING] = {
		.name = "RX_REMAINING",
		.in_event_mask = S(ISO7816_E_RX_COMPL),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_SW1),
		.action = tpdu_s_rx_remaining_action,
	},
	[TPDU_S_RX_SINGLE] = {
		.name = "RX_SINGLE",
		.in_event_mask = S(ISO7816_E_RX_SINGLE),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_PROCEDURE),
		.action = tpdu_s_rx_single_action,
	},
	[TPDU_S_SW1] = {
		.name = "SW1",
		.in_event_mask = S(ISO7816_E_RX_SINGLE),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_SW2),
		.action = tpdu_s_sw1_action,
	},
	[TPDU_S_SW2] = {
		.name = "SW2",
		.in_event_mask = S(ISO7816_E_RX_SINGLE),
		.out_state_mask = S(TPDU_S_INIT) |
				  S(TPDU_S_DONE),
		.action = tpdu_s_sw2_action,
	},
	[TPDU_S_DONE] = {
		.name = "DONE",
		.in_event_mask = 0,
		.out_state_mask = S(TPDU_S_INIT),
		.action = NULL,
	},
};
static struct osmo_fsm tpdu_fsm = {
	.name = "TPDU",
	.states = tpdu_states,
	.num_states = ARRAY_SIZE(tpdu_states),
	.allstate_event_mask =	S(ISO7816_E_RX_ERR_IND) |
			 	S(ISO7816_E_TX_ERR_IND) |
				S(ISO7816_E_TPDU_CLEAR_REQ),
	.allstate_action = tpdu_allstate_action,
	.log_subsys = DTPDU,
	.event_names = iso7816_3_event_names,
};

struct osmo_fsm_inst *iso7816_fsm_alloc(void *ctx, int log_level, const char *id,
					struct card_uart *cuart, iso7816_user_cb user_cb,
					void *user_priv)
{
	struct iso7816_3_priv *ip;
	struct osmo_fsm_inst *fi;

	fi = osmo_fsm_inst_alloc(&iso7816_3_fsm, ctx, NULL, log_level, id);
	ip = talloc_zero(fi, struct iso7816_3_priv);
	if (!ip)
		goto out_fi;
	fi->priv = ip;

	ip->uart = cuart;
	cuart->priv = fi;
	cuart->handle_event = tpdu_uart_notification;

	ip->user_cb = user_cb;
	ip->user_priv = user_priv;

	ip->atr_fi = osmo_fsm_inst_alloc_child(&atr_fsm, fi, ISO7816_E_SW_ERR_IND);
	if (!ip->atr_fi)
		goto out_fi;
	ip->atr_fi->priv = talloc_zero(ip->atr_fi, struct atr_fsm_priv);
	if (!ip->atr_fi->priv)
		goto out_atr;

	ip->tpdu_fi = osmo_fsm_inst_alloc_child(&tpdu_fsm, fi, ISO7816_E_SW_ERR_IND);
	if (!ip->tpdu_fi)
		goto out_atr;
	ip->tpdu_fi->priv = talloc_zero(ip->tpdu_fi, struct tpdu_fsm_priv);
	if (!ip->tpdu_fi->priv)
		goto out_tpdu;

#if 1
	ip->pps_fi = osmo_fsm_inst_alloc_child(&pps_fsm, fi, ISO7816_E_SW_ERR_IND);
	if (!ip->pps_fi)
		goto out_tpdu;
	ip->pps_fi->priv = talloc_zero(ip->pps_fi, struct pps_fsm_priv);
	if (!ip->pps_fi->priv)
		goto out_pps;
#endif

	/* This ensures the 'onenter' function of the initial state is called */
	osmo_fsm_inst_state_chg(fi, ISO7816_S_RESET, 0, 0);

	return fi;

#if 1
out_pps:
	osmo_fsm_inst_free(ip->pps_fi);
#endif
out_tpdu:
	osmo_fsm_inst_free(ip->tpdu_fi);
out_atr:
	osmo_fsm_inst_free(ip->atr_fi);
out_fi:
	osmo_fsm_inst_free(fi);
	cuart->priv = NULL;
	return NULL;
}

void *iso7816_fsm_get_user_priv(struct osmo_fsm_inst *fi)
{
	struct iso7816_3_priv *ip = get_iso7816_3_priv(fi);
	return ip->user_priv;
}


static __attribute__((constructor)) void on_dso_load_iso7816(void)
{
	osmo_fsm_register(&iso7816_3_fsm);
	osmo_fsm_register(&atr_fsm);
	osmo_fsm_register(&tpdu_fsm);
	osmo_fsm_register(&pps_fsm);
}
