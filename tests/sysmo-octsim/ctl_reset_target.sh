#!/bin/sh -e
. ./test-data

#usb_rly08
# relay1 = usbpower
# relay2 = dfu-btn

#12 off
echo "resetting target"
echo -n "op" > $USB_RELAY
sleep 1
# 1 on
echo -n "e" > $USB_RELAY
sleep 1
echo "done"
