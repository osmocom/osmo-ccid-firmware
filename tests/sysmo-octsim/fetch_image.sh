#!/bin/sh -e
. ./test-data

echo "fetch image"
wget -O $DFU_IMAGE https://downloads.osmocom.org/binaries/osmo-ccid-firmware/all/sysmoOCTSIM-0.3.44-639d.bin -nv
echo "done"
