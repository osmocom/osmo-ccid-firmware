#!/bin/bash

# this script is to scan for octsim boards, sort them by usb path and generate a bankd_pcsc_slots.csv on stdout
# basic flow
#  - get usb path and serialno tupel from dfu-util -l
#  - reformat and sort by usbpath, drop the usbpath (grep cut tr sort cut)
#  - generate the config file from the list of serialno in the format required by osmo-remsim-bankd (awk)

dfu-util -l \
 | grep "Found Runtime: \[1d50:6141\]" \
 | cut -d ' ' -f 8,13 \
 | tr -d ',' \
 | cut -d \" -f 2,4 \
 | tr \" ' ' \
 | sort \
 | cut -d ' ' -f 2 \
 | awk 'BEGIN {count=0}{for (slot=0; slot<8; slot++) {print count, $0, 0slot; count++}}' \
 | awk '{print "\"1\",\""$1"\",\"sysmocom sysmoOCTSIM \[CCID\] \("$2"\) [A-F0-9]{2} "$3"\""}'

