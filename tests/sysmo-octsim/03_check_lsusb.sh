#!/bin/sh -e
. ./test-data

lsusb -d $USBD_TARGET -v 2>/dev/null | sed -e "s/$Bus.*Device.*:\ ID/ID/g"|sed -e "s/^.*iProduct.*2.*sysmoOCTSIM.*$/  iProduct                2 sysmoOCTSIM/g"
