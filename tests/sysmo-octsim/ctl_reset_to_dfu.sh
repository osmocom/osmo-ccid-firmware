#!/bin/sh

#usb_rly08 serial
USB_RELAY="/dev/serial/by-id/usb-Devantech_Ltd._USB-RLY08_00021197-if00"

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
