#!/bin/sh

#usb_rly08 serial
USB_RELAY="/dev/serial/by-id/usb-Devantech_Ltd._USB-RLY08_00021197-if00"

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
