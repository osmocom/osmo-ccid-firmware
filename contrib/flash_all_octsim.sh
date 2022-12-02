#!/bin/bash

# this script is to scan for octsim boards, sort them by usb path and call dfu-util on each to update the board
# basic flow
#  - get usb path and serialno tupel from dfu-util -l
#  - reformat and sort by usbpath, drop the serno (grep cut tr sort cut)
#  - flash the boards

# TODO:
#  - error handling
#  - filter dfu-util output and generate nice summary

dfu-util -l \
 | grep "^Found" \
 | grep "\[1d50:6141\]" \
 | cut -d ' ' -f 8,13 \
 | tr -d ',' \
 | cut -d \" -f 2,4 \
 | tr \" ' ' \
 | sort \
 | cut -d ' ' -f 1 \
 | xargs -n 1 dfu-util -d 1d50:6141 -D sysmoOCTSIM.bin -R -p 
