#!/bin/sh -e
. ./test-data

dfu-util --device $USBD_TARGET --alt 0 --reset --download $DFU_IMAGE 2>/dev/null |grep -v "Download\t"|grep -v "\["
sleep 1
