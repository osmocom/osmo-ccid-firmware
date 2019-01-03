
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
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <osmocom/core/select.h>
#include <osmocom/core/utils.h>

#ifndef FUNCTIONFS_SUPPORTS_POLL
#include <libaio.h>
struct aio_help {
	uint8_t buf[64];
	struct iocb *iocb;
};
#endif

/* usb function handle */
struct ufunc_handle {
	struct osmo_fd ep0;
	struct osmo_fd ep_in;
	struct osmo_fd ep_out;
	struct osmo_fd ep_int;
#ifndef FUNCTIONFS_SUPPORTS_POLL
	struct osmo_fd aio_evfd;
	io_context_t aio_ctx;
	struct aio_help aio_in;
	struct aio_help aio_out;
	struct aio_help aio_int;
#endif
};

static int ep_int_cb(struct osmo_fd *ofd, unsigned int what)
{
	printf("INT\n");
	return 0;
}

static int ep_out_cb(struct osmo_fd *ofd, unsigned int what)
{
	uint8_t buf[64];
	int rc;

	printf("OUT\n");
	if (what & BSC_FD_READ) {
		rc = read(ofd->fd, buf, sizeof(buf));
		ccid_handle_out(uh->ccid_handle, buf, rc);
	}
	return 0;
}

static int ep_in_cb(struct osmo_fd *ofd, unsigned int what)
{
	printf("IN\n");
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
	printf("bRequestType = %d\n", setup->bRequestType);
	printf("bRequest     = %d\n", setup->bRequest);
	printf("wValue       = %d\n", le16_to_cpu(setup->wValue));
	printf("wIndex       = %d\n", le16_to_cpu(setup->wIndex));
	printf("wLength      = %d\n", le16_to_cpu(setup->wLength));
}

static void aio_refill_out(struct ufunc_handle *uh);

static int ep_0_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct ufunc_handle *uh = (struct ufunc_handle *) ofd->data;
	int rc;

	printf("EP0\n");

	if (what & BSC_FD_READ) {
		struct usb_functionfs_event evt;
		rc = read(ofd->fd, (uint8_t *)&evt, sizeof(evt));
		if (rc < sizeof(evt))
			return -23;
		printf("\t%s\n", get_value_string(ffs_evt_type_names, evt.type));
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

static void aio_refill_out(struct ufunc_handle *uh)
{
	int rc;
	struct aio_help *ah = &uh->aio_out;
	io_prep_pread(ah->iocb, uh->ep_out.fd, ah->buf, sizeof(ah->buf), 0);
	io_set_eventfd(ah->iocb, uh->aio_evfd.fd);
	rc = io_submit(uh->aio_ctx, 1, &ah->iocb);
	OSMO_ASSERT(rc >= 0);
}

static int evfd_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct ufunc_handle *uh = (struct ufunc_handle *) ofd->data;
	struct io_event evt[3];
	uint64_t ev_cnt;
	int i, rc;

	rc = read(ofd->fd, &ev_cnt, sizeof(ev_cnt));
	assert(rc == sizeof(ev_cnt));

	rc = io_getevents(uh->aio_ctx, 1, 3, evt, NULL);
	if (rc <= 0)
		return rc;

	for (i = 0; i < rc; i++) {
		int fd = evt[i].obj->aio_fildes;
		if (fd == uh->ep_int.fd) {
			/* interrupt endpoint AIO has completed. This means the IRQ transfer
			 * which we generated has reached the host */
		} else if (fd == uh->ep_in.fd) {
			/* IN endpoint AIO has completed. This means the IN transfer which
			 * we sent to the host has completed */
		} else if (fd == uh->ep_out.fd) {
			/* IN endpoint AIO has completed. This means the host has sent us
			 * some OUT data */
			//printf("\t%s\n", osmo_hexdump(uh->aio_out.buf, evt[i].res));
			ccid_handle_out(uh->ccid_handle, uh->aio_out.buf, evt[i].res);
			aio_refill_out(uh);
		}
	}
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
	if (rc != sizeof(descriptors))
		return -1;
	rc = write(uh->ep0.fd, &strings, sizeof(strings));
	if (rc != sizeof(strings))
		return -1;

	/* open other endpoint file descriptors */
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

#endif

	return 0;
}


int main(int argc, char **argv)
{
	struct ufunc_handle ufh = (struct ufunc_handle) { 0, };
	int rc;

	chdir(argv[1]);
	rc = ep0_init(&ufh);
	if (rc < 0) {
		fprintf(stderr, "Error %d\n", rc);
		exit(2);
	}

	while (1) {
		osmo_select_main(0);
	}
}
