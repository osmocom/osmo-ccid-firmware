#!/bin/sh
. ./test-data

lsusb -d 04d8:ffee -v 2>/dev/null | sed -e "s/$Bus.*Device.*:\ ID/ID/g"
