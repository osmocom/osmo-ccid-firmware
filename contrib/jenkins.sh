#!/bin/bash
exit 0
if ! [ -x "$(command -v osmo-deps.sh)" ]; then
	echo "Error: We need to have scripts/osmo-deps.sh from http://git.osmocom.org/osmo-ci/ in PATH !"
	exit 2
fi

set -ex

TOPDIR=`pwd`

deps="$TOPDIR/deps"
inst="$TOPDIR/install"
export deps inst


echo
echo "=============== libosmocore hostt build ==========="
osmo-build-dep.sh libosmocore "" --disable-doxygen
export PKG_CONFIG_PATH="$inst/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$inst/lib"
export PATH="$inst/bin:$PATH"

echo
echo "=============== CCID usb_gadget build ==========="
cd $TOPDIR/ccid_host
make clean
make $PARALLEL_MAKE
make clean

# adapted from
echo
echo "=============== libosmocore cross-build ==========="
mkdir -p "$deps"
cd "$deps"
osmo-deps.sh libosmocore master
cd libosmocore

mkdir -p "$inst/stow"
autoreconf --install --force
./configure	--enable-static --prefix="$inst/stow/libosmocore" --host=arm-none-eabi \
		--enable-embedded --disable-doxygen --disable-shared --disable-pseudotalloc \
		--disable-libsctp --enable-external-tests \
		CFLAGS="-Os -ffunction-sections -fdata-sections -nostartfiles -nodefaultlibs -Werror -Wno-error=deprecated -Wno-error=deprecated-declarations -Wno-error=cpp -mthumb -Os -mlong-calls -g3 -mcpu=cortex-m4 -mfloat-abi=softfp -mfpu=fpv4-sp-d16 -I /home/laforge/projects/git/osmo-ccid-firmware/sysmoOCTSIM -Wno-error=format"
make $PARALLEL_MAKE install
make clean
STOW_DIR="$inst/stow" stow --restow libosmocore

export PKG_CONFIG_PATH="$inst/lib/pkgconfig"
export LD_LIBRARY_PATH="$inst/lib"

echo
echo "=============== sysmoOCTSIM firmware build ==========="
cd $TOPDIR/sysmoOCTSIM
cd gcc
make clean
make SYSTEM_PREFIX="$inst" $PARALLEL_MAKE
make clean
