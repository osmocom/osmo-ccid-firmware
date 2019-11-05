#include "usbd_config.h"
#include "usbdc.h"
#include "usb_protocol.h"
#include "usb_protocol_cdc.h"
#include "ccid_proto.h"
#include "cdcdf_acm_desc.h"
#include "usb_descriptors.h"



const struct usb_desc_collection usb_fs_descs = {
	.dev = {
		.bLength = sizeof(struct usb_dev_desc),
		.bDescriptorType = USB_DT_DEVICE,
		.bcdUSB = USB_V2_0,
		.bDeviceClass = 0x02,
		.bDeviceSubClass = 0,
		.bDeviceProtocol = 0,
		.bMaxPacketSize0 = CONF_USB_CDCD_ACM_BMAXPKSZ0,
		.idVendor = CONF_USB_CDCD_ACM_IDVENDER,
		.idProduct = CONF_USB_CDCD_ACM_IDPRODUCT,
		.iManufacturer = STR_DESC_MANUF,
		.iProduct = STR_DESC_PRODUCT,
		.iSerialNumber = STR_DESC_SERIAL,
		.bNumConfigurations = 1,
	},
	.cfg = {
		.bLength = sizeof(struct usb_config_desc),
		.bDescriptorType = USB_DT_CONFIG,
		.wTotalLength = sizeof(usb_fs_descs.cfg) +
				sizeof(usb_fs_descs.cdc) +
				sizeof(usb_fs_descs.ccid),
		.bNumInterfaces = 3,
		.bConfigurationValue = CONF_USB_CDCD_ACM_BCONFIGVAL,
		.iConfiguration = STR_DESC_CONFIG,
		.bmAttributes = CONF_USB_CDCD_ACM_BMATTRI,
		.bMaxPower = CONF_USB_CDCD_ACM_BMAXPOWER,
	},
	.cdc = {
		.comm = {
			.iface = {
				.bLength = sizeof(struct usb_iface_desc),
				.bDescriptorType = USB_DT_INTERFACE,
				.bInterfaceNumber = CONF_USB_CDCD_ACM_COMM_BIFCNUM,
				.bAlternateSetting = CONF_USB_CDCD_ACM_COMM_BALTSET,
				.bNumEndpoints = 1,
				.bInterfaceClass = CDC_CLASS_COMM,
				.bInterfaceSubClass = CDC_SUBCLASS_ACM,
				.bInterfaceProtocol = 0x00,
				.iInterface = STR_DESC_INTF_ACM_COMM,
			},
			.cdc_hdr = {
				.bFunctionLength = sizeof(struct usb_cdc_hdr_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_HEADER,
				.bcdCDC = LE16(0x1001),
			},
			.cdc_call_mgmt = {
				.bFunctionLength = sizeof(struct usb_cdc_call_mgmt_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_CALL_MGMT,
				.bmCapabilities = 0x01,
				.bDataInterface = 0x00,
			},
			.cdc_acm = {
				.bFunctionLength = sizeof(struct usb_cdc_acm_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_ACM,
				.bmCapabilities = 0x02,
			},
			.cdc_union = {
				.bFunctionLength = sizeof(struct usb_cdc_union_desc),
				.bDescriptorType = CDC_CS_INTERFACE,
				.bDescriptorSubtype = CDC_SCS_UNION,
				.bMasterInterface = CONF_USB_CDCD_ACM_COMM_BIFCNUM,
				.bSlaveInterface0 = CONF_USB_CDCD_ACM_DATA_BIFCNUM,
			},
			.ep = {
				{
					.bLength = sizeof(struct usb_ep_desc),
					.bDescriptorType = USB_DT_ENDPOINT,
					.bEndpointAddress = CONF_USB_CDCD_ACM_COMM_INT_EPADDR,
					.bmAttributes = USB_EP_TYPE_INTERRUPT,
					.wMaxPacketSize = CONF_USB_CDCD_ACM_COMM_INT_MAXPKSZ,
					.bInterval = CONF_USB_CDCD_ACM_COMM_INT_INTERVAL,
				},
			},
		},
		.data = {
			.iface = {
				.bLength = sizeof(struct usb_iface_desc),
				.bDescriptorType = USB_DT_INTERFACE,
				.bInterfaceNumber = CONF_USB_CDCD_ACM_DATA_BIFCNUM,
				.bAlternateSetting = CONF_USB_CDCD_ACM_DATA_BALTSET,
				.bNumEndpoints = 2,
				.bInterfaceClass = CDC_CLASS_DATA,
				.bInterfaceSubClass = 0x00,
				.bInterfaceProtocol = 0x00,
				.iInterface = STR_DESC_INTF_ACM_DATA,
			},
			.ep = {
				{
					.bLength = sizeof(struct usb_ep_desc),
					.bDescriptorType = USB_DT_ENDPOINT,
					.bEndpointAddress = CONF_USB_CDCD_ACM_DATA_BULKOUT_EPADDR,
					.bmAttributes = USB_EP_TYPE_BULK,
					.wMaxPacketSize = CONF_USB_CDCD_ACM_DATA_BULKOUT_MAXPKSZ,
					.bInterval = 0,
				},
				{
					.bLength = sizeof(struct usb_ep_desc),
					.bDescriptorType = USB_DT_ENDPOINT,
					.bEndpointAddress = CONF_USB_CDCD_ACM_DATA_BULKIN_EPADDR,
					.bmAttributes = USB_EP_TYPE_BULK,
					.wMaxPacketSize = CONF_USB_CDCD_ACM_DATA_BULKIN_MAXPKSZ,
					.bInterval = 0,
				},
			},
		},
	},
	.ccid = {
		.iface = {
			.bLength = sizeof(struct usb_iface_desc),
			.bDescriptorType = USB_DT_INTERFACE,
			.bInterfaceNumber = 0,
			.bAlternateSetting = 0,
			.bNumEndpoints = 3,
			.bInterfaceClass = 11,
			.bInterfaceSubClass = 0,
			.bInterfaceProtocol = 0,
			.iInterface = STR_DESC_INTF_CCID,
		},
		.class = {
			.bLength = sizeof(struct usb_ccid_class_descriptor),
			.bDescriptorType = 33,
			.bcdCCID = LE16(0x0110),
			.bMaxSlotIndex = 7,
			.bVoltageSupport = 0x07, /* 5/3/1.8V */
			.dwProtocols = 0x01,
			.dwDefaultClock = LE32(2500),
			.dwMaximumClock = LE32(20000),
			.bNumClockSupported = CCID_NUM_CLK_SUPPORTED,
			.dwDataRate = LE32(6720), // default clock 2.5M/372
			.dwMaxDataRate = LE32(921600),
			.bNumDataRatesSupported = 0,
			.dwMaxIFSD = LE32(0),
			.dwSynchProtocols = LE32(0),
			.dwMechanical = LE32(0),
			.dwFeatures = LE32(0x10 | 0x20 | 0x80 | 0x00010000),
//			.dwFeatures = LE32(0x2 | 0x40),
			.dwMaxCCIDMessageLength = 272,
			.bClassGetResponse = 0xff,
			.bClassEnvelope = 0xff,
			.wLcdLayout = LE16(0),
			.bPINSupport = 0,
			.bMaxCCIDBusySlots = 8,
		},
		.ep = {
			{	/* Bulk-OUT descriptor */
				.bLength = sizeof(struct usb_ep_desc),
				.bDescriptorType = USB_DT_ENDPOINT,
				.bEndpointAddress = 0x02,
				.bmAttributes = USB_EP_TYPE_BULK,
				.wMaxPacketSize = 64,
				.bInterval = 0,
			},
			{ 	/* Bulk-IN descriptor */
				.bLength = sizeof(struct usb_ep_desc),
				.bDescriptorType = USB_DT_ENDPOINT,
				.bEndpointAddress = 0x83,
				.bmAttributes = USB_EP_TYPE_BULK,
				.wMaxPacketSize = 64,
				.bInterval = 0,
			},
			{	/* Interrupt dscriptor */
				.bLength = sizeof(struct usb_ep_desc),
				.bDescriptorType = USB_DT_ENDPOINT,
				.bEndpointAddress = 0x84,
				.bmAttributes = USB_EP_TYPE_INTERRUPT,
				.wMaxPacketSize = 64,
				.bInterval = 0x10,
			},
		},
	},
	//DFURT_IF_DESCRIPTOR,
	.str = {
#if 0
		CDCD_ACM_STR_DESCES
#else
		 4,  3, 0x09, 0x04,
		18,  3, 's',0, 'y',0, 's',0, 'm',0, 'o',0, 'c',0, 'o',0, 'm',0,
		24,  3, 's',0, 'y',0, 's',0, 'm',0, 'o',0, 'O',0, 'C',0, 'T',0, 'S',0, 'I',0, 'M',0,
		 4,  3, 'A', 0,
		22,  3, 'd',0, 'e',0, 'b',0, 'u',0, 'g',0, ' ',0, 'U',0, 'A',0, 'R',0, 'T',0,
		22,  3, 'd',0, 'e',0, 'b',0, 'u',0, 'g',0, ' ',0, 'U',0, 'A',0, 'R',0, 'T',0,
		10,  3, 'C',0, 'C',0, 'I',0, 'D',0,
		12,  3, 'F',0, 'I',0, 'X',0, 'M',0, 'E',0,
#endif
	}
};

const struct usbd_descriptors usb_descs[]
    = {{ (uint8_t *)&usb_fs_descs, (uint8_t *)&usb_fs_descs + sizeof(usb_fs_descs) }};
