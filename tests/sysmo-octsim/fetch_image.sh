#!/bin/sh -e
. ./test-data

echo "fetch image"
wget -O $DFU_IMAGE http://ftp.osmocom.org/binaries/osmo-ccid-firmware/latest/sysmoOCTSIM.bin -nv
echo "done"
