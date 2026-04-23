#!/bin/sh -e
. ./test-data

./ctl_reset_target.sh
sleep 1
./get_installed_version.sh

echo "card slot 0 - pysimshell (read)"
$PYSIMSHELL -p 0 --noprompt --script 06_test_simcards_toaster.script
echo ""
echo "card slot 4 - pysimshell (read)"
$PYSIMSHELL -p 4 --noprompt --script 06_test_simcards_toaster.script
echo ""
echo "card slot 0 - pysimshell (export)"
$PYSIMSHELL -p 0 -e "export" --noprompt
echo ""
echo "card slot 4 - pysimshell (export)"
$PYSIMSHELL -p 4 -e "export" --noprompt

#pySim-shell.py -p 0 -e "verify_adm" -e "export" --noprompt --csv card_data.csv

