#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "ccid_proto.h"

enum {
	DCCID,
	DUSB,
};

#define NR_SLOTS	8

#define LOGPCI(ci, lvl, fmt, args ...) LOGP(DCCID, lvl, "%s: " fmt, (ci)->name, ## args)
#define LOGPCS(cs, lvl, fmt, args ...) \
	LOGP(DCCID, lvl, "%s(%u): " fmt, (cs)->ci->name, (cs)->slot_nr, ## args)

struct msgb;

struct ccid_pars_decoded {
	/* global for T0/T1 */
	uint8_t fi;
	uint8_t di;
	enum ccid_clock_stop clock_stop;
	bool inverse_convention;

	struct {
		uint8_t guard_time_etu;
		uint8_t waiting_integer;
	} t0;

	struct {
		enum ccid_t1_csum_type csum_type;
		uint8_t guard_time_t1;
		uint8_t bwi;
		uint8_t cwi;
		uint8_t ifsc;
		uint8_t nad;
	} t1;
};

struct ccid_slot {
	/* back-pointer to the ccid_instance */
	struct ccid_instance *ci;
	/* number of this slot (0 = first) */
	uint8_t slot_nr;
	/* is there an ICC physically present (card detect)? */
	bool icc_present;
	/* was there an ICC present during the last NotifSlotStatus?
	 * should be set to zero every USB resume and setConfig != 0 */
	bool icc_present_last;
	/* is the ICC physically powered? */
	bool icc_powered;
	/* is the ICC currently in reset? */
	bool icc_in_reset;
	/* is this slot currently busy with processing a CCID command? */
	bool cmd_busy;
	/* decided CCID parameters */
	struct ccid_pars_decoded pars;
	/* default parameters; applied on ResetParameters */
	const struct ccid_pars_decoded *default_pars;
};

/* CCID operations provided by USB transport layer */
struct ccid_ops {
	/* msgb ownership in below functions is transferred, i.e. whoever
	 * provides the callback function must make sure to msgb_free() them
	 * once transmission on IN or INT EP has completed. */
	int (*send_in)(struct ccid_instance *ci, struct msgb *msg);
	int (*send_int)(struct ccid_instance *ci, struct msgb *msg);
};

/* CCID operations provided by actual slot hardware */
struct ccid_slot_ops {
	/* called once on start-up for initialization */
	int (*init)(struct ccid_slot *cs);
	/* called before processing any command for a slot; used e.g. to
	 * update the (power/clock/...) status from the hardware */
	void (*pre_proc_cb)(struct ccid_slot *cs, struct msgb *msg);

	void (*set_power)(struct ccid_slot *cs, bool enable);
	void (*set_clock)(struct ccid_slot *cs, enum ccid_clock_command cmd);
	int (*set_params)(struct ccid_slot *cs, enum ccid_protocol_num proto,
			  const struct ccid_pars_decoded *pars_dec);
	int (*set_rate_and_clock)(struct ccid_slot *cs, uint32_t freq_hz, uint32_t rate_bps);
};

/* An instance of CCID (i.e. a card reader device) */
struct ccid_instance {
	/* slots within the reader */
	struct ccid_slot slot[NR_SLOTS];
	/* set of function pointers implementing specific operations */
	const struct ccid_ops *ops;
	const struct ccid_slot_ops *slot_ops;
	const char *name;
	/* user-supplied opaque data */
	void *priv;
};

void ccid_instance_init(struct ccid_instance *ci, const struct ccid_ops *ops,
			const struct ccid_slot_ops *slot_ops, const char *name, void *priv);
int ccid_handle_out(struct ccid_instance *ci, struct msgb *msg);
