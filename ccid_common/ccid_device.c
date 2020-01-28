#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>

#include "ccid_proto.h"
#include "ccid_device.h"

/* local, stand-alone definition of a USB control request */
struct _usb_ctrl_req {
	uint8_t bRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__ ((packed));;

/* decode on-the-wire T0 parameters into their parsed form */
static int decode_ccid_pars_t0(struct ccid_pars_decoded *out, const struct ccid_proto_data_t0 *in)
{
	/* input validation: only 0x00 and 0x02 permitted for bmTCCKST0
	 * if (in->bmTCCKST0 & 0xFD)
	 *	return -11;
	 * 7816-3 6.1.7 says: "Note: the CCID ignores this bit", placeholder for GETparameters */

	/* input validation: only 0x00 to 0x03 permitted for bClockSTop */
	if (in->bClockStop & 0xFC)
		return -14;

	out->fi = in->bmFindexDindex >> 4;
	out->di = in->bmFindexDindex & 0xF;
	if (in->bmTCCKST0 & 2)
		out->inverse_convention = true;
	else
		out->inverse_convention = false;
	if (in->bGuardTimeT0 == 0xff)
		out->t0.guard_time_etu = 0;
	else
		out->t0.guard_time_etu = in->bGuardTimeT0;
	out->t0.waiting_integer = in->bWaitingIntegerT0;
	out->clock_stop = in->bClockStop & 0x03;

	return 0;
}

/* encode T0 parameters from parsed form into on-the-wire encoding */
static void encode_ccid_pars_t0(struct ccid_proto_data_t0 *out, const struct ccid_pars_decoded *in)
{
	out->bmFindexDindex = ((in->fi << 4) & 0xF0) | (in->di & 0x0F);
	if (in->inverse_convention)
		out->bmTCCKST0 = 0x02;
	else
		out->bmTCCKST0 = 0x00;
	out->bGuardTimeT0 = in->t0.guard_time_etu;
	out->bWaitingIntegerT0 = in->t0.waiting_integer;
	out->bClockStop = in->clock_stop & 0x03;
}

/* decode on-the-wire T1 parameters into their parsed form */
static int decode_ccid_pars_t1(struct ccid_pars_decoded *out, const struct ccid_proto_data_t1 *in)
{
	/* input validation: only some values permitted for bmTCCKST0 */
	if (in->bmTCCKST1 & 0xE8)
		return -11;
	/* input validation: only 0x00 to 0x9F permitted for bmWaitingIntegersT1 */
	if (in->bWaitingIntegersT1 > 0x9F)
		return -13;
	/* input validation: only 0x00 to 0x03 permitted for bClockSTop */
	if (in->bClockStop & 0xFC)
		return -14;
	/* input validation: only 0x00 to 0xFE permitted for bIFSC */
	if (in->bIFSC > 0xFE)
		return -15;

	out->fi = in->bmFindexDindex >> 4;
	out->di = in->bmFindexDindex & 0xF;
	if (in->bmTCCKST1 & 1)
		out->t1.csum_type = CCID_CSUM_TYPE_CRC;
	else
		out->t1.csum_type = CCID_CSUM_TYPE_LRC;
	if (in->bmTCCKST1 & 2)
		out->inverse_convention = true;
	else
		out->inverse_convention = false;
	out->t1.guard_time_t1 = in->bGuardTimeT1;
	out->t1.bwi = in->bWaitingIntegersT1 >> 4;
	out->t1.cwi = in->bWaitingIntegersT1 & 0xF;
	out->clock_stop = in->bClockStop & 0x03;
	out->t1.ifsc = in->bIFSC;
	out->t1.nad = in->bNadValue;

	return 0;
}

/* encode T1 parameters from parsed form into on-the-wire encoding */
static void encode_ccid_pars_t1(struct ccid_proto_data_t1 *out, const struct ccid_pars_decoded *in)
{
	out->bmFindexDindex = ((in->fi << 4) & 0xF0) | (in->di & 0x0F);
	out->bmTCCKST1 = 0x10;
	if (in->t1.csum_type == CCID_CSUM_TYPE_CRC)
		out->bmTCCKST1 |= 0x01;
	if (in->inverse_convention)
		out->bmTCCKST1 |= 0x02;
	out->bGuardTimeT1 = in->t1.guard_time_t1;
	out->bWaitingIntegersT1 = ((in->t1.bwi << 4) & 0xF0) | (in->t1.cwi & 0x0F);
	out->bClockStop = in->clock_stop & 0x03;
	out->bIFSC = in->t1.ifsc;
	out->bNadValue = in->t1.nad;
}

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

#if 0
static uint8_t ccid_pc_to_rdr_get_seq(const struct ccid_pc_to_rdr *u)
{
	const struct ccid_header *ch = (const struct ccid_header *) u;
	return ch->bSeq;
}
#endif

/***********************************************************************
 * Message generation / sending
 ***********************************************************************/

static struct msgb *ccid_msgb_alloc(void)
{
	struct msgb *msg = msgb_alloc(512, "ccid");
	OSMO_ASSERT(msg);
	return msg;
}

/* Send given CCID message */
static int ccid_send(struct ccid_instance *ci, struct msgb *msg)
{
	struct ccid_header *ch = (struct ccid_header *) msgb_ccid_in(msg);
	struct ccid_slot *cs = get_ccid_slot(ci, ch->bSlot);
	if (cs) {
		LOGPCS(cs, LOGL_DEBUG, "Tx CCID(IN) %s %s\n",
			get_value_string(ccid_msg_type_vals, ch->bMessageType), msgb_hexdump(msg));
	} else {
		LOGPCI(ci, LOGL_DEBUG, "Tx CCID(IN) %s %s\n",
			get_value_string(ccid_msg_type_vals, ch->bMessageType), msgb_hexdump(msg));
	}
	return ci->ops->send_in(ci, msg);
}

/* Send given CCID message for given slot; patch bSlot into message */
int ccid_slot_send(struct ccid_slot *cs, struct msgb *msg)
{
	struct ccid_header *ch = (struct ccid_header *) msgb_ccid_in(msg);

	/* patch bSlotNr into message */
	ch->bSlot = cs->slot_nr;
	return ccid_send(cs->ci, msg);
}

/* Send given CCID message and mark slot as un-busy */
int ccid_slot_send_unbusy(struct ccid_slot *cs, struct msgb *msg)
{
	cs->cmd_busy = false;
	return ccid_slot_send(cs, msg);
}

/* Section 6.2.1 */
static struct msgb *ccid_gen_data_block_nr(uint8_t slot_nr, uint8_t icc_status, uint8_t seq,
					   uint8_t cmd_sts, enum ccid_error_code err,
					   const uint8_t *data, uint32_t data_len)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_data_block *db =
		(struct ccid_rdr_to_pc_data_block *) msgb_put(msg, sizeof(*db) + data_len);
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | icc_status;

	SET_HDR_IN(db, RDR_to_PC_DataBlock, slot_nr, seq, sts, err);
	osmo_store32le(data_len, &db->hdr.hdr.dwLength);
	memcpy(db->abData, data, data_len);
	return msg;
}
struct msgb *ccid_gen_data_block(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
				 enum ccid_error_code err, const uint8_t *data,
				 uint32_t data_len)
{
	return ccid_gen_data_block_nr(cs->slot_nr, get_icc_status(cs), seq, cmd_sts, err, data, data_len);
}

/* Section 6.2.2 */
static struct msgb *ccid_gen_slot_status_nr(uint8_t slot_nr, uint8_t icc_status,
					    uint8_t seq, uint8_t cmd_sts,
					    enum ccid_error_code err)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_slot_status *ss =
		(struct ccid_rdr_to_pc_slot_status *) msgb_put(msg, sizeof(*ss));
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | icc_status;

	SET_HDR_IN(ss, RDR_to_PC_SlotStatus, slot_nr, seq, sts, err);
	return msg;
}
struct msgb *ccid_gen_slot_status(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
				  enum ccid_error_code err)
{
	return ccid_gen_slot_status_nr(cs->slot_nr, get_icc_status(cs), seq, cmd_sts, err);
}

/* Section 6.2.3 */
static struct msgb *ccid_gen_parameters_t0_nr(uint8_t slot_nr, uint8_t icc_status,
					      uint8_t seq, uint8_t cmd_sts, enum ccid_error_code err,
					      const struct ccid_pars_decoded *dec_par)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_parameters *par =
		(struct ccid_rdr_to_pc_parameters *) msgb_put(msg, sizeof(par->hdr)+sizeof(par->abProtocolData.t0));
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | icc_status;

	SET_HDR_IN(par, RDR_to_PC_Parameters, slot_nr, seq, sts, err);
	if (dec_par) {
		osmo_store32le(sizeof(par->abProtocolData.t0), &par->hdr.hdr.dwLength);
		encode_ccid_pars_t0(&par->abProtocolData.t0, dec_par);
	}
	return msg;
}
struct msgb *ccid_gen_parameters_t0(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
					   enum ccid_error_code err)
{
	return ccid_gen_parameters_t0_nr(cs->slot_nr, get_icc_status(cs), seq, cmd_sts, err, &cs->pars);
}

static struct msgb *ccid_gen_parameters_t1_nr(uint8_t slot_nr, uint8_t icc_status,
					      uint8_t seq, uint8_t cmd_sts, enum ccid_error_code err,
					      const struct ccid_pars_decoded *dec_par)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_parameters *par =
		(struct ccid_rdr_to_pc_parameters *) msgb_put(msg, sizeof(par->hdr)+sizeof(par->abProtocolData.t1));
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | icc_status;

	SET_HDR_IN(par, RDR_to_PC_Parameters, slot_nr, seq, sts, err);
	if (dec_par) {
		osmo_store32le(sizeof(par->abProtocolData.t1), &par->hdr.hdr.dwLength);
		encode_ccid_pars_t1(&par->abProtocolData.t1, dec_par);
	}
	return msg;
}
struct msgb *ccid_gen_parameters_t1(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
					   enum ccid_error_code err)
{
	return ccid_gen_parameters_t1_nr(cs->slot_nr, get_icc_status(cs), seq, cmd_sts, err, &cs->pars);
}


/* Section 6.2.4 */
static struct msgb *ccid_gen_escape_nr(uint8_t slot_nr, uint8_t icc_status, uint8_t seq, uint8_t cmd_sts,
					enum ccid_error_code err, const uint8_t *data, uint32_t data_len)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_escape *esc =
		(struct ccid_rdr_to_pc_escape *) msgb_put(msg, sizeof(*esc) + data_len);
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | icc_status;

	SET_HDR_IN(esc, RDR_to_PC_Escape, slot_nr, seq, sts, err);
	osmo_store32le(data_len, &esc->hdr.hdr.dwLength);
	memcpy(esc->abData, data, data_len);
	return msg;
}
static struct msgb *ccid_gen_escape(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
				    enum ccid_error_code err, const uint8_t *data,
				    uint32_t data_len)
{
	return ccid_gen_escape_nr(cs->slot_nr, get_icc_status(cs), seq, cmd_sts, err, data, data_len);
}

/* Section 6.2.5 */
static struct msgb *ccid_gen_clock_and_rate_nr(uint8_t slot_nr, uint8_t icc_status, uint8_t seq,
						uint8_t cmd_sts, enum ccid_error_code err,
						uint32_t clock_khz, uint32_t rate_bps)
{
	struct msgb *msg = ccid_msgb_alloc();
	struct ccid_rdr_to_pc_data_rate_and_clock *drc =
		(struct ccid_rdr_to_pc_data_rate_and_clock *) msgb_put(msg, sizeof(*drc));
	uint8_t sts = (cmd_sts & CCID_CMD_STATUS_MASK) | icc_status;

	SET_HDR_IN(drc, RDR_to_PC_DataRateAndClockFrequency, slot_nr, seq, sts, err);
	osmo_store32le(8, &drc->hdr.hdr.dwLength); /* Message-specific data length (wtf?) */
	osmo_store32le(clock_khz, &drc->dwClockFrequency); /* kHz */
	osmo_store32le(rate_bps, &drc->dwDataRate); /* bps */
	return msg;
}
static struct msgb *ccid_gen_clock_and_rate(struct ccid_slot *cs, uint8_t seq, uint8_t cmd_sts,
					    enum ccid_error_code err, uint32_t clock_khz,
					    uint32_t rate_bps)
{
	return ccid_gen_clock_and_rate_nr(cs->slot_nr, get_icc_status(cs), seq, cmd_sts, err,
					  clock_khz, rate_bps);
}

/*! generate an error response for given input message_type/slot_nr/seq
 *  \param[in] msg_type CCID Message Type against which response is to be created
 *  \param[in] slot_nr CCID Slot Number
 *  \param[in] icc_status ICC Status of the slot
 *  \param[in] seq CCID Sequence number
 *  \param[in] err_code CCID Error Code to send
 *  \returns dynamically-allocated message buffer containing error response */
static struct msgb *gen_err_resp(enum ccid_msg_type msg_type, uint8_t slot_nr, uint8_t icc_status,
				 uint8_t seq, enum ccid_error_code err_code)
{
	struct msgb *resp = NULL;

	switch (msg_type) {
	case PC_to_RDR_IccPowerOn:
	case PC_to_RDR_XfrBlock:
	case PC_to_RDR_Secure:
		/* Return RDR_to_PC_DataBlock */
		resp = ccid_gen_data_block_nr(slot_nr, icc_status, seq, CCID_CMD_STATUS_FAILED,
						err_code, NULL, 0);
		break;

	case PC_to_RDR_IccPowerOff:
	case PC_to_RDR_GetSlotStatus:
	case PC_to_RDR_IccClock:
	case PC_to_RDR_T0APDU:
	case PC_to_RDR_Mechanical:
	case PC_to_RDR_Abort:
		/* Return RDR_to_PC_SlotStatus */
		resp = ccid_gen_slot_status_nr(slot_nr, icc_status, seq, CCID_CMD_STATUS_FAILED,
						err_code);
		break;

	case PC_to_RDR_GetParameters:
	case PC_to_RDR_ResetParameters:
	case PC_to_RDR_SetParameters:
		/* Return RDR_to_PC_Parameters */
		resp = ccid_gen_parameters_t0_nr(slot_nr, icc_status, seq, CCID_CMD_STATUS_FAILED,
						 err_code, NULL); /* FIXME: parameters? */
		break;

	case PC_to_RDR_Escape:
		/* Return RDR_to_PC_Escape */
		resp = ccid_gen_escape_nr(slot_nr, icc_status, seq, CCID_CMD_STATUS_FAILED,
					  err_code, NULL, 0);
		break;

	case PC_to_RDR_SetDataRateAndClockFrequency:
		/* Return RDR_to_PC_SlotStatus */
		resp = ccid_gen_slot_status_nr(slot_nr, icc_status, seq, CCID_CMD_STATUS_FAILED,
						err_code);
		break;

	default:
		/* generate general error */
		resp = ccid_gen_slot_status_nr(slot_nr, icc_status, seq, CCID_CMD_STATUS_FAILED,
						CCID_ERR_CMD_NOT_SUPPORTED);
		break;
	}
	return resp;
}

/***********************************************************************
 * Message reception / parsing
 ***********************************************************************/

/* Section 6.1.3 */
static int ccid_handle_get_slot_status(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->get_slot_status.hdr.bSeq;
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_OK, 0);

	return ccid_slot_send_unbusy(cs, resp);
}


/* Section 6.1.1 */
static int ccid_handle_icc_power_on(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;

	/* handle this asynchronously */
	cs->ci->slot_ops->icc_power_on_async(cs, msg, &u->icc_power_on);
	return 1;
}

/* Section 6.1.2 */
static int ccid_handle_icc_power_off(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->icc_power_off.hdr.bSeq;
	struct msgb *resp;

	cs->ci->slot_ops->set_power(cs, false);
	resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_OK, 0);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.4 */
static int ccid_handle_xfr_block(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	struct msgb *resp;
	int rc;

	/* handle this asynchronously */
	rc = cs->ci->slot_ops->xfr_block_async(cs, msg, &u->xfr_block);
	if (rc < 0) {
		msgb_trim(msg, sizeof(struct ccid_rdr_to_pc_data_block));
		resp = ccid_gen_data_block(cs, u->xfr_block.hdr.bSeq, CCID_CMD_STATUS_FAILED, -rc, 0, 0);
		goto out;
	}
	/* busy */
	return 1;
out:
	ccid_slot_send_unbusy(cs, resp);
	return 1;
}

/* Section 6.1.5 */
static int ccid_handle_get_parameters(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->get_parameters.hdr.bSeq;
	struct msgb *resp;

	/* FIXME: T=1 */
	resp = ccid_gen_parameters_t0(cs, seq, CCID_CMD_STATUS_OK, 0);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.6 */
static int ccid_handle_reset_parameters(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->reset_parameters.hdr.bSeq;
	struct msgb *resp;
	int rc;

	/* copy default parameters from somewhere */
	/* FIXME: T=1 */

	/* validate parameters; abort if they are not supported */
	rc = cs->ci->slot_ops->set_params(cs, seq, CCID_PROTOCOL_NUM_T0, cs->default_pars);
	if (rc < 0) {
		resp = ccid_gen_parameters_t0(cs, seq, CCID_CMD_STATUS_FAILED, -rc);
		goto out;
	}

	msgb_free(msg);
	/* busy, tdpu like callback */
	return 1;
out:
	msgb_free(msg);
	ccid_slot_send_unbusy(cs, resp);
	return 1;
}

/* Section 6.1.7 */
static int ccid_handle_set_parameters(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_pc_to_rdr_set_parameters *spar = &u->set_parameters;
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->set_parameters.hdr.bSeq;
	struct ccid_pars_decoded pars_dec;
	struct msgb *resp;
	int rc;

	switch (spar->bProtocolNum) {
	case CCID_PROTOCOL_NUM_T0:
		rc = decode_ccid_pars_t0(&pars_dec, &spar->abProtocolData.t0);
		break;
	case CCID_PROTOCOL_NUM_T1:
		rc = decode_ccid_pars_t1(&pars_dec, &spar->abProtocolData.t1);
		break;
	default:
		LOGP(DCCID, LOGL_ERROR, "SetParameters: Invalid Protocol 0x%02x\n",spar->bProtocolNum);
		resp = ccid_gen_parameters_t0(cs, seq, CCID_CMD_STATUS_FAILED, 0);
		goto out;
	}

	if (rc < 0) {
		LOGP(DCCID, LOGL_ERROR, "SetParameters: Unable to parse: %d\n", rc);
		resp = ccid_gen_parameters_t0(cs, seq, CCID_CMD_STATUS_FAILED, -rc);
		goto out;
	}

	cs->proposed_pars = pars_dec;

	/* validate parameters; abort if they are not supported */
	rc = cs->ci->slot_ops->set_params(cs, seq, spar->bProtocolNum, &pars_dec);
	if (rc < 0) {
		resp = ccid_gen_parameters_t0(cs, seq, CCID_CMD_STATUS_FAILED, -rc);
		goto out;
	}

	msgb_free(msg);
	/* busy, tdpu like callback */
	return 1;
out:
	msgb_free(msg);
	ccid_slot_send_unbusy(cs, resp);
	return 1;
}

/* Section 6.1.8 */
static int ccid_handle_escape(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->escape.hdr.bSeq;
	struct msgb *resp;

	resp = ccid_gen_escape(cs, seq, CCID_CMD_STATUS_FAILED, CCID_ERR_CMD_NOT_SUPPORTED, NULL, 0);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.9 */
static int ccid_handle_icc_clock(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->icc_clock.hdr.bSeq;
	struct msgb *resp;

	cs->ci->slot_ops->set_clock(cs, u->icc_clock.bClockCommand);
	resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_OK, 0);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.10 */
static int ccid_handle_t0apdu(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->t0apdu.hdr.bSeq;
	struct msgb *resp;

	/* FIXME: Required for APDU level exchange */
	//resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_OK, 0);
	resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_FAILED, CCID_ERR_CMD_NOT_SUPPORTED);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.11 */
static int ccid_handle_secure(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->secure.hdr.bSeq;
	struct msgb *resp;

	/* FIXME */
	resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_FAILED, CCID_ERR_CMD_NOT_SUPPORTED);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.12 */
static int ccid_handle_mechanical(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->mechanical.hdr.bSeq;
	struct msgb *resp;

	resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_FAILED, CCID_ERR_CMD_NOT_SUPPORTED);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.13 */
static int ccid_handle_abort(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->abort.hdr.bSeq;
	struct msgb *resp;

	/* Check if the currently in-progress message is Abortable */
	switch (0/* FIXME */) {
	case PC_to_RDR_IccPowerOn:
	case PC_to_RDR_XfrBlock:
	case PC_to_RDR_Escape:
	case PC_to_RDR_Secure:
	case PC_to_RDR_Mechanical:
	//case PC_to_RDR_Abort: /* seriously? WTF! */
		break;
	default:
		LOGP(DCCID, LOGL_ERROR, "Abort for non-Abortable Message Type\n");
		/* CCID spec lists CMD_NOT_ABORTED, but gives no numberic value ?!? */
		resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_FAILED, CCID_ERR_CMD_NOT_SUPPORTED);
		return ccid_slot_send_unbusy(cs, resp);
	}

	/* FIXME */
	resp = ccid_gen_slot_status(cs, seq, CCID_CMD_STATUS_OK, 0);
	return ccid_slot_send_unbusy(cs, resp);
}

/* Section 6.1.14 */
static int ccid_handle_set_rate_and_clock(struct ccid_slot *cs, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	uint8_t seq = u->set_rate_and_clock.hdr.bSeq;
	uint32_t freq_hz = osmo_load32le(&u->set_rate_and_clock.dwClockFrequency);
	uint32_t rate_bps = osmo_load32le(&u->set_rate_and_clock.dwDataRate);
	struct msgb *resp;
	int rc;

	/* FIXME: which rate to return in failure case? */
	rc = cs->ci->slot_ops->set_rate_and_clock(cs, &freq_hz, &rate_bps);
	if (rc < 0)
		resp = ccid_gen_clock_and_rate(cs, seq, CCID_CMD_STATUS_FAILED, -rc, 9600, 2500000);
	else
		resp = ccid_gen_clock_and_rate(cs, seq, CCID_CMD_STATUS_OK, 0, rate_bps, freq_hz);
	return ccid_slot_send_unbusy(cs, resp);
}

/*! Handle data arriving from the host on the OUT endpoint.
 *  \param[in] cs CCID Instance on which to operate
 *  \param[in] msgb received message buffer containing one CCID OUT EP message from the host.
 *  		    Ownership of message buffer is transferred, i.e. it's our job to msgb_free()
 *  		    it eventually, after we're done with it (could be asynchronously).
 *  \returns 0 on success; negative on error */
int ccid_handle_out(struct ccid_instance *ci, struct msgb *msg)
{
	const union ccid_pc_to_rdr *u = msgb_ccid_out(msg);
	const struct ccid_header *ch = (const struct ccid_header *) u;
	unsigned int len = msgb_length(msg);
	struct ccid_slot *cs;
	struct msgb *resp;
	int rc;

	if (len < sizeof(*ch)) {
		/* FIXME */
		msgb_free(msg);
		return -1;
	}

	/* Check for invalid slot number */
	cs = get_ccid_slot(ci, ch->bSlot);
	if (!cs) {
		LOGPCI(ci, LOGL_ERROR, "Invalid bSlot %u\n", ch->bSlot);
		resp = gen_err_resp(ch->bMessageType, ch->bSlot, CCID_ICC_STATUS_NO_ICC, ch->bSeq, 5);
		msgb_free(msg);
		return ccid_send(ci, resp);
	}

	/* Check if slot is already busy; Reject any additional commands meanwhile */
	if (cs->cmd_busy) {
		LOGPCS(cs, LOGL_ERROR, "Slot Busy, but another cmd received\n");
		/* FIXME: ABORT logic as per section 5.3.1 of CCID Spec v1.1 */
		resp = gen_err_resp(ch->bMessageType, ch->bSlot, get_icc_status(cs), ch->bSeq,
					CCID_ERR_CMD_SLOT_BUSY);
		msgb_free(msg);
		return ccid_send(ci, resp);
	}

	if(!cs->icc_present) {
		LOGPCS(cs, LOGL_ERROR, "No icc present, but another cmd received\n");
		/* FIXME: ABORT logic as per section 5.3.1 of CCID Spec v1.1 */
		resp = gen_err_resp(ch->bMessageType, ch->bSlot, get_icc_status(cs), ch->bSeq,
				CCID_ERR_ICC_MUTE);
		msgb_free(msg);
		return ccid_send(ci, resp);
	}

	LOGPCS(cs, LOGL_DEBUG, "Rx CCID(OUT) %s %s\n",
		get_value_string(ccid_msg_type_vals, ch->bMessageType), msgb_hexdump(msg));

	/* we're now processing a command for the slot; mark slot as busy */
	cs->cmd_busy = true;

	/* TODO: enqueue into the per-slot specific input queue */

	/* call pre-processing call-back function; allows reader to update state */
	if (ci->slot_ops->pre_proc_cb)
		ci->slot_ops->pre_proc_cb(cs, msg);

	switch (ch->bMessageType) {
	case PC_to_RDR_GetSlotStatus:
		if (len < sizeof(u->get_slot_status))
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
		// smallest union member
		if (len < (sizeof(u->set_parameters.abProtocolData.t0)+10))
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
		/* generic response with bERror = 0 (command not supported) */
		LOGP(DCCID, LOGL_NOTICE, "Unknown CCID Message received: 0x%02x\n", ch->bMessageType);
		resp = gen_err_resp(ch->bMessageType, ch->bSlot, CCID_ICC_STATUS_NO_ICC, ch->bSeq,
				    CCID_ERR_CMD_NOT_SUPPORTED);
		msgb_free(msg);
		return ccid_slot_send_unbusy(cs, resp);
	}
	/* the various ccid_handle_* functions can return '1' to tell us that they took ownership
	 * of the msgb */
	if (rc != 1)
		msgb_free(msg);
	return 0;

short_msg:
	LOGP(DCCID, LOGL_ERROR, "Short CCID message received: %s; ignoring\n", msgb_hexdump(msg));
	msgb_free(msg);
	return -1;
}

/* Section 5.3.1 ABORT */
static int ccid_handle_ctrl_abort(struct ccid_instance *ci, const struct _usb_ctrl_req *req)
{
	uint16_t w_value = osmo_load16le(&req->wValue);
	uint8_t slot_nr = w_value & 0xff;
	uint8_t seq = w_value >> 8;
	struct ccid_slot *cs;

	if (slot_nr >= ARRAY_SIZE(ci->slot))
		return CCID_CTRL_RET_INVALID;

	cs = &ci->slot[slot_nr];

	LOGP(DCCID, LOGL_NOTICE, "Not handling PC_to_RDR_Abort; please implement it\n");
	/* Upon receiving the Control pipe ABORT request the CCID should check
	 * the state of the requested slot. */

	/* If the last Bulk-OUT message received by the CCID was a
	 * PC_to_RDR_Abort command with the same bSlot and bSeq as the ABORT
	 * request, then the CCID will respond to the Bulk-OUT message with
	 * the RDR_to_PC_SlotStatus response. */

	/* FIXME */

	/* If the previous Bulk-OUT message received by the CCID was not a
	 * PC_to_RDR_Abort command with the same bSlot and bSeq as the ABORT
	 * request, then the CCID will fail all Bulk-Out commands to that slot
	 * until the PC_to_RDR_Abort command with the same bSlot and bSeq is
	 * received. Bulk-OUT commands will be failed by sending a response
	 * with bmCommandStatus=Failed and bError=CMD_ABORTED. */

	/* FIXME */
	return CCID_CTRL_RET_OK;
}

/* Section 5.3.2 GET_CLOCK_FREQUENCIES */
static int ccid_handle_ctrl_get_clock_freq(struct ccid_instance *ci, const struct _usb_ctrl_req *req,
					   const uint8_t **data_in)
{
	uint16_t len = osmo_load16le(&req->wLength);

	if (len != sizeof(uint32_t) * ci->class_desc->bNumClockSupported)
		return CCID_CTRL_RET_INVALID;

	*data_in = (const uint8_t *) ci->clock_freqs;
	return CCID_CTRL_RET_OK;
}

/* Section 5.3.3 GET_DATA_RATES */
static int ccid_handle_ctrl_get_data_rates(struct ccid_instance *ci, const struct _usb_ctrl_req *req,
					   const uint8_t **data_in)
{
	uint16_t len = osmo_load16le(&req->wLength);

	if (len != sizeof(uint32_t) * ci->class_desc->bNumClockSupported)
		return CCID_CTRL_RET_INVALID;

	*data_in = (const uint8_t *) ci->data_rates;
	return CCID_CTRL_RET_OK;
}

/*! Handle [class specific] CTRL request. We assume the caller has already verified that the
 *  request was made to the correct interface as well as it is a class-specific request.
 *  \param[in] ci CCID Instance for which CTRL request was received
 *  \param[in] ctrl_req buffer holding the 8 bytes CTRL transfer header
 *  \param[out] data_in data to be returned to the host in the IN transaction (if any)
 *  \returns CCID_CTRL_RET_OK, CCID_CTRL_RET_INVALID or CCID_CTRL_RET_UNKNOWN
 */
int ccid_handle_ctrl(struct ccid_instance *ci, const uint8_t *ctrl_req, const uint8_t **data_in)
{
	const struct _usb_ctrl_req *req = (const struct _usb_ctrl_req *) ctrl_req;
	int rc;

	LOGPCI(ci, LOGL_DEBUG, "CTRL bmReqT=0x%02X bRequest=%s, wValue=0x%04X, wIndex=0x%04X, wLength=%d\n",
		req->bRequestType, get_value_string(ccid_class_spec_req_vals, req->bRequest),
		req->wValue, req->wIndex, req->wLength);

	switch (req->bRequest) {
	case CLASS_SPEC_CCID_ABORT:
		rc = ccid_handle_ctrl_abort(ci, req);
		break;
	case CLASS_SPEC_CCID_GET_CLOCK_FREQ:
		rc = ccid_handle_ctrl_get_clock_freq(ci, req, data_in);
		break;
	case CLASS_SPEC_CCID_GET_DATA_RATES:
		rc = ccid_handle_ctrl_get_data_rates(ci, req, data_in);
		break;
	default:
		return CCID_CTRL_RET_UNKNOWN;
	}
	return rc;
}

void ccid_instance_init(struct ccid_instance *ci, const struct ccid_ops *ops,
			const struct ccid_slot_ops *slot_ops,
			const struct usb_ccid_class_descriptor *class_desc,
			const uint32_t *data_rates, const uint32_t *clock_freqs,
			const char *name, void *priv)
{
	int i;

	ci->ops = ops;
	ci->slot_ops = slot_ops;
	ci->class_desc = class_desc;
	ci->clock_freqs = clock_freqs;
	ci->data_rates = data_rates;
	ci->name = name;
	ci->priv = priv;

	for (i = 0; i < ARRAY_SIZE(ci->slot); i++) {
		struct ccid_slot *cs = &ci->slot[i];
		cs->slot_nr = i;
		cs->ci = ci;

		slot_ops->init(cs);
	}

}
