/**
 * \file
 *
 * \brief USB Device Stack DFU Function Descriptor Setting.
 *
 * Copyright (c) 2015-2018 Microchip Technology Inc. and its subsidiaries.
 * Copyright (c) 2018 sysmocom -s.f.m.c. GmbH, Author: Kevin Redon <kredon@sysmocom.de>
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 */

#ifndef USBDF_DFU_DESC_H_
#define USBDF_DFU_DESC_H_

#include "usb_protocol.h"
#include "usbd_config.h"
#include "usb_protocol_dfu.h"

#define DFUD_DEV_DESC \
	USB_DEV_DESC_BYTES(CONF_USB_DFUD_BCDUSB, \
	                   CONF_USB_DFUD_BDEVICECLASS, \
	                   CONF_USB_DFUD_BDEVICESUBCLASS, \
	                   CONF_USB_DFUD_BDEVICEPROTOCOL, \
	                   CONF_USB_DFUD_BMAXPKSZ0, \
	                   CONF_USB_OPENMOKO_IDVENDOR, \
	                   CONF_USB_OSMOASF4DFU_IDPRODUCT, \
	                   CONF_USB_DFUD_BCDDEVICE, \
	                   CONF_USB_DFUD_IMANUFACT, \
	                   CONF_USB_DFUD_IPRODUCT, \
	                   CONF_USB_DFUD_ISERIALNUM, \
	                   CONF_USB_DFUD_BNUMCONFIG)

#define DFUD_DEV_QUAL_DESC \
	USB_DEV_QUAL_DESC_BYTES(CONF_USB_DFUD_BCDUSB, \
	                        CONF_USB_DFUD_BDEVICECLASS, \
	                        CONF_USB_DFUD_BDEVICESUBCLASS, \
	                        CONF_USB_DFUD_BMAXPKSZ0, \
	                        CONF_USB_DFUD_BNUMCONFIG)

#define DFUD_CFG_DESC \
	USB_CONFIG_DESC_BYTES(CONF_USB_DFUD_WTOTALLENGTH, \
	                      CONF_USB_DFUD_BNUMINTERFACES, \
	                      CONF_USB_DFUD_BCONFIGVAL, \
	                      CONF_USB_DFUD_ICONFIG, \
	                      CONF_USB_DFUD_BMATTRI, \
	                      CONF_USB_DFUD_BMAXPOWER)

#define DFUD_OTH_SPD_CFG_DESC \
	USB_OTH_SPD_CFG_DESC_BYTES(CONF_USB_DFUD_WTOTALLENGTH, \
	                           CONF_USB_DFUD_BNUMINTERFACES, \
	                           CONF_USB_DFUD_BCONFIGVAL, \
	                           CONF_USB_DFUD_ICONFIG, \
	                           CONF_USB_DFUD_BMATTRI, \
	                           CONF_USB_DFUD_BMAXPOWER)

#define DFUD_IFACE_DESCB USB_DFU_FUNC_DESC_BYTES(USB_DFU_ATTRIBUTES_CAN_DOWNLOAD | USB_DFU_ATTRIBUTES_WILL_DETACH, \
	                     	                     0, /**< detaching makes only sense in run-time mode */ \
	                     	                     512, /**< transfer size corresponds to page size for optimal flash writing */ \
	                     	                     0x0110 /**< DFU specification version 1.1 used */ )

#define DFUD_IFACE_DESCES \
	USB_IFACE_DESC_BYTES(CONF_USB_DFUD_BIFCNUM, \
	                     CONF_USB_DFUD_BALTSET, \
	                     CONF_USB_DFUD_BNUMEP, \
	                     USB_DFU_CLASS, \
	                     USB_DFU_SUBCLASS, \
	                     USB_DFU_PROTOCOL_DFU, \
	                     CONF_USB_DFUD_IINTERFACE), \
	                     DFUD_IFACE_DESCB

#define DFUD_STR_DESCES \
	CONF_USB_DFUD_LANGID_DESC \
	CONF_USB_DFUD_IMANUFACT_STR_DESC \
	CONF_USB_DFUD_IPRODUCT_STR_DESC \
	CONF_USB_DFUD_ISERIALNUM_STR_DESC \
	CONF_USB_DFUD_ICONFIG_STR_DESC \
	CONF_USB_DFUD_IINTERFACE_STR_DESC

/** USB Device descriptors and configuration descriptors */
#define DFUD_DESCES_LS_FS \
	DFUD_DEV_DESC, DFUD_CFG_DESC, DFUD_IFACE_DESCES, DFUD_STR_DESCES

#define DFUD_HS_DESCES_LS_FS \
	DFUD_DEV_DESC, DFUD_DEV_QUAL_DESC, DFUD_CFG_DESC, DFUD_M_IFACE_DESCES, \
	    DFUD_IFACE_DESCES, DFUD_OTH_SPD_CFG_DESC, \
	    DFUD_IFACE_DESCES_HS, DFUD_STR_DESCES

#define DFUD_HS_DESCES_HS \
	DFUD_CFG_DESC, DFUD_IFACE_DESCES, DFUD_IFACE_DESCES_HS, DFUD_OTH_SPD_CFG_DESC, \
	    DFUD_IFACE_DESCES

#endif /* USBDF_DFU_DESC_H_ */
