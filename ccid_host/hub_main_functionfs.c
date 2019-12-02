
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <signal.h>
#include <sys/types.h>
#include <linux/usb/functionfs.h>
#include <linux/usb/ch11.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(x)  (x)
#define cpu_to_le32(x)  (x)
#else
#define cpu_to_le16(x)  ((((x) >> 8) & 0xffu) | (((x) & 0xffu) << 8))
#define cpu_to_le32(x)  \
	((((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >>  8) | \
	(((x) & 0x0000ff00u) <<  8) | (((x) & 0x000000ffu) << 24))
#endif

#define le32_to_cpu(x)  le32toh(x)
#define le16_to_cpu(x)  le16toh(x)

enum {
	DUSB,
};

/***********************************************************************
 * Actual USB Descriptors
 ***********************************************************************/

static const struct {
	struct usb_functionfs_descs_head_v2 header;
	__le32 fs_count;
	struct {
		struct usb_interface_descriptor intf;
		struct usb_endpoint_descriptor_no_audio ep_int;
	} __attribute__ ((packed)) fs_descs;
} __attribute__ ((packed)) descriptors = {
	.header = {
		.magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
		.flags = cpu_to_le32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_ALL_CTRL_RECIP),
		.length = cpu_to_le32(sizeof(descriptors)),
	},
	.fs_count = cpu_to_le32(2),
	.fs_descs = {
		.intf = {
			.bLength = sizeof(descriptors.fs_descs.intf),
			.bDescriptorType = USB_DT_INTERFACE,
			.bNumEndpoints = 1,
			.bInterfaceClass = USB_CLASS_HUB,
			.iInterface = 1,
		},
		.ep_int = {
			.bLength = sizeof(descriptors.fs_descs.ep_int),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 1 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_INT,
			.wMaxPacketSize = 64,
			.bInterval = 12,
		},
	},
};

#define STR_INTERFACE_ "Osmocom USB Hub"

static const struct {
	struct usb_functionfs_strings_head header;
	struct {
		__le16 code;
		const char str1[sizeof(STR_INTERFACE_)];
	} __attribute__((packed)) lang0;
} __attribute__((packed)) strings = {
	.header = {
		.magic = cpu_to_le32(FUNCTIONFS_STRINGS_MAGIC),
		.length = cpu_to_le32(sizeof(strings)),
		.str_count = cpu_to_le32(1),
		.lang_count = cpu_to_le32(1),
	},
	.lang0 = {
		cpu_to_le16(0x0409), /* en-us */
		STR_INTERFACE_,
	},
};


struct usb2_hub_desc_header {
	__u8 bDescLength;
	__u8 bDescriptorType;
	__u8 bNbrPorts;
	__le16 wHubCharacteristics;
	__u8 bPwrOn2PwrGood;
	__u8 bHubContrCurrent;
	__u8 data[0];
};

#define NUM_PORTS	31
#define HDESC_ARR_BYTES	((NUM_PORTS + 1 + 7) / 8)
static const struct {
	struct usb2_hub_desc_header hdr;
	uint8_t DeviceRemovable[HDESC_ARR_BYTES];
	uint8_t PortPwrCtrlMask[HDESC_ARR_BYTES];
} __attribute__ ((packed)) hub_desc = {
	.hdr = {
		.bDescLength = sizeof(hub_desc),
		.bDescriptorType = USB_DT_HUB,
		.bNbrPorts = NUM_PORTS,
		.wHubCharacteristics = cpu_to_le16(0x0001),
		.bPwrOn2PwrGood = 100,
		.bHubContrCurrent = 0,
	},
};


/***********************************************************************
 * USB FunctionFS interface
 ***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <osmocom/core/select.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/application.h>
#include <osmocom/core/logging.h>

#ifndef FUNCTIONFS_SUPPORTS_POLL
#include <libaio.h>
struct aio_help {
	struct msgb *msg;
	struct iocb *iocb;
};
#endif

/* usb function handle */
struct ufunc_handle {
	struct osmo_fd ep0;
	struct osmo_fd ep_int;
	struct llist_head ep_int_queue;
#ifndef FUNCTIONFS_SUPPORTS_POLL
	struct osmo_fd aio_evfd;
	io_context_t aio_ctx;
	struct aio_help aio_int;
#endif
};

static int ep_int_cb(struct osmo_fd *ofd, unsigned int what)
{
	LOGP(DUSB, LOGL_DEBUG, "%s\n", __func__);
	return 0;
}

const struct value_string ffs_evt_type_names[] = {
	{ FUNCTIONFS_BIND,	"BIND" },
	{ FUNCTIONFS_UNBIND,	"UNBIND" },
	{ FUNCTIONFS_ENABLE,	"ENABLE" },
	{ FUNCTIONFS_DISABLE,	"DISABLE" },
	{ FUNCTIONFS_SETUP,	"SETUP" },
	{ FUNCTIONFS_SUSPEND,	"SUSPEND" },
	{ FUNCTIONFS_RESUME,	"RESUME" },
	{ 0, NULL }
};

/* local, stand-alone definition of a USB control request */
struct _usb_ctrl_req {
	uint8_t bRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__ ((packed));;

/* class requests from the USB 2.0 hub spec, table 11-15 */
#define HUB_CLASS_REQ(dir, type, request) ((((dir) | (type)) << 8) | (request))
/* GetBusState and SetHubDescriptor are optional, omitted */
#define ClearHubFeature         HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_HUB, USB_REQ_CLEAR_FEATURE)
#define ClearPortFeature        HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_PORT, USB_REQ_CLEAR_FEATURE)
#define GetHubDescriptor        HUB_CLASS_REQ(USB_DIR_IN, USB_RT_HUB, USB_REQ_GET_DESCRIPTOR)
#define GetHubStatus            HUB_CLASS_REQ(USB_DIR_IN, USB_RT_HUB, USB_REQ_GET_STATUS)
#define GetPortStatus           HUB_CLASS_REQ(USB_DIR_IN, USB_RT_PORT, USB_REQ_GET_STATUS)
#define SetHubFeature           HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_HUB, USB_REQ_SET_FEATURE)
#define SetPortFeature          HUB_CLASS_REQ(USB_DIR_OUT, USB_RT_PORT, USB_REQ_SET_FEATURE)

static const struct value_string hub_class_spec_req_vals[] = {
	OSMO_VALUE_STRING(USB_REQ_CLEAR_FEATURE),
	OSMO_VALUE_STRING(USB_REQ_GET_DESCRIPTOR),
	OSMO_VALUE_STRING(USB_REQ_GET_STATUS),
	OSMO_VALUE_STRING(USB_REQ_SET_FEATURE),
	{ 0, NULL }
};

#define CCID_CTRL_RET_INVALID     -1
#define CCID_CTRL_RET_UNKNOWN     -2
#define CCID_CTRL_RET_OK  0

/*! Handle [class specific] CTRL request. We assume the caller has already verified that the
 *  request was made to the correct interface as well as it is a class-specific request.
 *  \param[in] ci CCID Instance for which CTRL request was received
 *  \param[in] ctrl_req buffer holding the 8 bytes CTRL transfer header
 *  \param[out] data_in data to be returned to the host in the IN transaction (if any)
 *  \returns CCID_CTRL_RET_OK, CCID_CTRL_RET_INVALID or CCID_CTRL_RET_UNKNOWN
 */
int hub_handle_ctrl(void *ci, const uint8_t *ctrl_req, const uint8_t **data_in)
{
	const struct _usb_ctrl_req *req = (const struct _usb_ctrl_req *) ctrl_req;
	static uint16_t status[2];
	int rc;

	LOGP(DUSB, LOGL_NOTICE, "CTRL bmReqT=0x%02X bRequest=%s, wValue=0x%04X, wIndex=0x%04X, wLength=%d\n",
		req->bRequestType, get_value_string(hub_class_spec_req_vals, req->bRequest),
		req->wValue, req->wIndex, req->wLength);

	switch (req->bRequestType & 0x7f) {
	case USB_RT_HUB:
		switch (req->bRequest) {
		case USB_REQ_GET_DESCRIPTOR:
			if (req->wIndex != 0) {
				LOGP(DUSB, LOGL_ERROR, "GET_DESC wIndex invalid\n");
				return CCID_CTRL_RET_INVALID;
			}
			if (0) // ctrl_req->wValue != FIXME
				return CCID_CTRL_RET_INVALID;
			*data_in = (const uint8_t *) &hub_desc;
			return sizeof(hub_desc);
		case USB_REQ_CLEAR_FEATURE:
			switch (req->wValue) {
			case C_HUB_LOCAL_POWER:
			case C_HUB_OVER_CURRENT:
				return CCID_CTRL_RET_OK;
			}
			break;
		case USB_REQ_GET_STATUS:
			status[0] = cpu_to_le16(HUB_STATUS_LOCAL_POWER);
			status[1] = cpu_to_le16(0);
			*data_in = (const uint8_t *) status;
			return sizeof(status);
		case USB_REQ_SET_FEATURE:
			if (req->wValue > 1)
				return CCID_CTRL_RET_INVALID;
			return CCID_CTRL_RET_OK;
		}
		break;
	case USB_RT_PORT:
		switch (req->bRequest) {
		case USB_REQ_CLEAR_FEATURE:
			switch (req->wValue) {
			case USB_PORT_FEAT_CONNECTION:
			case USB_PORT_FEAT_ENABLE:
			case USB_PORT_FEAT_SUSPEND:
			case USB_PORT_FEAT_OVER_CURRENT:
			case USB_PORT_FEAT_RESET:
			case USB_PORT_FEAT_L1:
			case USB_PORT_FEAT_POWER:
			case USB_PORT_FEAT_LOWSPEED:
			case USB_PORT_FEAT_C_CONNECTION:
			case USB_PORT_FEAT_C_ENABLE:
			case USB_PORT_FEAT_C_SUSPEND:
			case USB_PORT_FEAT_C_OVER_CURRENT:
			case USB_PORT_FEAT_C_RESET:
			case USB_PORT_FEAT_TEST:
			case USB_PORT_FEAT_C_PORT_L1:
				return CCID_CTRL_RET_OK;
			}
			break;
		case USB_REQ_GET_STATUS:
			status[0] = cpu_to_le16(0);
			status[1] = cpu_to_le16(0);
			*data_in = (const uint8_t *) status;
			return sizeof(status);
		case USB_REQ_SET_FEATURE:
			//selector = wIndex >> 8
			//port = wIndex & 0xff
			return CCID_CTRL_RET_OK;
		}
	}
	return CCID_CTRL_RET_UNKNOWN;
}

static void handle_setup(int fd, const struct usb_ctrlrequest *setup)
{
	const uint8_t *data_in = NULL;
	int rc;

	LOGP(DUSB, LOGL_NOTICE, "EP0 SETUP bRequestType=0x%02x, bRequest=0x%02x wValue=0x%04x, "
		"wIndex=0x%04x, wLength=%u\n", setup->bRequestType, setup->bRequest,
		le16_to_cpu(setup->wValue), le16_to_cpu(setup->wIndex), le16_to_cpu(setup->wLength));

	rc = hub_handle_ctrl(NULL, (const uint8_t *) setup, &data_in);
	switch (rc) {
	case CCID_CTRL_RET_INVALID:
		if (setup->bRequestType & USB_DIR_IN)
			read(fd, NULL, 0); /* cause stall */
		else
			write(fd, NULL, 0); /* cause stall */
		break;
	case CCID_CTRL_RET_UNKNOWN:
		/* FIXME: is this correct behavior? */
		if (setup->bRequestType & USB_DIR_IN)
			write(fd, NULL, 0); /* send ZLP */
		else
			read(fd, NULL, 0);
		break;
	default:
		if (setup->bRequestType & USB_DIR_IN) {
			uint16_t len = rc;
			if (setup->wLength < len)
				len = setup->wLength;
			LOGP(DUSB, LOGL_NOTICE, "Writing %u: %s\n", len, osmo_hexdump_nospc(data_in, len));
			write(fd, data_in, le16_to_cpu(len));
		} else
			read(fd, NULL, 0); /* FIXME: control OUT? */
		break;
	}
}

static int ep_0_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct ufunc_handle *uh = (struct ufunc_handle *) ofd->data;
	int rc;

	if (what & BSC_FD_READ) {
		struct usb_functionfs_event evt;
		rc = read(ofd->fd, (uint8_t *)&evt, sizeof(evt));
		if (rc < sizeof(evt))
			return -23;
		LOGP(DUSB, LOGL_NOTICE, "EP0 %s\n", get_value_string(ffs_evt_type_names, evt.type));
		switch (evt.type) {
		case FUNCTIONFS_ENABLE:
			//aio_refill_out(uh);
			break;
		case FUNCTIONFS_SETUP:
			handle_setup(ofd->fd, &evt.u.setup);
			break;
		}

	}
	return 0;
}

#ifndef FUNCTIONFS_SUPPORTS_POLL

/* dequeue the next msgb from ep_int_queue and set up AIO for it */
static void dequeue_aio_write_int(struct ufunc_handle *uh)
{
	struct aio_help *ah = &uh->aio_int;
	struct msgb *d;
	int rc;

	if (ah->msg)
		return;

	d = msgb_dequeue(&uh->ep_int_queue);
	if (!d)
		return;

	OSMO_ASSERT(ah->iocb);
	ah->msg = d;
	io_prep_pwrite(ah->iocb, uh->ep_int.fd, msgb_data(d), msgb_length(d), 0);
	io_set_eventfd(ah->iocb, uh->aio_evfd.fd);
	rc = io_submit(uh->aio_ctx, 1, &ah->iocb);
	OSMO_ASSERT(rc >= 0);
}

static int evfd_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct ufunc_handle *uh = (struct ufunc_handle *) ofd->data;
	struct io_event evt[1];
	struct msgb *msg;
	uint64_t ev_cnt;
	int i, rc;

	rc = read(ofd->fd, &ev_cnt, sizeof(ev_cnt));
	assert(rc == sizeof(ev_cnt));

	rc = io_getevents(uh->aio_ctx, 1, 1, evt, NULL);
	if (rc <= 0) {
		LOGP(DUSB, LOGL_ERROR, "error in io_getevents(): %d\n", rc);
		return rc;
	}

	for (i = 0; i < rc; i++) {
		int fd = evt[i].obj->aio_fildes;
		if (fd == uh->ep_int.fd) {
			/* interrupt endpoint AIO has completed. This means the IRQ transfer
			 * which we generated has reached the host */
			LOGP(DUSB, LOGL_DEBUG, "IRQ AIO completed, free()ing msgb\n");
			msgb_free(uh->aio_int.msg);
			uh->aio_int.msg = NULL;
			dequeue_aio_write_int(uh);
		}
	}
	return 0;
}
#endif


static int ep0_init(struct ufunc_handle *uh)
{
	int rc;

	/* open control endpoint and write descriptors to it */
	rc = open("ep0", O_RDWR);
	assert(rc >= 0);
	osmo_fd_setup(&uh->ep0, rc, BSC_FD_READ, &ep_0_cb, uh, 0);
	osmo_fd_register(&uh->ep0);
	rc = write(uh->ep0.fd, &descriptors, sizeof(descriptors));
	if (rc != sizeof(descriptors)) {
		LOGP(DUSB, LOGL_ERROR, "Cannot write descriptors: %s\n", strerror(errno));
		return -1;
	}
	rc = write(uh->ep0.fd, &strings, sizeof(strings));
	if (rc != sizeof(strings)) {
		LOGP(DUSB, LOGL_ERROR, "Cannot write strings: %s\n", strerror(errno));
		return -1;
	}

	/* open other endpoint file descriptors */
	INIT_LLIST_HEAD(&uh->ep_int_queue);
	rc = open("ep1", O_RDWR);
	assert(rc >= 0);
	osmo_fd_setup(&uh->ep_int, rc, 0, &ep_int_cb, uh, 1);
#ifdef FUNCTIONFS_SUPPORTS_POLL
	osmo_fd_register(&uh->ep_int);
#endif

#ifndef FUNCTIONFS_SUPPORTS_POLL
#include <sys/eventfd.h>
	/* for some absolutely weird reason, gadgetfs+functionfs don't support
	 * the standard methods of non-blocking I/o (select/poll).  We need to
	 * work around using Linux AIO, which is not to be confused with POSIX AIO! */

	memset(&uh->aio_ctx, 0, sizeof(uh->aio_ctx));
	rc = io_setup(1, &uh->aio_ctx);
	OSMO_ASSERT(rc >= 0);

	/* create an eventfd, which will be marked readable once some AIO completes */
	rc = eventfd(0, 0);
	OSMO_ASSERT(rc >= 0);
	osmo_fd_setup(&uh->aio_evfd, rc, BSC_FD_READ, &evfd_cb, uh, 0);
	osmo_fd_register(&uh->aio_evfd);

	uh->aio_int.iocb = malloc(sizeof(struct iocb));
#endif

	return 0;
}

static const struct log_info_cat log_info_cat[] = {
	[DUSB] = {
		.name = "USB",
		.description = "USB Transport",
		.enabled = 1,
		.loglevel = LOGL_NOTICE,
	},
};

static const struct log_info log_info = {
	.cat = log_info_cat,
	.num_cat = ARRAY_SIZE(log_info_cat),
};

static void *g_tall_ctx;

static void signal_handler(int signal)
{
	switch (signal) {
	case SIGUSR1:
		talloc_report_full(g_tall_ctx, stderr);
		break;
	}
}


int main(int argc, char **argv)
{
	struct ufunc_handle ufh = (struct ufunc_handle) { 0, };
	int rc;

	g_tall_ctx = talloc_named_const(NULL, 0, "hub_main_functionfs");
	msgb_talloc_ctx_init(g_tall_ctx, 0);
	osmo_init_logging2(g_tall_ctx, &log_info);

	signal(SIGUSR1, &signal_handler);

	if (argc < 2) {
		fprintf(stderr, "You have to specify the mount-path of the functionfs\n");
		exit(2);
	}

	chdir(argv[1]);
	rc = ep0_init(&ufh);
	if (rc < 0) {
		fprintf(stderr, "Error %d\n", rc);
		exit(1);
	}

	while (1) {
		osmo_select_main(0);
	}
}
