/* Code providing a ccid_slot_ops implementation based on iso7716_fsm,
 * (which in turn sits on top of card_uart) */

/* (C) 2019-2020 by Harald Welte <laforge@gnumonks.org>
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
#include <errno.h>
#include <string.h>

#ifdef OCTSIMFWBUILD
#include "hal/include/hal_delay.h"
#endif
#include <osmocom/core/msgb.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/fsm.h>

#include "ccid_device.h"
#include "cuart.h"
#include "iso7816_fsm.h"
#include "iso7816_3.h"

#define FAKE_CCID_SETPARAMETERS

struct iso_fsm_slot {
	/* CCID slot above us */
	struct ccid_slot *cs;
	/* main ISO7816-3 FSM instance beneath us */
	struct osmo_fsm_inst *fi;
	/* UART beneath the ISO7816-3 FSM */
	struct card_uart *cuart;
	/* bSeq of the operation currently in progress */
	uint8_t seq;
};

struct iso_fsm_slot_instance {
	struct iso_fsm_slot slot[NR_SLOTS];
};

static struct iso_fsm_slot_instance g_si;

static struct iso_fsm_slot *ccid_slot2iso_fsm_slot(struct ccid_slot *cs)
{
	OSMO_ASSERT(cs->slot_nr < ARRAY_SIZE(g_si.slot));
	return &g_si.slot[cs->slot_nr];
}

struct card_uart *cuart4slot_nr(uint8_t slot_nr)
{
	OSMO_ASSERT(slot_nr < ARRAY_SIZE(g_si.slot));
	return g_si.slot[slot_nr].cuart;
}

static const uint8_t sysmousim_sjs1_atr[] = {
		0x3B, 0x9F, 0x96, 0x80, 0x1F, 0xC7, 0x80, 0x31,
		0xA0, 0x73, 0xBE, 0x21, 0x13, 0x67, 0x43, 0x20,
		0x07, 0x18, 0x00, 0x00, 0x01, 0xA5 };

static const struct ccid_pars_decoded iso_fsm_def_pars = {
	.fi = 372,
	.di = 1,
	.clock_stop = CCID_CLOCK_STOP_NOTALLOWED,
	.inverse_convention = false,
	.t0 = {
		.guard_time_etu = 0,
		.waiting_integer = 0,
	},
	/* FIXME: T=1 */
};

static void iso_fsm_slot_pre_proc_cb(struct ccid_slot *cs, struct msgb *msg)
{
	/* do nothing; real hardware would update the slot related state here */
}

static void iso_fsm_slot_icc_set_insertion_status(struct ccid_slot *cs, bool present) {
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);

	if(present == cs->icc_present)
		return;

	cs->icc_present = present;

	if (!present) {
		osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_CARD_REMOVAL, NULL);
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER_5V0, false);
		cs->icc_powered = false;
		cs->cmd_busy = false;
	}
}

static void iso_fsm_slot_icc_power_on_async(struct ccid_slot *cs, struct msgb *msg,
					const struct ccid_pc_to_rdr_icc_power_on *ipo)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);
	enum ccid_power_select pwrsel = ipo->bPowerSelect;
	enum card_uart_ctl cctl;

	ss->seq = ipo->hdr.bSeq;
	LOGPCS(cs, LOGL_DEBUG, "scheduling power-up\n");

	switch (pwrsel) {
		case CCID_PWRSEL_5V0: cctl = CUART_CTL_POWER_5V0; break;
		case CCID_PWRSEL_3V0: cctl = CUART_CTL_POWER_3V0; break;
		case CCID_PWRSEL_1V8: cctl = CUART_CTL_POWER_1V8; break;
		default: cctl = CUART_CTL_POWER_5V0;
	}

	if (! cs->icc_powered) {
		/* FIXME: do this via a FSM? */
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
		osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_RESET_ACT_IND, NULL);
		card_uart_ctrl(ss->cuart, cctl, true);
		osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_POWER_UP_IND, NULL);
		cs->icc_powered = true;
		card_uart_ctrl(ss->cuart, CUART_CTL_CLOCK, true);
	#ifdef OCTSIMFWBUILD
		delay_us(10000);
	#else
		usleep(10000);
	#endif

		osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_RESET_REL_IND, NULL);
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, false);
	} else { /* warm reset */
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
		osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_RESET_ACT_IND, NULL);
	#ifdef OCTSIMFWBUILD
		delay_us(10000);
	#else
		usleep(10000);
	#endif
		osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_RESET_REL_IND, NULL);
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, false);
	}
	msgb_free(msg);
	/* continues in iso_fsm_clot_user_cb once ATR is received */
}

#ifndef __NOP
#define __NOP()
#endif

static void iso_fsm_clot_user_cb(struct osmo_fsm_inst *fi, int event, int cause, void *data)
{
	struct iso_fsm_slot *ss = iso7816_fsm_get_user_priv(fi);
	struct ccid_slot *cs = ss->cs;

	switch (event) {
	/* special case: not handled as a normal callback below, in case slot was busy the fsm will
	 * additionally emit a proper error event handled below to notify the host */
	case ISO7816_E_HW_ERR_IND:
		card_uart_ctrl(ss->cuart, CUART_CTL_NO_RXTX, true);
		break;
	case ISO7816_E_ATR_ERR_IND:
	case ISO7816_E_TPDU_FAILED_IND:
	case ISO7816_E_PPS_FAILED_IND:
	case ISO7816_E_PPS_UNSUPPORTED_IND:
	case ISO7816_E_WTIME_EXP:
		__NOP();
		/* no break */
	case ISO7816_E_ATR_DONE_IND:
	case ISO7816_E_TPDU_DONE_IND:
	case ISO7816_E_PPS_DONE_IND:
		cs->event_data = data;
#ifdef OCTSIMFWBUILD
		asm volatile("dmb st": : :"memory");
#endif
		cs->event = event;
		break;
	default:
		LOGPCS(cs, LOGL_NOTICE, "%s(event=%d, cause=%d, data=%p) unhandled\n",
			__func__, event, cause, data);
		break;
	}
}

/* do not free msgbs passed from the fsms, they are statically allocated! */
static int iso_handle_fsm_events(struct ccid_slot *cs, bool enable){
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);
	struct msgb *tpdu, *resp;
	volatile uint32_t event = cs->event;
	volatile void * volatile data = cs->event_data;

	if(!event)
		return 0;
//	if(event && !data)
//		return 0;

	switch (event) {
	case ISO7816_E_WTIME_EXP:
		tpdu = data;
		LOGPCS(cs, LOGL_DEBUG, "%s(event=%d, data=0)\n", __func__, event);


		/* perform deactivation */
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER_5V0, false);
		cs->icc_powered = false;


		resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_FAILED, CCID_ERR_ICC_MUTE, 0, 0);
		ccid_slot_send_unbusy(cs, resp);
		cs->event = 0;
		break;
	case ISO7816_E_ATR_DONE_IND:
		tpdu = data;

		/* inverse condition, error interrupt is always disabled during atr and reenabled here after atr */
		if(*msgb_data(tpdu) == 0x3f) {
			card_uart_ctrl(ss->cuart, CUART_CTL_ERROR_AND_INV, true);
		} else {
			card_uart_ctrl(ss->cuart, CUART_CTL_ERROR_AND_INV, false);
		}

		LOGPCS(cs, LOGL_DEBUG, "%s(event=%d, data=%s)\n", __func__, event, msgb_hexdump(tpdu));
		resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_OK, 0, msgb_data(tpdu), msgb_length(tpdu));
		ccid_slot_send_unbusy(cs, resp);
		cs->event = 0;
		break;
	case ISO7816_E_ATR_ERR_IND:
		tpdu = data;
		LOGPCS(cs, LOGL_DEBUG, "%s(event=%d, data=%s)\n", __func__, event, msgb_hexdump(tpdu));

		/* perform deactivation */
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER_5V0, false);
		cs->icc_powered = false;

		resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_FAILED, CCID_ERR_ICC_MUTE, msgb_data(tpdu), msgb_length(tpdu));
		ccid_slot_send_unbusy(cs, resp);
		cs->event = 0;
		break;
		break;
	case ISO7816_E_TPDU_DONE_IND:
		tpdu = data;
		LOGPCS(cs, LOGL_DEBUG, "%s(event=%d, data=%s)\n", __func__, event,
			msgb_hexdump(tpdu));
		resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_OK, 0, msgb_l4(tpdu), msgb_l4len(tpdu));
		ccid_slot_send_unbusy(cs, resp);
		cs->event = 0;
		break;
	case ISO7816_E_TPDU_FAILED_IND:
		tpdu = data;

		/* perform deactivation */
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER_5V0, false);
		cs->icc_powered = false;

		LOGPCS(cs, LOGL_DEBUG, "%s(event=%d, data=%s)\n", __func__, event, msgb_hexdump(tpdu));
		/* FIXME: other error causes than card removal?*/
		resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_FAILED, CCID_ERR_ICC_MUTE, msgb_l2(tpdu), 0);
		ccid_slot_send_unbusy(cs, resp);
		cs->event = 0;
		break;
	case ISO7816_E_PPS_DONE_IND:
		tpdu = data;
		/* pps was successful, so we know these values are fine */
		uint16_t F = iso7816_3_fi_table[cs->proposed_pars.fi];
		uint8_t D = iso7816_3_di_table[cs->proposed_pars.di];
		uint32_t fmax = iso7816_3_fmax_table[cs->proposed_pars.fi];
		uint8_t D_or_one = D > 0 ? D : 1;

		/* 7816-3 5.2.3
		 * No  information  shall  be  exchanged  when  switching  the
		 * frequency  value.  Two  different  times  are  recommended
		 * for switching the frequency value, either
		 * - after ATR while card is idle
		 * - after PPS while card is idle
		 */
#ifndef FAKE_CCID_SETPARAMETERS
		card_uart_ctrl(ss->cuart, CUART_CTL_SET_CLOCK_FREQ, fmax);
		card_uart_ctrl(ss->cuart, CUART_CTL_SET_FD, F/D);
		card_uart_ctrl(ss->cuart, CUART_CTL_WTIME, cs->proposed_pars.t0.waiting_integer * 960 * D_or_one);

		cs->pars = cs->proposed_pars;
#else
		//card_uart_ctrl(ss->cuart, CUART_CTL_SET_CLOCK_FREQ, fmax);
		card_uart_ctrl(ss->cuart, CUART_CTL_SET_FD, F/D);
		//card_uart_ctrl(ss->cuart, CUART_CTL_WTIME, cs->proposed_pars.t0.waiting_integer);

		cs->pars.fi = cs->proposed_pars.fi;
		cs->pars.di = cs->proposed_pars.di;
#endif
		resp = ccid_gen_parameters_t0(cs, ss->seq, CCID_CMD_STATUS_OK, 0);

		ccid_slot_send_unbusy(cs, resp);

		cs->event = 0;
		break;
	case ISO7816_E_PPS_UNSUPPORTED_IND:
	/* unsupported means no response, failed means request/response mismatch
	 * yet both lead to a deactivation, and the host always gets a fi/di error
	 * 10 "FI - DI pair invalid or not supported" since that part of the
	 * ccid setparameters is handled by the pps exchange
	 */
		/* fall-through */
	case ISO7816_E_PPS_FAILED_IND:
		tpdu = data;

		/* perform deactivation */
		card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER_5V0, false);
		cs->icc_powered = false;

		/* failed fi/di */
		resp = ccid_gen_parameters_t0(cs, ss->seq, CCID_CMD_STATUS_FAILED, 10);
		ccid_slot_send_unbusy(cs, resp);

		cs->event = 0;
		break;
	case 0:
		break;
	default:
		LOGPCS(cs, LOGL_NOTICE, "%s(event=%d, data=%p) unhandled\n",
			__func__, event, data);
		break;
	}
}

static int iso_fsm_slot_xfr_block_async(struct ccid_slot *cs, struct msgb *msg,
				const struct ccid_pc_to_rdr_xfr_block *xfb)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);


	ss->seq = xfb->hdr.bSeq;

	/* must be '0' for TPDU level exchanges or for short APDU */
	if (xfb->wLevelParameter != 0x0000)
		return -8;

	/* ccid header is 10b */
	if (msgb_length(msg) != xfb->hdr.dwLength + 10)
		return -1;

	/* might be unpowered after failed ppss that led to reset */
	if (cs->icc_powered != true)
		return -0;

	msgb_pull(msg, 10);

	LOGPCS(cs, LOGL_DEBUG, "scheduling TPDU transfer: %s\n", msgb_hexdump(msg));
	osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_XCEIVE_TPDU_CMD, msg);
	msgb_free(msg);
	/* continues in iso_fsm_clot_user_cb once response/error/timeout is received */
	return 1;
}


static void iso_fsm_slot_set_power(struct ccid_slot *cs, bool enable)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);

	if (enable) {
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER_5V0, true);
		cs->icc_powered = true;
	} else {
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER_5V0, false);
		cs->icc_powered = false;
	}
}

static void iso_fsm_slot_set_clock(struct ccid_slot *cs, enum ccid_clock_command cmd)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);

	switch (cmd) {
	case CCID_CLOCK_CMD_STOP:
		card_uart_ctrl(ss->cuart, CUART_CTL_CLOCK, false);
		break;
	case CCID_CLOCK_CMD_RESTART:
		card_uart_ctrl(ss->cuart, CUART_CTL_CLOCK, true);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static int iso_fsm_slot_set_params(struct ccid_slot *cs, uint8_t seq, enum ccid_protocol_num proto,
				const struct ccid_pars_decoded *pars_dec)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);
	uint8_t PPS1 = (pars_dec->fi << 4 | pars_dec->di);

	/* see 6.1.7 for error offsets */
	if(proto != CCID_PROTOCOL_NUM_T0)
		return -7;

	if(pars_dec->t0.guard_time_etu != 0)
		return -12;

	if(pars_dec->clock_stop != CCID_CLOCK_STOP_NOTALLOWED)
		return -14;

	ss->seq = seq;

	/* FIXME:
	When  using  D=64,  the  interface  device  shall  ensure  a  delay
	  of  at  least  16  etu  between  the  leading  edge  of  the last
	   received character and the leading edge of the character transmitted
	    for initiating a command.
	    -> we can't really do 4 stop bits?!
	*/

	LOGPCS(cs, LOGL_DEBUG, "scheduling PPS transfer, PPS1: %2x\n", PPS1);

#ifdef FAKE_CCID_SETPARAMETERS
	ccid_slot_send_unbusy(cs, ccid_gen_parameters_t0(cs, ss->seq, CCID_CMD_STATUS_OK, 0));
#else
	/* pass PPS1 instead of msgb */
	osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_XCEIVE_PPS_CMD, (void*)PPS1);
#endif

	/* continues in iso_fsm_clot_user_cb once response/error/timeout is received */
	return 0;
}

static int iso_fsm_slot_set_rate_and_clock(struct ccid_slot *cs, uint32_t* freq_hz, uint32_t* rate_bps)
{
	/* we return the currently used values, since we support automatic features */
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);

	*rate_bps = card_uart_ctrl(ss->cuart, CUART_CTL_GET_BAUDRATE, false);
	*freq_hz = card_uart_ctrl(ss->cuart, CUART_CTL_GET_CLOCK_FREQ, false)/1000;

	return 0;
}

extern void *g_tall_ctx;
static int iso_fsm_slot_init(struct ccid_slot *cs)
{
	void *ctx = g_tall_ctx; /* FIXME */
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);
	struct card_uart *cuart = talloc_zero(ctx, struct card_uart);
	char id_buf[3+3+1];
	char devname[2+1];
	char *devnamep = 0;
	char *drivername = "asf4";
	int rc;

	LOGPCS(cs, LOGL_DEBUG, "%s\n", __func__);

	snprintf(id_buf, sizeof(id_buf), "SIM%d", cs->slot_nr);
#ifdef OCTSIMFWBUILD
	snprintf(devname, sizeof(devname), "%d", cs->slot_nr);
	devnamep = devname;
#else
	if (cs->slot_nr == 0) {
		cs->icc_present = true;
		devnamep = "/dev/ttyUSB5";
	}
	drivername = "tty";
#endif

	if (!cuart)
		return -ENOMEM;

	if (devnamep) {
		rc = card_uart_open(cuart, drivername, devnamep);
		if (rc < 0) {
			LOGPCS(cs, LOGL_ERROR, "Cannot open UART %s: %d\n", devname, rc);
			talloc_free(cuart);
			return rc;
		}
	}

	ss->fi = iso7816_fsm_alloc(ctx, LOGL_DEBUG, id_buf, cuart, iso_fsm_clot_user_cb, ss);
	if (!ss->fi) {
		LOGPCS(cs, LOGL_ERROR, "Cannot allocate ISO FSM\n");
		talloc_free(cuart);
		return -1;
	}

	cs->default_pars = &iso_fsm_def_pars;
	ss->cuart = cuart;
	ss->cs = cs;


	return 0;
}

const struct ccid_slot_ops iso_fsm_slot_ops = {
	.init = iso_fsm_slot_init,
	.pre_proc_cb = iso_fsm_slot_pre_proc_cb,
	.icc_power_on_async = iso_fsm_slot_icc_power_on_async,
	.icc_set_insertion_status = iso_fsm_slot_icc_set_insertion_status,
	.xfr_block_async = iso_fsm_slot_xfr_block_async,
	.set_power = iso_fsm_slot_set_power,
	.set_clock = iso_fsm_slot_set_clock,
	.set_params = iso_fsm_slot_set_params,
	.set_rate_and_clock = iso_fsm_slot_set_rate_and_clock,
	.handle_fsm_events = iso_handle_fsm_events,
};
