#!/bin/sh
. ./test-data

echo "fetch image"
wget -O dl/sysmoOCTSIM-latest.bin http://ftp.osmocom.org/binaries/osmo-ccid-firmware/latest/sysmoOCTSIM.bin -nv
echo "done"
