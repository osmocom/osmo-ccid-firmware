#!/bin/bash

GADGET_NAME=osmo-ccid



GADGET_CONFIGFS=/sys/kernel/config/usb_gadget

die() {
	echo ERROR: $1
	exit 2
}

modprobe configfs
modprobe usb_f_fs
modprobe dummy_hcd is_high_speed=0 is_super_speed=0

[ -d $GADGET_CONFIGFS ] || die "usb_gadget configfs not mounted"

gadgetdir="$GADGET_CONFIGFS/$GADGET_NAME"

# create gadget
[ -d $gadgetdir ] || mkdir $gadgetdir || die "Cannot create $gadgetdir. Permission problem?"
set -e -x
cd $gadgetdir
echo 0x1d50 > idVendor
echo 0x615f > idProduct
[ -d strings/0x409 ] || mkdir strings/0x409
echo 2342 > strings/0x409/serialnumber
echo "sysmocom GmbH" > strings/0x409/manufacturer
echo "sysmoOCTSIM" > strings/0x409/product

# create config
[ -d configs/c.1 ] || mkdir configs/c.1
[ -d configs/c.1/strings/0x409 ] || mkdir configs/c.1/strings/0x409
echo "sysmoOCTSIM config" > configs/c.1/strings/0x409/configuration

[ -d functions/ffs.usb0 ] || mkdir functions/ffs.usb0
[ -e configs/c.1/ffs.usb0 ] || ln -s functions/ffs.usb0 configs/c.1

[ -d /dev/ffs-ccid ] || mkdir /dev/ffs-ccid
[ -e /dev/ffs-ccid/ep0 ] || mount -t functionfs usb0 /dev/ffs-ccid/

# enable device, only works after program has opened EP FDs
#echo dummy_udc.0 > UDC

