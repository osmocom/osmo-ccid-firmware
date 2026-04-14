#!/usr/bin/env python3
#
# Copyright (C) 2026 sysmocom -s.f.m.c. GmbH, Author: Alexander Couzens <lynxis@fe80.eu>
# License: MIT

import sys
import usb.util
from usb.core import USBError

USB_VENDOR_REQ_GET  = 0
USB_VENDOR_REQ_SET  = 1
USB_VENDOR_IRQ      = 2
USB_VENDOR_READ_MEM = 3

GET_SET_DEBUG = 0
REQ_BUS_ERROR = 1

TIMEOUT = 3

# only implement by a wip branch
def vendor_read_mem(device: usb.core.Device, addr: int):
    if addr >> 32:
        raise RuntimeError("Address is outside of 32 bit")

    high = addr >> 16
    low = addr & 0xffff
    return vendor_req_in(device, USB_VENDOR_READ_MEM, wValue = high, wIndex = low)

def vendor_get_debug(device: usb.core.Device):
    """ Set debug / break on panic """
    return vendor_req_in(device, USB_VENDOR_REQ_GET, wValue = 0, wIndex = GET_SET_DEBUG)

def vendor_set_debug(device: usb.core.Device, debug: int):
    return vendor_req_in(octsim, USB_VENDOR_REQ_SET, wValue = debug, wIndex = GET_SET_DEBUG)

def vendor_req_in(device: usb.core.Device, bRequest: int, wValue: int, wIndex: int, length: int = 8):
    """ do a vendor request in (device -> host)
    @param bRequest: usb single byte
    @param wIndex: short index
    """
    return device.ctrl_transfer(
        bmRequestType=usb.util.ENDPOINT_IN
                      | usb.util.CTRL_TYPE_VENDOR
                      | usb.util.CTRL_RECIPIENT_DEVICE,
        bRequest=bRequest,
        wValue=wValue,
        wIndex=wIndex,
        data_or_wLength=length,
        timeout=TIMEOUT,
    )

def find_octsim():
    return usb.core.find(idVendor=0x1d50, idProduct=0x6141)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(prog='octsim-util')
    parser.add_argument(
        "--get-debug",
        action='store_true',
        help="Get debug")
    parser.add_argument(
        "--enable-debug",
        action='store_const',
        dest='set_debug',
        const=1,
        help="Enable debug")
    parser.add_argument(
        "--disable-debug",
        action='store_const',
        dest='set_debug',
        const=0,
        help="Disable debug")

    args = parser.parse_args()
    # no action given
    if not args.get_debug and args.set_debug is None:
        parser.print_usage()
        sys.exit(1)

    print(args)
    sys.exit(1)

    device = find_octsim()
    if not device:
        print("Could not find the octsim board", file=sys.stderr)
        sys.exit(1)

    if args.set_debug is not None:
        result = vendor_set_debug(device, args.set_debug)
