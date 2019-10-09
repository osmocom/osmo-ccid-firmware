/* Simulated CCID card slot. This is used in absence of a real hardware back-end
 * in order to test the CCID firmware codebase in a virtual environment */

#include <osmocom/core/msgb.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/logging.h>

#include "ccid_device.h"

struct slotsim_slot {
	struct osmo_timer_list pwron_timer;
	struct osmo_timer_list xfr_timer;
	/* bSeq of the operation currently in progress */
	uint8_t seq;
};

struct slotsim_instance {
	struct slotsim_slot slot[NR_SLOTS];
};

static struct slotsim_instance g_si;

struct slotsim_slot *ccid_slot2slotsim_slot(struct ccid_slot *cs)
{
	OSMO_ASSERT(cs->slot_nr < ARRAY_SIZE(g_si.slot));
	return &g_si.slot[cs->slot_nr];
}

static const uint8_t sysmousim_sjs1_atr[] = {
		0x3B, 0x9F, 0x96, 0x80, 0x1F, 0xC7, 0x80, 0x31,
		0xA0, 0x73, 0xBE, 0x21, 0x13, 0x67, 0x43, 0x20,
		0x07, 0x18, 0x00, 0x00, 0x01, 0xA5 };

static const struct ccid_pars_decoded slotsim_def_pars = {
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

static void slotsim_pre_proc_cb(struct ccid_slot *cs, struct msgb *msg)
{
	/* do nothing; real hardware would update the slot related state here */
}

static void slotsim_icc_power_on_async(struct ccid_slot *cs, struct msgb *msg,
					const struct ccid_pc_to_rdr_icc_power_on *ipo)
{
	struct slotsim_slot *ss = ccid_slot2slotsim_slot(cs);

	ss->seq = ipo->hdr.bSeq;
	LOGPCS(cs, LOGL_DEBUG, "scheduling pwron_timer\n");
	osmo_timer_schedule(&ss->pwron_timer, 1, 0);
	msgb_free(msg);
	/* continues in timer call-back below */
}
static void slotsim_pwron_timer_cb(void *data)
{
	struct ccid_slot *cs = data;
	struct slotsim_slot *ss = ccid_slot2slotsim_slot(cs);
	struct msgb *resp;

	LOGPCS(cs, LOGL_DEBUG, "%s\n", __func__);

	resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_OK, 0,
				   sysmousim_sjs1_atr, sizeof(sysmousim_sjs1_atr));
	ccid_slot_send_unbusy(cs, resp);
}

static void slotsim_xfr_block_async(struct ccid_slot *cs, struct msgb *msg,
				const struct ccid_pc_to_rdr_xfr_block *xfb)
{
	struct slotsim_slot *ss = ccid_slot2slotsim_slot(cs);

	ss->seq = xfb->hdr.bSeq;
	LOGPCS(cs, LOGL_DEBUG, "scheduling xfr_timer\n");
	osmo_timer_schedule(&ss->xfr_timer, 0, 50000);
	msgb_free(msg);
	/* continues in timer call-back below */
}
static void slotsim_xfr_timer_cb(void *data)
{
	struct ccid_slot *cs = data;
	struct slotsim_slot *ss = ccid_slot2slotsim_slot(cs);
	struct msgb *resp;

	LOGPCS(cs, LOGL_DEBUG, "%s\n", __func__);

	resp = ccid_gen_data_block(cs, ss->seq, CCID_CMD_STATUS_OK, 0, NULL, 0);
	ccid_slot_send_unbusy(cs, resp);
}


static void slotsim_set_power(struct ccid_slot *cs, bool enable)
{
	if (enable) {
		cs->icc_powered = true;
		/* FIXME: What to do about ATR? */
	} else {
		cs->icc_powered = false;
	}
}

static void slotsim_set_clock(struct ccid_slot *cs, enum ccid_clock_command cmd)
{
	/* FIXME */
	switch (cmd) {
	case CCID_CLOCK_CMD_STOP:
		break;
	case CCID_CLOCK_CMD_RESTART:
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static int slotsim_set_params(struct ccid_slot *cs, enum ccid_protocol_num proto,
				const struct ccid_pars_decoded *pars_dec)
{
	/* we always acknowledge all parameters */
	return 0;
}

static int slotsim_set_rate_and_clock(struct ccid_slot *cs, uint32_t freq_hz, uint32_t rate_bps)
{
	/* we always acknowledge all rates/clocks */
	return 0;
}


static int slotsim_init(struct ccid_slot *cs)
{
	struct slotsim_slot *ss = ccid_slot2slotsim_slot(cs);

	LOGPCS(cs, LOGL_DEBUG, "%s\n", __func__);
	cs->icc_present = true;
	cs->icc_powered = true;
	osmo_timer_setup(&ss->pwron_timer, slotsim_pwron_timer_cb, cs);
	osmo_timer_setup(&ss->xfr_timer, slotsim_xfr_timer_cb, cs);
	cs->default_pars = &slotsim_def_pars;
	return 0;
}

const struct ccid_slot_ops slotsim_slot_ops = {
	.init = slotsim_init,
	.pre_proc_cb = slotsim_pre_proc_cb,
	.icc_power_on_async = slotsim_icc_power_on_async,
	.xfr_block_async = slotsim_xfr_block_async,
	.set_power = slotsim_set_power,
	.set_clock = slotsim_set_clock,
	.set_params = slotsim_set_params,
	.set_rate_and_clock = slotsim_set_rate_and_clock,
};
