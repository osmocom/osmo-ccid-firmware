#!/bin/sh -e
. ./test-data

./get_installed_version.sh
#./fetch_image.sh
./ctl_reset_target.sh
sleep 1
./flash_octsim_dfu.sh
sleep 1
./get_installed_version.sh
