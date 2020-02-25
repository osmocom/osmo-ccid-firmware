#!/bin/sh -e
. ./test-data

#usb_rly08
# relay1 = usbpower
# relay2 = dfu-btn

echo "resetting target to dfu"
#12 off
echo -n "op" > $USB_RELAY
sleep 1
# 2 on
echo -n "f" > $USB_RELAY
sleep 1
# 1 on
echo -n "e" > $USB_RELAY
sleep 1
# 2 off
echo -n "p" > $USB_RELAY
sleep 1
echo "done"
