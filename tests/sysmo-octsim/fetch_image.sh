#!/bin/sh -e
. ./test-data

echo "fetch image"

if [ "$SKIP_FETCH_IMAGE" = 1 ]; then
	echo "skipping fetch image (SKIP_FETCH_IMAGE=1)" >&2
else
	wget -O $DFU_IMAGE https://ftp.osmocom.org/binaries/osmo-ccid-firmware/latest/sysmoOCTSIM.bin -nv
fi

echo "done"
