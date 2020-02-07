#!/bin/sh
. ./test-data

lsusb -d 1d50:6141 -v 2>/dev/null | sed -e "s/$Bus.*Device.*:\ ID/ID/g"|sed -e "s/^.*iProduct.*2.*sysmoOCTSIM.*$/  iProduct                2 sysmoOCTSIM/g"
