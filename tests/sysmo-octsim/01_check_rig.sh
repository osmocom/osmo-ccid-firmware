#!/bin/sh -e
. ./test-data

lsusb -d $USBD_RELAY -v 2>/dev/null | sed -e "s/$Bus.*Device.*:\ ID/ID/g"
