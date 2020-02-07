#!/bin/sh

dfu-util --device 1d50:6141 --alt 0 --reset --download dl/sysmoOCTSIM-latest.bin 2>/dev/null |grep -v "Download\t"|grep -v "\["
