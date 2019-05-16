
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <sys/types.h>
#include <linux/usb/functionfs.h>

#include "ccid_proto.h"

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

/***********************************************************************
 * Actual USB CCID Descriptors
 ***********************************************************************/

static const struct {
	struct usb_functionfs_descs_head_v2 header;
	__le32 fs_count;
	struct {
		struct usb_interface_descriptor intf;
		struct usb_ccid_class_descriptor ccid;
		struct usb_endpoint_descriptor_no_audio ep_irq;
		struct usb_endpoint_descriptor_no_audio ep_out;
		struct usb_endpoint_descriptor_no_audio ep_in;
	} __attribute__ ((packed)) fs_descs;
} __attribute__ ((packed)) descriptors = {
	.header = {
		.magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
		.flags = cpu_to_le32(FUNCTIONFS_HAS_FS_DESC),
		.length = cpu_to_le32(sizeof(descriptors)),
	},
	.fs_count = cpu_to_le32(5),
	.fs_descs = {
		.intf = {
			.bLength = sizeof(descriptors.fs_descs.intf),
			.bDescriptorType = USB_DT_INTERFACE,
			.bNumEndpoints = 3,
			.bInterfaceClass = 11,
			.iInterface = 1,
		},
		.ccid = {
			.bLength = sizeof(descriptors.fs_descs.ccid),
			.bDescriptorType = 33,
			.bcdCCID = cpu_to_le16(0x0110),
			.bMaxSlotIndex = 7,
			.bVoltageSupport = 0x07, /* 5/3/1.8V */
			.dwProtocols = cpu_to_le32(1), /* T=0 only */
			.dwDefaultClock = cpu_to_le32(5000),
			.dwMaximumClock = cpu_to_le32(20000),
			.bNumClockSupported = 0,
			.dwDataRate = cpu_to_le32(9600),
			.dwMaxDataRate = cpu_to_le32(921600),
			.bNumDataRatesSupported = 0,
			.dwMaxIFSD = cpu_to_le32(0),
			.dwSynchProtocols = cpu_to_le32(0),
			.dwMechanical = cpu_to_le32(0),
			.dwFeatures = cpu_to_le32(0x10),
			.dwMaxCCIDMessageLength = 272,
			.bClassGetResponse = 0xff,
			.bClassEnvelope = 0xff,
			.wLcdLayout = cpu_to_le16(0),
			.bPINSupport = 0,
			.bMaxCCIDBusySlots = 8,
		},
		.ep_irq = {
			.bLength = sizeof(descriptors.fs_descs.ep_irq),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 1 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_INT,
			.wMaxPacketSize = 64,
		},
		.ep_out = {
			.bLength = sizeof(descriptors.fs_descs.ep_out),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 2 | USB_DIR_OUT,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			/* .wMaxPacketSize = autoconfiguration (kernel) */
		},
		.ep_in = {
			.bLength = sizeof(descriptors.fs_descs.ep_in),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 3 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			/* .wMaxPacketSize = autoconfiguration (kernel) */
		},
	},
};

#define STR_INTERFACE_ "Osmocom CCID Interface"

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

#include "ccid_device.h"
#include "ccid_slot_sim.h"

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
	struct osmo_fd ep_in;
	struct osmo_fd ep_out;
	struct osmo_fd ep_int;
	struct llist_head ep_in_queue;
	struct llist_head ep_int_queue;
#ifndef FUNCTIONFS_SUPPORTS_POLL
	struct osmo_fd aio_evfd;
	io_context_t aio_ctx;
	struct aio_help aio_in;
	struct aio_help aio_out;
	struct aio_help aio_int;
#endif
	struct ccid_instance *ccid_handle;
};

static int ep_int_cb(struct osmo_fd *ofd, unsigned int what)
{
	LOGP(DUSB, LOGL_DEBUG, "%s\n", __func__);
	return 0;
}

static int ep_out_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct ufunc_handle *uh = (struct ufunc_handle *) ofd->data;
	struct msgb *msg = msgb_alloc(512, "OUT-Rx");
	int rc;

	LOGP(DUSB, LOGL_DEBUG, "%s\n", __func__);
	if (what & BSC_FD_READ) {
		rc = read(ofd->fd, msgb_data(msg), msgb_tailroom(msg));
		if (rc <= 0) {
			msgb_free(msg);
			return rc;
		}
		msgb_put(msg, rc);
		ccid_handle_out(uh->ccid_handle, msg);
	}
	return 0;
}

static int ep_in_cb(struct osmo_fd *ofd, unsigned int what)
{
	LOGP(DUSB, LOGL_DEBUG, "%s\n", __func__);
	if (what & BSC_FD_WRITE) {
		/* write what we have to write */
	}
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

static void handle_setup(const struct usb_ctrlrequest *setup)
{
	LOGP(DUSB, LOGL_NOTICE, "EP0 SETUP bRequestType=0x%02x, bRequest=0x%02x wValue=0x%04x, "
		"wIndex=0x%04x, wLength=%u\n", setup->bRequestType, setup->bRequest,
		le16_to_cpu(setup->wValue), le16_to_cpu(setup->wIndex), le16_to_cpu(setup->wLength));
	/* FIXME: Handle control transfer */
}

static void aio_refill_out(struct ufunc_handle *uh);

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
			aio_refill_out(uh);
			break;
		case FUNCTIONFS_SETUP:
			handle_setup(&evt.u.setup);
			break;
		}

	}
	return 0;
}

#ifndef FUNCTIONFS_SUPPORTS_POLL

/* an AIO read (OUT) has just completed, let's refill the transfer */
static void aio_refill_out(struct ufunc_handle *uh)
{
	int rc;
	struct aio_help *ah = &uh->aio_out;

	LOGP(DUSB, LOGL_DEBUG, "%s\n", __func__);
	OSMO_ASSERT(!ah->msg);
	ah->msg = msgb_alloc(512, "OUT-Rx-AIO");
	OSMO_ASSERT(ah->msg);
	io_prep_pread(ah->iocb, uh->ep_out.fd, msgb_data(ah->msg), msgb_tailroom(ah->msg), 0);
	io_set_eventfd(ah->iocb, uh->aio_evfd.fd);
	rc = io_submit(uh->aio_ctx, 1, &ah->iocb);
	OSMO_ASSERT(rc >= 0);
}

/* dequeue the next msgb from ep_in_queue and set up AIO for it */
static void dequeue_aio_write_in(struct ufunc_handle *uh)
{
	struct aio_help *ah = &uh->aio_in;
	struct msgb *d;
	int rc;

	if (ah->msg)
		return;

	d = msgb_dequeue(&uh->ep_in_queue);
	if (!d)
		return;

	OSMO_ASSERT(ah->iocb);
	ah->msg = d;
	io_prep_pwrite(ah->iocb, uh->ep_in.fd, msgb_data(d), msgb_length(d), 0);
	io_set_eventfd(ah->iocb, uh->aio_evfd.fd);
	rc = io_submit(uh->aio_ctx, 1, &ah->iocb);
	OSMO_ASSERT(rc >= 0);

}

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
	struct io_event evt[3];
	struct msgb *msg;
	uint64_t ev_cnt;
	int i, rc;

	rc = read(ofd->fd, &ev_cnt, sizeof(ev_cnt));
	assert(rc == sizeof(ev_cnt));

	rc = io_getevents(uh->aio_ctx, 1, 3, evt, NULL);
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
		} else if (fd == uh->ep_in.fd) {
			/* IN endpoint AIO has completed. This means the IN transfer which
			 * we sent to the host has completed */
			LOGP(DUSB, LOGL_DEBUG, "IN AIO completed, free()ing msgb\n");
			msgb_free(uh->aio_in.msg);
			uh->aio_in.msg = NULL;
			dequeue_aio_write_in(uh);
		} else if (fd == uh->ep_out.fd) {
			/* OUT endpoint AIO has completed. This means the host has sent us
			 * some OUT data */
			LOGP(DUSB, LOGL_DEBUG, "OUT AIO completed, dispatching received msg\n");
			msgb_put(uh->aio_out.msg, evt[i].res);
			//printf("\t%s\n", msgb_hexdump(uh->aio_out.msg));
			msg = uh->aio_out.msg;
			uh->aio_out.msg = NULL;
			/* CCID handler takes ownership of msgb */
			ccid_handle_out(uh->ccid_handle, msg);
			aio_refill_out(uh);
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

	rc = open("ep2", O_RDWR);
	assert(rc >= 0);
	osmo_fd_setup(&uh->ep_out, rc, BSC_FD_READ, &ep_out_cb, uh, 2);
#ifdef FUNCTIONFS_SUPPORTS_POLL
	osmo_fd_register(&uh->ep_out);
#endif

	INIT_LLIST_HEAD(&uh->ep_in_queue);
	rc = open("ep3", O_RDWR);
	assert(rc >= 0);
	osmo_fd_setup(&uh->ep_in, rc, 0, &ep_in_cb, uh, 3);
#ifdef FUNCTIONFS_SUPPORTS_POLL
	osmo_fd_register(&uh->ep_in);
#endif

#ifndef FUNCTIONFS_SUPPORTS_POLL
#include <sys/eventfd.h>
	/* for some absolutely weird reason, gadgetfs+functionfs don't support
	 * the standard methods of non-blocking I/o (select/poll).  We need to
	 * work around using Linux AIO, which is not to be confused with POSIX AIO! */

	memset(&uh->aio_ctx, 0, sizeof(uh->aio_ctx));
	rc = io_setup(3, &uh->aio_ctx);
	OSMO_ASSERT(rc >= 0);

	/* create an eventfd, which will be marked readable once some AIO completes */
	rc = eventfd(0, 0);
	OSMO_ASSERT(rc >= 0);
	osmo_fd_setup(&uh->aio_evfd, rc, BSC_FD_READ, &evfd_cb, uh, 0);
	osmo_fd_register(&uh->aio_evfd);

	uh->aio_out.iocb = malloc(sizeof(struct iocb));
	uh->aio_in.iocb = malloc(sizeof(struct iocb));
	uh->aio_int.iocb = malloc(sizeof(struct iocb));
#endif

	return 0;
}

static int ccid_ops_send_in(struct ccid_instance *ci, struct msgb *msg)
{
	struct ufunc_handle *uh = ci->priv;

	/* append to the queue */
	msgb_enqueue(&uh->ep_in_queue, msg);

	/* trigger, if needed */
#ifndef FUNCTIONFS_SUPPORTS_POLL
	dequeue_aio_write_in(uh);
#else
	uh->ep_in.when |= BSC_FD_WRITE;
#endif
	return 0;
}

static int ccid_ops_send_int(struct ccid_instance *ci, struct msgb *msg)
{
	struct ufunc_handle *uh = ci->priv;

	/* append to the queue */
	msgb_enqueue(&uh->ep_int_queue, msg);

	/* trigger, if needed */
#ifndef FUNCTIONFS_SUPPORTS_POLL
	dequeue_aio_write_int(uh);
#else
	uh->ep_int.when |= BSC_FD_WRITE;
#endif
	return 0;
}

static const struct ccid_ops c_ops = {
	.send_in = ccid_ops_send_in,
	.send_int = ccid_ops_send_int,
};

static const struct log_info_cat log_info_cat[] = {
	[DUSB] = {
		.name = "USB",
		.description = "USB Transport",
		.enabled = 1,
		.loglevel = LOGL_NOTICE,
	},
	[DCCID] = {
		.name = "CCID",
		.description = "CCID Core",
		.color = "\033[1;35m",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
};

static const struct log_info log_info = {
	.cat = log_info_cat,
	.num_cat = ARRAY_SIZE(log_info_cat),
};

static void *tall_main_ctx;

static void signal_handler(int signal)
{
	switch (signal) {
	case SIGUSR1:
		talloc_report_full(tall_main_ctx, stderr);
		break;
	}
}


int main(int argc, char **argv)
{
	struct ufunc_handle ufh = (struct ufunc_handle) { 0, };
	struct ccid_instance ci = (struct ccid_instance) { 0, };
	int rc;

	tall_main_ctx = talloc_named_const(NULL, 0, "ccid_main_functionfs");
	msgb_talloc_ctx_init(tall_main_ctx, 0);
	osmo_init_logging2(tall_main_ctx, &log_info);

	signal(SIGUSR1, &signal_handler);

	ccid_instance_init(&ci, &c_ops, &slotsim_slot_ops, "", &ufh);
	ufh.ccid_handle = &ci;

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
