#!/bin/sh -e
. ./test-data

./ctl_reset_target.sh
sleep 1
./get_installed_version.sh

# OS#7022: restart pcscd right before using it through pySim-read.py, as it may
# have crashed during flashing
sudo /etc/osmo-ccid-firmware-tests/restart-pcscd.sh
sleep 5

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

