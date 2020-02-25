#!/bin/sh -e
. ./test-data

./ctl_reset_target.sh
sleep 1
./get_installed_version.sh

echo "card slot 0"
$PYSIMREAD -p 0
echo ""
echo "card slot 4"
$PYSIMREAD -p 4

