#!/bin/sh
. ./test-data

./ctl_reset_to_dfu.sh
lsusb -d 1d50:6141 -v 2>/dev/null | sed -e "s/$Bus.*Device.*:\ ID/ID/g"
./ctl_reset_target.sh
