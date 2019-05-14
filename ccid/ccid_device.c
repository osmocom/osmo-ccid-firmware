#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/utils.h>

#include "ccid_proto.h"

#define NR_SLOTS	8

struct ccid_slot {
	struct ccid_instance *ci;
	uint8_t slot_nr;
	bool icc_present;
	bool icc_powered;
	bool icc_in_reset;
};

struct ccid_ops {
	int (*send_in)(struct ccid_instance *ci, struct msgb *msg);
};

struct ccid_instance {
	struct ccid_slot slot[NR_SLOTS];
	struct ccid_ops ops;
};

#define msgb_ccid_out(x) (union ccid_pc_to_rdr *)msgb_data(x)
#define msgb_ccid_in(x) (union ccid_rdr_to_pc *)msgb_data(x)

static struct ccid_slot *get_ccid_slot(struct ccid_instance *ci, uint8_t slot_nr)
{
	if (slot_nr >= sizeof(ci->slot))
		return NULL;
	else
		return &ci->slot[slot_nr];
}

static uint8_t get_icc_status(const struct ccid_slot *cs)
{
	if (cs->icc_present && cs->icc_powered && !cs->icc_in_reset)
		return CCID_ICC_STATUS_PRES_ACT;
	else if (!cs->icc_present)
		return CCID_ICC_STATUS_NO_ICC;
	else
		return CCID_ICC_STATUS_PRES_INACT;
}

#define SET_HDR(x, msg_type, slot, seq) do {	\
	(x)->hdr.bMessageType = msg_type;	\
	(x)->hdr.dwLength = 0;		\
	(x)->hdr.bSlot = slot;			\
	(x)->hdr.bSeq = seq;			\
	} while (0)

#define SET_HDR_IN(x, msg_type, slot, seq, status, error) do {	\
	SET_HDR(&(x)->hdr, msg_type, slot, seq);		\
	(x)->hdr.bStatus = status;				\
	(x)->hdr.bError = error;				\
	} while (0)

/***********************************************************************
 * Message generation / sending
 ***********************************************************************/

static struct msgb *ccid_msgb_alloc(void)
{
	struct msgb *msg = msgb_alloc(512, "ccid");
	OSMO_ASSERT(msg);
	return msg;
}

static int ccid_send(struct ccid_instance *ci, struct msgb *msg)
{
	return ci->ops.send_in(ci, msg);
}

static int ccid_slot_send(struct ccid_slot *cs, struct msgb *msg)
{
	struct ccid_header *ch = (struct ccid_header *) msgb_ccid_in(msg);

	/* patch bSlotNr into message */
	ch->bSlot = cs->slot_nr;
	return ccid_send(cs->ci, msg);
}


/* Section 6.2.1 */
static struct msgb *ccid_gen_data_block(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
					 enum ccid_error_code err, const uint8_t *data,
					 uint32_t data_len)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_data_block *db = 
		(struct ccid_rdr_to_pc_data_block *) msgb_put(msg, sizeof(*db) + data_len);
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | get_icc_status(cs);

	SET_HDR_IN(db, RDR_to_PC_DataBlock, cs->slot_nr, seq, sts, err);
	osmo_store32le(data_len, &db->hdr.hdr.dwLength);
	memcpy(db->abData, data, data_len);
	return msg;
}

/* Section 6.2.2 */
static struct msgb *ccid_gen_slot_status(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
					 enum ccid_error_code err)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_slot_status *ss =
		(struct ccid_rdr_to_pc_slot_status *) msgb_put(msg, sizeof(*ss));
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | get_icc_status(cs);

	SET_HDR_IN(ss, RDR_to_PC_SlotStatus, cs->slot_nr, seq, sts, err);
	return msg;
}

/* Section 6.2.3 */
/* TODO */

/* Section 6.2.4 */
static struct msgb *ccid_gen_escape(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
				    enum ccid_error_code err, const uint8_t *data,
				    uint32_t data_len)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_escape *esc =
		(struct ccid_rdr_to_pc_escape *) msgb_put(msg, sizeof(*esc) + data_len);
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | get_icc_status(cs);

	SET_HDR_IN(esc, RDR_to_PC_Escape, cs->slot_nr, seq, sts, err);
	osmo_store32le(data_len, &esc->hdr.hdr.dwLength);
	memcpy(esc->abData, data, data_len);
	return msg;
}

/* Section 6.2.5 */
static struct msgb *ccid_gen_clock_and_rate(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
					    enum ccid_error_code err, uint32_t clock_khz, uint32_t rate_bps)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_data_rate_and_clock *drc =
		(struct ccid_rdr_to_pc_data_rate_and_clock *) msgb_put(msg, sizeof(*drc));
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | get_icc_status(cs);

	SET_HDR_IN(drc, RDR_to_PC_DataRateAndClockFrequency, cs->slot_nr, seq, sts, err);
	osmo_store32le(8, &drc->hdr.hdr.dwLength); /* Message-specific data length (wtf?) */
	osmo_store32le(clock_khz, &drc->dwClockFrequency); /* kHz */
	osmo_store32le(rate_bps, &drc->dwDataRate); /* bps */
	return msg;
}



#if 0
static struct msgb *gen_err_resp(struct ccid_instance *ci, enum ccid_msg_type msg_type,
				 enum ccid_error_code err_code)
{
	struct c
}
#endif

/***********************************************************************
 * Message reception / parsing
 ***********************************************************************/

/* Section 6.1.3 */
static int ccid_handle_get_slot_status(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, u->get_slot_status.hdr.bSeq, CCID_CMD_STATUS_OK, 0);

	return ccid_send(cs->ci, resp);
}


/* Section 6.1.1 */
static int ccid_handle_icc_power_on(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	/* TODO: send actual ATR; handle error cases */
	/* TODO: handle this asynchronously */
	resp = ccid_gen_data_block(cs, u->icc_power_on.hdr.bSeq, CCID_CMD_STATUS_OK, 0, NULL, 0);

	return ccid_send(cs->ci, resp);
}

/* Section 6.1.2 */
static int ccid_handle_icc_power_off(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, u->get_slot_status.hdr.bSeq, CCID_CMD_STATUS_OK, 0);
	return ccid_send(cs->ci, resp);
}

/* Section 6.1.4 */
static int ccid_handle_xfr_block(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	resp = ccid_gen_data_block(cs, u->icc_power_on.hdr.bSeq, CCID_CMD_STATUS_OK, 0, NULL, 0);
	return ccid_send(cs->ci, resp);
}

/* Section 6.1.5 */
static int ccid_handle_get_parameters(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;
}

/* Section 6.1.6 */
static int ccid_handle_reset_parameters(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;
}

/* Section 6.1.7 */
static int ccid_handle_set_parameters(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;
}

/* Section 6.1.8 */
static int ccid_handle_escape(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;
}

/* Section 6.1.9 */
static int ccid_handle_icc_clock(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, u->get_slot_status.hdr.bSeq, CCID_CMD_STATUS_OK, 0);
	return ccid_send(cs->ci, resp);
}

/* Section 6.1.10 */
static int ccid_handle_t0apdu(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, u->get_slot_status.hdr.bSeq, CCID_CMD_STATUS_OK, 0);
	return ccid_send(cs->ci, resp);
}

/* Section 6.1.11 */
static int ccid_handle_secure(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
}

/* Section 6.1.12 */
static int ccid_handle_mechanical(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, u->get_slot_status.hdr.bSeq, CCID_CMD_STATUS_OK, 0);
	return ccid_send(cs->ci, resp);
}

/* Section 6.1.13 */
static int ccid_handle_abort(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, u->get_slot_status.hdr.bSeq, CCID_CMD_STATUS_OK, 0);
	return ccid_send(cs->ci, resp);
}

/* Section 6.1.14 */
static int ccid_handle_set_rate_and_clock(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	struct msgb *resp;
}

/* handle data arriving from the host on the OUT endpoint */
int ccid_handle_out(struct ccid_instance *ci, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	unsigned int len = msgb_length(msg);
	struct ccid_slot *cs;
	int rc;

	if (len < sizeof(*ch)) {
		/* FIXME */
		return -1;
	}

	cs = get_ccid_slot(ci, ch->bSlot);
	if (!cs) {
		/* FIXME */
		return -1;
	}

	switch (ch->bMessageType) {
	case PC_to_RDR_GetSlotStatus:
		if (len != sizeof(u->get_slot_status))
			goto short_msg;
		rc = ccid_handle_get_slot_status(cs, msg);
		break;
	case PC_to_RDR_IccPowerOn:
		if (len != sizeof(u->icc_power_on))
			goto short_msg;
		rc = ccid_handle_icc_power_on(cs, msg);
		break;
	case PC_to_RDR_IccPowerOff:
		if (len != sizeof(u->icc_power_off))
			goto short_msg;
		rc = ccid_handle_icc_power_off(cs, msg);
		break;
	case PC_to_RDR_XfrBlock:
		if (len < sizeof(u->xfr_block))
			goto short_msg;
		rc = ccid_handle_xfr_block(cs, msg);
		break;
	case PC_to_RDR_GetParameters:
		if (len != sizeof(u->get_parameters))
			goto short_msg;
		rc = ccid_handle_get_parameters(cs, msg);
		break;
	case PC_to_RDR_ResetParameters:
		if (len != sizeof(u->reset_parameters))
			goto short_msg;
		rc = ccid_handle_reset_parameters(cs, msg);
		break;
	case PC_to_RDR_SetParameters:
		if (len != sizeof(u->set_parameters))
			goto short_msg;
		rc = ccid_handle_set_parameters(cs, msg);
		break;
	case PC_to_RDR_Escape:
		if (len < sizeof(u->escape))
			goto short_msg;
		rc = ccid_handle_escape(cs, msg);
		break;
	case PC_to_RDR_IccClock:
		if (len != sizeof(u->icc_clock))
			goto short_msg;
		rc = ccid_handle_icc_clock(cs, msg);
		break;
	case PC_to_RDR_T0APDU:
		if (len != /*FIXME*/ sizeof(u->t0apdu))
			goto short_msg;
		rc = ccid_handle_t0apdu(cs, msg);
		break;
	case PC_to_RDR_Secure:
		if (len < sizeof(u->secure))
			goto short_msg;
		rc = ccid_handle_secure(cs, msg);
		break;
	case PC_to_RDR_Mechanical:
		if (len != sizeof(u->mechanical))
			goto short_msg;
		rc = ccid_handle_mechanical(cs, msg);
		break;
	case PC_to_RDR_Abort:
		if (len != sizeof(u->abort))
			goto short_msg;
		rc = ccid_handle_abort(cs, msg);
		break;
	case PC_to_RDR_SetDataRateAndClockFrequency:
		if (len != sizeof(u->set_rate_and_clock))
			goto short_msg;
		rc = ccid_handle_set_rate_and_clock(cs, msg);
		break;
	default:
		/* FIXME */
		break;
	}
	return 0;

short_msg:
	/* FIXME */
	return -1;
}
