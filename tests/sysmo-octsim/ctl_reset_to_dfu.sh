#!/bin/sh -e
. ./test-data

#usb_relay
# relay1 = usbpower
# relay2 = dfu-btn

echo "resetting target to dfu"
#12 off
usbrelay BITFT_1=0 BITFT_2=0 2>/dev/null
sleep 1
# 2 on
usbrelay BITFT_1=0 BITFT_2=1 2>/dev/null
sleep 1
# 1 on
usbrelay BITFT_1=1 BITFT_2=1 2>/dev/null
sleep 1
# 2 off
usbrelay BITFT_1=1 BITFT_2=0 2>/dev/null
sleep 1
echo "done"
