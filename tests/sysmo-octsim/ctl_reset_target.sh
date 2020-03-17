#!/bin/sh -e
. ./test-data

#usb_relay
# relay1 = usbpower
# relay2 = dfu-btn

#12 off
echo "resetting target"
usbrelay BITFT_1=0 BITFT_2=0 2>/dev/null
sleep 1
# 1 on
usbrelay BITFT_1=1 BITFT_2=0 2>/dev/null
sleep 1
echo "done"
