/* Simulated CCID card slot. This is used in absence of a real hardware back-end
 * in order to test the CCID firmware codebase in a virtual environment */

#include "ccid_device.h"

static const struct ccid_pars_decoded slotsim_def_pars = {
	.fi = 0,
	.di = 0,
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
	cs->default_pars = &slotsim_def_pars;
	return 0;
}

const struct ccid_slot_ops slotsim_slot_ops = {
	.init = slotsim_init,
	.pre_proc_cb = slotsim_pre_proc_cb,
	.set_power = slotsim_set_power,
	.set_clock = slotsim_set_clock,
	.set_params = slotsim_set_params,
	.set_rate_and_clock = slotsim_set_rate_and_clock,
};
