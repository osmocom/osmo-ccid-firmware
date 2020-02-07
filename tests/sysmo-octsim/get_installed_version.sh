#!/bin/sh
. ./test-data

echo -n "fw version: " 1>&2
lsusb -d 1d50:6141 -v 2>/dev/null | grep iProduct|awk '{}{print $4}' 1>&2
