#!/bin/sh -e
. ./test-data

echo -n "installed fw version: " 1>&2
lsusb -d $USBD_TARGET -v 2>/dev/null | grep iProduct|awk '{}{print $4}' 1>&2
