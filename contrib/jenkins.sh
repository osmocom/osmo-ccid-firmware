#!/bin/bash

TOPDIR=`pwd`

set -e

echo
echo "=============== sysmoOCTSIM firmware build ==========="
cd $TOPDIR/sysmoOCTSIM
cd gcc
make clean
make
make clean
