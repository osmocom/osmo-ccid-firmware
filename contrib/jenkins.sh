#!/bin/bash

if ! [ -x "$(command -v osmo-deps.sh)" ]; then
	echo "Error: We need to have scripts/osmo-deps.sh from http://git.osmocom.org/osmo-ci/ in PATH !"
	exit 2
fi

set -ex

TOPDIR=`pwd`

publish="$1"
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
verify_value_string_arrays_are_terminated.py $(find . -name "*.[hc]")
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
		--enable-embedded --disable-log-macros --disable-doxygen --disable-shared --disable-pseudotalloc \
		--disable-libsctp --disable-libusb --disable-gb --enable-external-tests \
		--disable-libmnl \
		CFLAGS="-Os -ffunction-sections -fdata-sections -nostartfiles -nodefaultlibs -Werror -Wno-error=deprecated -Wno-error=deprecated-declarations -Wno-error=cpp -mthumb -Os -mlong-calls -g3 -mcpu=cortex-m4 -mfloat-abi=softfp -mfpu=fpv4-sp-d16 -I /home/laforge/projects/git/osmo-ccid-firmware/sysmoOCTSIM -Wno-error=format" \
		CPPFLAGS="-D__thread=''"
make $PARALLEL_MAKE install
make clean
STOW_DIR="$inst/stow" stow --restow libosmocore

export PKG_CONFIG_PATH="$inst/lib/pkgconfig"
export LD_LIBRARY_PATH="$inst/lib"

echo
echo "=============== sysmoOCTSIM firmware build ==========="
cd $TOPDIR/sysmoOCTSIM
cd gcc
make mrproper
verify_value_string_arrays_are_terminated.py $(find . -name "*.[hc]")
make SYSTEM_PREFIX="$inst" $PARALLEL_MAKE

if [ "x$publish" = "x--publish" ]; then
	echo
	echo "=============== UPLOAD BUILD  =============="

	cat > "/build/known_hosts" <<EOF
[ftp.osmocom.org]:48 ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDDgQ9HntlpWNmh953a2Gc8NysKE4orOatVT1wQkyzhARnfYUerRuwyNr1GqMyBKdSI9amYVBXJIOUFcpV81niA7zQRUs66bpIMkE9/rHxBd81SkorEPOIS84W4vm3SZtuNqa+fADcqe88Hcb0ZdTzjKILuwi19gzrQyME2knHY71EOETe9Yow5RD2hTIpB5ecNxI0LUKDq+Ii8HfBvndPBIr0BWYDugckQ3Bocf+yn/tn2/GZieFEyFpBGF/MnLbAAfUKIdeyFRX7ufaiWWz5yKAfEhtziqdAGZaXNaLG6gkpy3EixOAy6ZXuTAk3b3Y0FUmDjhOHllbPmTOcKMry9
[ftp.osmocom.org]:48 ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBPdWn1kEousXuKsZ+qJEZTt/NSeASxCrUfNDW3LWtH+d8Ust7ZuKp/vuyG+5pe5pwpPOgFu7TjN+0lVjYJVXH54=
[ftp.osmocom.org]:48 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIK8iivY70EiR5NiGChV39gRLjNpC8lvu1ZdHtdMw2zuX
EOF
	SSH_COMMAND="ssh -o 'UserKnownHostsFile=/build/known_hosts' -p 48"
	rsync --archive --copy-links --verbose --compress --delete --rsh "$SSH_COMMAND" \
		$TOPDIR/sysmoOCTSIM/gcc/sysmoOCTSIM.{bin,elf} \
			binaries@ftp.osmocom.org:web-files/osmo-ccid-firmware/latest/
	rsync --archive --verbose --compress --rsh "$SSH_COMMAND" \
		--exclude $TOPDIR/sysmoOCTSIM/gcc/sysmoOCTSIM.bin \
		--exclude $TOPDIR/sysmoOCTSIM/gcc/sysmoOCTSIM.elf \
		$TOPDIR/sysmoOCTSIM/gcc/*-*.{bin,elf} \
			binaries@ftp.osmocom.org:web-files/osmo-ccid-firmware/all/
fi

echo
echo "=============== FIRMWARE CLEAN  =============="
make clean
