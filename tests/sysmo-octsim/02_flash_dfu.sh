#!/bin/sh
. ./test-data

./get_installed_version.sh
./fetch_image.sh
./ctl_reset_to_dfu.sh
./flash_octsim_dfu.sh
sleep 1
./get_installed_version.sh
