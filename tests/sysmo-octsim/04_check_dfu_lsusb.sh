#!/bin/sh -e
. ./test-data

./ctl_reset_to_dfu.sh
lsusb -d $USBD_TARGET -v 2>/dev/null | sed -e "s/$Bus.*Device.*:\ ID/ID/g"
./ctl_reset_target.sh
