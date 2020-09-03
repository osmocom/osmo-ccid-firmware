#pragma once
/* ISO7816-3 Finite State Machine
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

#include <osmocom/core/fsm.h>
struct card_uart;

enum iso7816_3_event {
	ISO7816_E_RX_SINGLE,		/*!< single-byte data received on UART */
	ISO7816_E_RX_COMPL,		/*!< data receive complete on UART */
	ISO7816_E_TX_COMPL,		/*!< data transmit complete on UART */
	ISO7816_E_POWER_UP_IND,		/*!< Card powered up */
	ISO7816_E_RESET_REL_IND,	/*!< Reset released */
	ISO7816_E_RX_ERR_IND,		/*!< Uncorrectable Rx [parity] error */
	ISO7816_E_TX_ERR_IND,		/*!< Uncorrectable Rx [parity] error */
	ISO7816_E_XCEIVE_TPDU_CMD,	/*!< Ask for start of TPDU transmission */
	/* allstate events */
	ISO7816_E_WTIME_EXP,		/*!< WTIME expired */
	ISO7816_E_HW_ERR_IND,		/*!< Hardware error (overcurrent, ...) */
	ISO7816_E_SW_ERR_IND,		/*!< Software error */
	ISO7816_E_CARD_REMOVAL,		/*!< card has been removed from slot */
	ISO7816_E_POWER_DN_IND,		/*!< Card powered down */
	ISO7816_E_RESET_ACT_IND,	/*!< Reset activated */
	ISO7816_E_ABORT_REQ,		/*!< Abort request (e.g. from CCID) */
	/* TODO: PPS request */
	ISO7816_E_XCEIVE_PPS_CMD,
	ISO7816_E_PPS_DONE_IND,
	ISO7816_E_PPS_FAILED_IND,
	/* TODO: Clock stop request */
	/* TODO: Rx FIFO overrun */
	/* TODO: Rx buffer overrun */

	/* internal events between FSMs in this file */
	ISO7816_E_ATR_DONE_IND,		/*!< ATR Done indication from ATR child FSM */
	ISO7816_E_ATR_ERR_IND,		/*!< ATR Error indication from ATR child FSM */
	ISO7816_E_TPDU_DONE_IND,	/*!< TPDU Done indication from TPDU child FSM */
	ISO7816_E_TPDU_FAILED_IND,	/*!< TPDU Failed indication from TPDU child FSM */
	ISO7816_E_TPDU_CLEAR_REQ,	/*!< Return TPDU FSM to TPDU_S_INIT */
};

typedef void (*iso7816_user_cb)(struct osmo_fsm_inst *fi, int event, int cause, void *data);

struct osmo_fsm_inst *iso7816_fsm_alloc(void *ctx, int log_level, const char *id,
					struct card_uart *cuart, iso7816_user_cb user_cb,
					void *ussr_priv);

void *iso7816_fsm_get_user_priv(struct osmo_fsm_inst *fi);
