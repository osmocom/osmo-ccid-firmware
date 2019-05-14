#pragma once
#include <stdint.h>
#include <osmocom/core/utils.h>

/* Identifies the length of type of subordinate descriptors of a CCID device
 * Table 5.1-1 Smart Card Device Class descriptors */
struct usb_ccid_class_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdCCID;
	uint8_t  bMaxSlotIndex;
	uint8_t  bVoltageSupport;
	uint32_t dwProtocols;
	uint32_t dwDefaultClock;
	uint32_t dwMaximumClock;
	uint8_t  bNumClockSupported;
	uint32_t dwDataRate;
	uint32_t dwMaxDataRate;
	uint8_t  bNumDataRatesSupported;
	uint32_t dwMaxIFSD;
	uint32_t dwSynchProtocols;
	uint32_t dwMechanical;
	uint32_t dwFeatures;
	uint32_t dwMaxCCIDMessageLength;
	uint8_t  bClassGetResponse;
	uint8_t  bClassEnvelope;
	uint16_t wLcdLayout;
	uint8_t  bPINSupport;
	uint8_t  bMaxCCIDBusySlots;
} __attribute__((packed));
/* handling of bulk out from host */

enum ccid_msg_type {
	/* Section 6.3 / Table 6.3-1: Interrupt IN */
	RDR_to_PC_NotifySlotChange		= 0x50,
	RDR_to_PC_HardwareError			= 0x51,

	/* Section 6.1 / Table 6.1-1: Bulk OUT */
	PC_to_RDR_IccPowerOn			= 0x62,
	PC_to_RDR_IccPowerOff			= 0x63,
	PC_to_RDR_GetSlotStatus			= 0x65,
	PC_to_RDR_XfrBlock			= 0x6f,
	PC_to_RDR_GetParameters			= 0x6c,
	PC_to_RDR_ResetParameters		= 0x6d,
	PC_to_RDR_SetParameters			= 0x61,
	PC_to_RDR_Escape			= 0x6b,
	PC_to_RDR_IccClock			= 0x6e,
	PC_to_RDR_T0APDU			= 0x6a,
	PC_to_RDR_Secure			= 0x69,
	PC_to_RDR_Mechanical			= 0x71,
	PC_to_RDR_Abort				= 0x72,
	PC_to_RDR_SetDataRateAndClockFrequency	= 0x73,

	/* Section 6.2 / Table 6.2-1: Bulk IN */
	RDR_to_PC_DataBlock			= 0x80,
	RDR_to_PC_SlotStatus			= 0x81,
	RDR_to_PC_Parameters			= 0x82,
	RDR_to_PC_Escape			= 0x83,
	RDR_to_PC_DataRateAndClockFrequency	= 0x84,
};

/* CCID message header on BULK-OUT endpoint */
struct ccid_header {
	uint8_t		bMessageType;
	uint32_t	dwLength;
	uint8_t		bSlot;
	uint8_t		bSeq;
} __attribute__ ((packed));

/* CCID Class-Specific Control Request (Section 5.3 / Table 5.3-1) */
enum ccid_class_spec_req {
	CLASS_SPEC_CCID_ABORT		= 0x01,
	CLASS_SPEC_CCID_GET_CLOCK_FREQ	= 0x02,
	CLASS_SPEC_CCID_GET_DATA_RATES	= 0x03
};

/***********************************************************************
 * Bulk OUT
 ***********************************************************************/

/* Section 6.1.1 */
enum ccid_power_select {
	CCID_PWRSEL_AUTO	= 0x00,
	CCID_PWRSEL_5V0		= 0x01,
	CCID_PWRSEL_3V0		= 0x02,
	CCID_PWRSEL_1V8		= 0x03,
};

/* Section 6.1.1 */
struct ccid_pc_to_rdr_icc_power_on {
	struct ccid_header hdr;
	uint8_t bPowerSelect;
	uint8_t abRFU[2];
} __attribute__ ((packed));
/* Response: RDR_to_PC_DataBlock */

/* Section 6.1.2 */
struct ccid_pc_to_rdr_icc_power_off {
	struct ccid_header hdr;
	uint8_t abRFU[3];
} __attribute__ ((packed));
/* Response: RDR_to_PC_SlotStatus */

/* Section 6.1.3 */
struct ccid_pc_to_rdr_get_slot_status {
	struct ccid_header hdr;
	uint8_t abRFU[3];
} __attribute__ ((packed));
/* Response: RDR_to_PC_SlotStatus */

/* Section 6.1.4 */
struct ccid_pc_to_rdr_xfr_block {
	struct ccid_header hdr;
	uint8_t bBWI;
	uint16_t wLevelParameter;
	uint8_t abData[0];
} __attribute__ ((packed));
/* Response: RDR_to_PC_DataBlock */

/* Section 6.1.5 */
struct ccid_pc_to_rdr_get_parameters {
	struct ccid_header hdr;
	uint8_t abRFU[3];
} __attribute__ ((packed));
/* Response: RDR_to_PC_Parameters */

/* Section 6.1.6 */
struct ccid_pc_to_rdr_reset_parameters {
	struct ccid_header hdr;
	uint8_t abRFU[3];
} __attribute__ ((packed));
/* Response: RDR_to_PC_Parameters */

/* Section 6.1.7 */
struct ccid_proto_data_t0 {
	uint8_t bmFindexDindex;
	uint8_t bmTCCKST0;
	uint8_t bGuardTimeT0;
	uint8_t bWaitingIntegerT0;
	uint8_t bClockStop;
} __attribute__ ((packed));
struct ccid_proto_data_t1 {
	uint8_t bmFindexDindex;
	uint8_t bmTCCKST1;
	uint8_t bGuardTimeT1;
	uint8_t bWaitingIntegersT1;
	uint8_t bClockStop;
	uint8_t bFSC;
	uint8_t bNadValue;
} __attribute__ ((packed));
struct ccid_pc_to_rdr_set_parameters {
	struct ccid_header hdr;
	uint8_t bProtocolNum;
	uint8_t abRFU[2];
	union {
		struct ccid_proto_data_t0 t0;
		struct ccid_proto_data_t1 t1;
	} abProtocolData;
} __attribute__ ((packed));
/* Response: RDR_to_PC_Parameters */

/* Section 6.1.8 */
struct ccid_pc_to_rdr_escape {
	struct ccid_header hdr;
	uint8_t abRFU[3];
	uint8_t abData[0];
} __attribute__ ((packed));
/* Response: RDR_to_PC_Escape */

/* Section 6.1.9 */
enum ccid_clock_command {
	CCID_CLOCK_CMD_RESTART		= 0x00,
	CCID_CLOCK_CMD_STOP		= 0x01,
};
struct ccid_pc_to_rdr_icc_clock {
	struct ccid_header hdr;
	uint8_t bClockCommand;
	uint8_t abRFU[2];
} __attribute__ ((packed));
/* response: RDR_to_PC_SlotStatus */

/* Section 6.1.10 */
struct ccid_pc_to_rdr_t0apdu {
	struct ccid_header hdr;
	uint8_t bmChanges;
	uint8_t bClassGetResponse;
	uint8_t bClassEnvelope;
} __attribute__ ((packed));
/* Response: RDR_to_PC_SlotStatus */

/* Section 6.1.11 */
struct ccid_pc_to_rdr_secure {
	struct ccid_header hdr;
	uint8_t bBWI;
	uint16_t wLevelParameter;
	uint8_t abData[0];
} __attribute__ ((packed));
struct ccid_pin_operation_data {
	uint8_t bPINOperation;
	uint8_t abPNDataStructure[0];
} __attribute__ ((packed));
struct ccid_pin_verification_data {
	uint8_t bTimeOut;
	uint8_t bmFormatString;
	uint8_t bmPINBlockString;
	uint8_t bmPINLengthFormat;
	uint16_t wPINMaxExtraDigit;
	uint8_t bEntryValidationCondition;
	uint8_t bNumberMessage;
	uint16_t wLangId;
	uint8_t bMsgIndex;
	uint8_t bTecPrologue;
	uint8_t abPINApdu[0];
} __attribute__ ((packed));
/* Response: RDR_to_PC_DataBlock */

/* Section 6.1.12 */
struct ccid_pc_to_rdr_mechanical {
	struct ccid_header hdr;
	uint8_t bFunction; /* ccid_mech_function */
	uint8_t abRFU[2];
} __attribute__ ((packed));
enum ccid_mech_function {
	CCID_MECH_FN_ACCEPT_CARD	= 0x01,
	CCID_MECH_FN_EJECT_CARD		= 0x02,
	CCID_MECH_FN_CAPTURE_CARD	= 0x03,
	CCID_MECH_FN_LOCK_CARD		= 0x04,
	CCID_MECH_FN_UNLOCK_CARD	= 0x05,
};
/* Response: RDR_to_PC_SlotStatus */

/* Section 6.1.13 */
struct ccid_pc_to_rdr_abort {
	struct ccid_header hdr;
	uint8_t abRFU[3];
} __attribute__ ((packed));
/* Response: RDR_to_PC_SlotStatus */

/* Section 6.1.14 */
struct ccid_pc_to_rdr_set_rate_and_clock {
	struct ccid_header hdr;
	uint8_t abRFU[3];
	uint32_t dwClockFrequency;
	uint32_t dwDataRate;
} __attribute__ ((packed));
/* Response: RDR_to_PC_DataRateAndClockFrequency */

union ccid_pc_to_rdr {
	struct ccid_pc_to_rdr_icc_power_on		icc_power_on;
	struct ccid_pc_to_rdr_icc_power_off		icc_power_off;
	struct ccid_pc_to_rdr_get_slot_status		get_slot_status;
	struct ccid_pc_to_rdr_xfr_block			xfr_block;
	struct ccid_pc_to_rdr_get_parameters		get_parameters;
	struct ccid_pc_to_rdr_reset_parameters		reset_parameters;
	struct ccid_pc_to_rdr_set_parameters		set_parameters;
	struct ccid_pc_to_rdr_escape			escape;
	struct ccid_pc_to_rdr_icc_clock			icc_clock;
	struct ccid_pc_to_rdr_t0apdu			t0apdu;
	struct ccid_pc_to_rdr_secure			secure;
	struct ccid_pc_to_rdr_mechanical		mechanical;
	struct ccid_pc_to_rdr_abort			abort;
	struct ccid_pc_to_rdr_set_rate_and_clock	set_rate_and_clock;
};

/***********************************************************************
 * Bulk IN
 ***********************************************************************/

/* CCID message header on BULK-IN endpoint */
struct ccid_header_in {
	struct ccid_header hdr;
	uint8_t		bStatus;
	uint8_t		bError;
} __attribute__ ((packed));

/* Section 6.2.1 RDR_to_PC_DataBlock */
struct ccid_rdr_to_pc_data_block {
	struct ccid_header_in hdr;
	uint8_t bChainParameter;
	uint8_t abData[0];
} __attribute__ ((packed));

/* Section 6.2.2 RDR_to_PC_SlotStatus */
struct ccid_rdr_to_pc_slot_status {
	struct ccid_header_in hdr;
	uint8_t bClockStatus;
} __attribute__ ((packed));
enum ccid_clock_status {
	CCID_CLOCK_STATUS_RUNNING	= 0x00,
	CCID_CLOCK_STATUS_STOPPED_L	= 0x01,
	CCID_CLOCK_STATUS_STOPPED_H	= 0x02,
	CCID_CLOCK_STATUS_STOPPED_UNKN	= 0x03,
};

/* Section 6.2.3 RDR_to_PC_Parameters */
struct ccid_rdr_to_pc_parameters {
	struct ccid_header_in hdr;
	union {
		struct ccid_proto_data_t0 t0;
		struct ccid_proto_data_t1 t1;
	} abProtocolData;
} __attribute__ ((packed));

/* Section 6.2.4 RDR_to_PC_Escape */
struct ccid_rdr_to_pc_escape {
	struct ccid_header_in hdr;
	uint8_t bRFU;
	uint8_t abData[0];
} __attribute__ ((packed));

/* Section 6.2.5 RDR_to_PC_DataRateAndClockFrequency */
struct ccid_rdr_to_pc_data_rate_and_clock {
	struct ccid_header_in hdr;
	uint8_t bRFU;
	uint32_t dwClockFrequency;
	uint32_t dwDataRate;
} __attribute__ ((packed));

/* Section 6.2.6 */
#define CCID_ICC_STATUS_MASK		0x03
#define CCID_ICC_STATUS_PRES_ACT	0x00
#define CCID_ICC_STATUS_PRES_INACT	0x01
#define CCID_ICC_STATUS_NO_ICC		0x02
#define CCID_CMD_STATUS_MASK		0xC0
#define CCID_CMD_STATUS_OK		0x00
#define CCID_CMD_STATUS_FAILED		0x40
#define CCID_CMD_STATUS_TIME_EXT	0x80
/* Table 6.2-2: Slot Error value when bmCommandStatus == 1 */
enum ccid_error_code {
	CCID_ERR_CMD_ABORTED			= 0xff,
	CCID_ERR_ICC_MUTE			= 0xfe,
	CCID_ERR_XFR_PARITY_ERROR		= 0xfd,
	CCID_ERR_XFR_OVERRUN			= 0xfc,
	CCID_ERR_HW_ERROR			= 0xfb,
	CCID_ERR_BAD_ATR_TS			= 0xf8,
	CCID_ERR_BAD_ATR_TCK			= 0xf7,
	CCID_ERR_ICC_PROTOCOL_NOT_SUPPORTED	= 0xf6,
	CCID_ERR_ICC_CLASS_NOT_SUPPORTED	= 0xf5,
	CCID_ERR_PROCEDURE_BYTE_CONFLICT	= 0xf4,
	CCID_ERR_DEACTIVATED_PROTOCOL		= 0xf3,
	CCID_ERR_BUSY_WITH_AUTO_SEQUENCE	= 0xf2,
	CCID_ERR_PIN_TIMEOUT			= 0xf0,
	CCID_ERR_PIN_CANCELLED			= 0xef,
	CCID_ERR_CMD_SLOT_BUSY			= 0xe0,
	CCID_ERR_CMD_NOT_SUPPORTED		= 0x00
};

union ccid_rdr_to_pc {
	struct ccid_rdr_to_pc_data_block		data_block;
	struct ccid_rdr_to_pc_slot_status		slot_status;
	struct ccid_rdr_to_pc_parameters		parameters;
	struct ccid_rdr_to_pc_escape			escape;
	struct ccid_rdr_to_pc_data_rate_and_clock	rate_and_clock;
};

/***********************************************************************
 * Interupt IN
 ***********************************************************************/

/* Section 6.3.1 */
struct ccid_rdr_to_pc_notify_slot_change {
	uint8_t bMessageType;
	uint8_t bmSlotCCState[0];	/* as long as bNumSlots/4 padded to next byte */
} __attribute__ ((packed));

/* Section 6.3.1 */
struct ccid_rdr_to_pc_hardware_error {
	struct ccid_header hdr;
	uint8_t bHardwareErrorCode;
} __attribute__ ((packed));

union ccid_rdr_to_pc_irq {
	struct ccid_rdr_to_pc_notify_slot_change	slot_change;
	struct ccid_rdr_to_pc_hardware_error		hw_error;
};


extern const struct value_string ccid_msg_type_vals[];
extern const struct value_string ccid_class_spec_req_vals[];
extern const struct value_string ccid_power_select_vals[];
extern const struct value_string ccid_clock_command_vals[];
extern const struct value_string ccid_error_code_vals[];

