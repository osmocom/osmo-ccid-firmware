#!/bin/sh -e
. ./test-data

./ctl_reset_target.sh
sleep 1
./get_installed_version.sh

echo "card slot 0 - pysimread"
$PYSIMREAD -p 0
echo ""
echo "card slot 4 - pysimread"
$PYSIMREAD -p 4
echo ""
echo "card slot 0 - pysimshell"
$PYSIMSHELL -p 0 -e "export" --noprompt
echo ""
echo "card slot 4 - pysimshell"
$PYSIMSHELL -p 4 -e "export" --noprompt

#pySim-shell.py -p 0 -e "verify_adm" -e "export" --noprompt --csv card_data.csv

