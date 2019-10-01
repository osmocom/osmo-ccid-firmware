/* Code providing a ccid_slot_ops implementation based on iso7716_fsm,
 * (which in turn sits on top of card_uart) */

#include <unistd.h>
#include <errno.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/fsm.h>

#include "ccid_device.h"
#include "cuart.h"
#include "iso7816_fsm.h"

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

static void iso_fsm_slot_icc_power_on_async(struct ccid_slot *cs, struct msgb *msg,
					const struct ccid_pc_to_rdr_icc_power_on *ipo)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);

	ss->seq = ipo->hdr.bSeq;
	LOGPCS(cs, LOGL_DEBUG, "scheduling power-up\n");

	/* FIXME: do this via a FSM? */
	card_uart_ctrl(ss->cuart, CUART_CTL_RST, true);
	card_uart_ctrl(ss->cuart, CUART_CTL_POWER, true);
	osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_POWER_UP_IND, NULL);
	cs->icc_powered = true;
	card_uart_ctrl(ss->cuart, CUART_CTL_CLOCK, true);
	usleep(10000);
	card_uart_ctrl(ss->cuart, CUART_CTL_RST, false);
	osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_RESET_REL_IND, NULL);

	msgb_free(msg);
	/* continues in iso_fsm_clot_user_cb once ATR is received */
}
static void iso_fsm_clot_user_cb(struct osmo_fsm_inst *fi, int event, int cause, void *data)
{
	struct iso_fsm_slot *ss = iso7816_fsm_get_user_priv(fi);
	struct ccid_slot *cs = ss->cs;
	struct msgb *tpdu, *resp;

	LOGPCS(cs, LOGL_DEBUG, "%s(event=%d, cause=%d, data=%p)\n", __func__, event, cause, data);

	switch (event) {
	case ISO7816_E_ATR_DONE_IND:
		tpdu = data;
		/* FIXME: copy response data over */
		resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_OK, 0,
					   msgb_data(tpdu), msgb_length(tpdu));
		ccid_slot_send_unbusy(cs, resp);
		msgb_free(tpdu);
		break;
	case ISO7816_E_TPDU_DONE_IND:
		tpdu = data;
		/* FIXME: copy response data over */
		resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_OK, 0, msgb_l2(tpdu), msgb_l2len(tpdu));
		ccid_slot_send_unbusy(cs, resp);
		msgb_free(tpdu);
		break;
	}
}

static void iso_fsm_slot_xfr_block_async(struct ccid_slot *cs, struct msgb *msg,
				const struct ccid_pc_to_rdr_xfr_block *xfb)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);

	LOGPCS(cs, LOGL_DEBUG, "scheduling TPDU transfer\n");
	ss->seq = xfb->hdr.bSeq;
	osmo_fsm_inst_dispatch(ss->fi, ISO7816_E_XCEIVE_TPDU_CMD, msg);
	/* continues in iso_fsm_clot_user_cb once response/error/timeout is received */
}


static void iso_fsm_slot_set_power(struct ccid_slot *cs, bool enable)
{
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);

	if (enable) {
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER, true);
	} else {
		card_uart_ctrl(ss->cuart, CUART_CTL_POWER, false);
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

static int iso_fsm_slot_set_params(struct ccid_slot *cs, enum ccid_protocol_num proto,
				const struct ccid_pars_decoded *pars_dec)
{
	/* we always acknowledge all parameters */
	return 0;
}

static int iso_fsm_slot_set_rate_and_clock(struct ccid_slot *cs, uint32_t freq_hz, uint32_t rate_bps)
{
	/* we always acknowledge all rates/clocks */
	return 0;
}


static int iso_fsm_slot_init(struct ccid_slot *cs)
{
	void *ctx = NULL; /* FIXME */
	struct iso_fsm_slot *ss = ccid_slot2iso_fsm_slot(cs);
	struct card_uart *cuart = talloc_zero(ctx, struct card_uart);
	char id_buf[16];
	char *devname = "/dev/null";
	int rc;

	LOGPCS(cs, LOGL_DEBUG, "%s\n", __func__);

	if (cs->slot_nr == 0) {
		cs->icc_present = true;
		devname = "/dev/ttyUSB5";
	}

	if (!cuart)
		return -ENOMEM;

	snprintf(id_buf, sizeof(id_buf), "SIM%d", cs->slot_nr);
	rc = card_uart_open(cuart, "tty", devname);
	if (rc < 0) {
		talloc_free(cuart);
		return rc;
	}
	ss->fi = iso7816_fsm_alloc(ctx, LOGL_DEBUG, id_buf, cuart, iso_fsm_clot_user_cb, ss);
	if (!ss->fi) {
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
	.xfr_block_async = iso_fsm_slot_xfr_block_async,
	.set_power = iso_fsm_slot_set_power,
	.set_clock = iso_fsm_slot_set_clock,
	.set_params = iso_fsm_slot_set_params,
	.set_rate_and_clock = iso_fsm_slot_set_rate_and_clock,
};