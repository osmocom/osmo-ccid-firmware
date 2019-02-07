The sysmocom sysmoOCTSIM is a USB CCID device with eight smart card slots.
This is the open source firmware for this device.

Hardware
========

The sysmocom sysmoOCTSIM hardware is proprietary.

It has the following specifications:

- 8 smart cards slots for 2FF cards (e.g. normal SIM format)
- each slot is driven individually (e.g. they are not multiplexed)
- each card can be operated in class A (5.0 V), B (3.0 V), or C (1.8 V)
- the ISO/IEC 7816-4 T=0 communication protocol is used
- each card can be clock up to 20 MHz (e.g. maximum allowed by the standard)

For more detailed specification refer to the USB description of the device.

Firmware
========

Compiling
---------

GNU make and the arm-none-eabi-gcc compiler are required to build the firmware.

The compile the firmware, run the following commands:

```
cd sysmoOCTSIM/gcc
make
```

The `sysmoOCTSIM/gcc/AtmelStart.bin` file is the resulting firmware for the device.

Flashing
--------

The device comes pre-flash with a USB DFU bootloader (e.g. [osmo-asf4-dfu](https://git.osmocom.org/osmo-asf4-dfu/)).
To start the USB DFU bootloader, press on the button on the front right of the panel while powering up the device.

Use the [dfu-util](http://dfu-util.sourceforge.net/) utility to flash the firmware onto the device:

```
dfu-util --device 1d50:6141 --download sysmoOCTSIM/gcc/AtmelStart.bin
```
