#!/usr/bin/env python3
"""
Copyright 2025 sysmocom - s.f.m.c. GmbH <info@sysmocom.de>

SPDX-License-Identifier: 0BSD

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted.THE SOFTWARE IS PROVIDED "AS IS" AND THE
AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
USE OR PERFORMANCE OF THIS SOFTWARE.

"""

"""
ch9 9.9 usb ep halt test code snippets
"""

import sys
import getopt
import ctypes
import libusb

VENDOR = 0x1d50
PRODUCT = 0x6141
TIMEOUT = 1000  # milliseconds

def is_myusb(dev):
    desc = libusb.device_descriptor()
    ret = libusb.get_device_descriptor(dev, ctypes.byref(desc))
    if ret != 0:
        sys.stderr.write("Failed to get device descriptor\n")
        return False
    return (desc.idVendor == VENDOR and desc.idProduct == PRODUCT)


def print_device_endpoints(dev):
    """Prints the list of endpoints for each interface from the first configuration."""
    enum_mapping = {
        value: name
        for name, value in vars(libusb).items()
        if name.startswith("LIBUSB_ENDPOINT_TRANSFER_TYPE_") and isinstance(value, int)
    }

    def _dir(ep):
        return "IN" if (ep & libusb.LIBUSB_ENDPOINT_DIR_MASK) == libusb.LIBUSB_ENDPOINT_DIR_MASK else "OUT"

    cfg_ptr = ctypes.POINTER(libusb.config_descriptor)()
    ret = libusb.get_config_descriptor(dev, 0, ctypes.byref(cfg_ptr))
    if ret != 0:
        sys.stderr.write("Error getting config descriptor: error {}\n".format(ret))
        return
    cfg = cfg_ptr.contents
    print("Device Configuration:")
    print("  Number of interfaces: {}".format(cfg.bNumInterfaces))
    # Iterate over interfaces
    for i in range(cfg.bNumInterfaces):
        iface = cfg.interface[i]
        print(" Interface {}: {} alternate setting(s)".format(i, iface.num_altsetting))
        for j in range(iface.num_altsetting):
            alt = iface.altsetting[j]
            print("  Interface Altsetting {}: Interface Number {}, Number of Endpoints: {}" .format(j, alt.bInterfaceNumber, alt.bNumEndpoints))
            for k in range(alt.bNumEndpoints):
                ep = alt.endpoint[k]
                print(f"    Endpoint {k}: address: 0x{ep.bEndpointAddress:02x} {_dir(ep.bEndpointAddress):3} {enum_mapping[ep.bmAttributes & 0b11]}")
    libusb.free_config_descriptor(cfg_ptr)


def set_endpoint_halt(handle, ep, timeout=TIMEOUT):
    # bmRequestType: 0x02 (Host-to-device, Standard, Endpoint)
    # bRequest:      0x03 (SET_FEATURE) with wValue = 0 (ENDPOINT_HALT)
    ret = libusb.control_transfer(handle, 0x02, 0x03, 0, ep, None, 0, timeout)
    return ret


def get_endpoint_halt(handle, ep, timeout=TIMEOUT):
    # bmRequestType: 0x82 (Device-to-host, Standard, Endpoint)
    # bRequest:      0x00 (GET_STATUS) with 2-byte response.
    buf = (ctypes.c_ubyte * 2)()
    ret = libusb.control_transfer(handle, 0x82, 0x00, 0, ep, buf, 2, timeout)
    if ret < 0:
        return None, ret
    status = buf[0] | (buf[1] << 8)
    halted = (status & 1) != 0
    return halted, 0


def clear_endpoint_halt(handle, ep, timeout=TIMEOUT):
    # bmRequestType: 0x02 (Host-to-device, Standard, Endpoint)
    # bRequest:      0x01 (CLEAR_FEATURE) with wValue = 0 (ENDPOINT_HALT)
    # ret = libusb.control_transfer(handle, 0x02, 0x01, 0, ep, None, 0, timeout)

    # use proper clear, because the halt resets the toggle, so the host needs to know
    ret = libusb.clear_halt(handle, ep)
    return ret


def main():
    try:
        opts, _ = getopt.getopt(sys.argv[1:], "e:d:i:")
    except getopt.GetoptError as err:
        sys.stderr.write("ERR: Invalid options: {}\n".format(err))
        sys.exit(1)

    ep = None
    iface = 0
    for opt, arg in opts:
        if opt == '-e':
            ep = int(arg, 0)
        elif opt == '-i':
            iface = int(arg, 0)

    ctx = ctypes.POINTER(libusb.context)()
    ret = libusb.init(ctypes.byref(ctx))
    if ret != 0:
        sys.stderr.write("Failed to initialize libusb, error {}\n".format(ret))
        sys.exit(1)

    # Get device list.
    devs = ctypes.POINTER(ctypes.POINTER(libusb.device))()
    cnt = libusb.get_device_list(ctx, ctypes.byref(devs))
    print("Number of attached USB devices = {}".format(cnt))

    my_dev = None
    for i in range(cnt):
        dev = devs[i]
        if is_myusb(dev):
            my_dev = dev
            break

    if not my_dev:
        sys.stderr.write("ERR: Device not found\n")
        libusb.free_device_list(devs, 1)
        libusb.exit(ctx)
        sys.exit(1)

    # Print the list of endpoints before performing endpoint halt operations.
    print("Listing device endpoints:")
    print_device_endpoints(my_dev)

    if ep is None:
        sys.stderr.write("ERR: -e: Specify endpoint number\n")
        libusb.free_device_list(devs, 1)
        libusb.exit(ctx)
        sys.exit(1)

    handle = ctypes.POINTER(libusb.device_handle)()
    ret = libusb.open(my_dev, ctypes.byref(handle))
    if ret != 0:
        sys.stderr.write("ERR: Failed to open device, error {}\n".format(libusb.strerror(ret)))
        libusb.free_device_list(devs, 1)
        libusb.exit(ctx)
        sys.exit(1)

    if libusb.kernel_driver_active(handle, iface) == 1:
        libusb.detach_kernel_driver(handle, iface)

    ret = libusb.claim_interface(handle, iface)
    if ret != 0:
        sys.stderr.write("ERR: Failed to claim interface {}: error {}\n".format(iface, libusb.strerror(ret)))
        libusb.close(handle)
        libusb.free_device_list(devs, 1)
        libusb.exit(ctx)
        sys.exit(1)

    print("Performing halt operations on endpoint 0x{:02x}".format(ep))

    # Set endpoint halt (stall).
    ret = set_endpoint_halt(handle, ep)
    if ret < 0:
        sys.stderr.write("Error setting halt on endpoint 0x{:02x}, error {}\n".format(ep, libusb.strerror(ret)))
    else:
        print("Endpoint 0x{:02x} set to halted state.".format(ep))

    # Get the halt status.
    halted, ret = get_endpoint_halt(handle, ep)
    if ret < 0:
        sys.stderr.write("Error getting halt status on endpoint 0x{:02x}, error {}\n".format(ep, libusb.strerror(ret)))
    else:
        print("Endpoint 0x{:02x} halt status: {}".format(ep, halted))

    # Clear the endpoint halt.
    ret = clear_endpoint_halt(handle, ep)
    if ret < 0:
        sys.stderr.write("Error clearing halt on endpoint 0x{:02x}, error {}\n".format(ep, libusb.strerror(ret)))
    else:
        print("Clear halt command issued for endpoint 0x{:02x}.".format(ep))

    # Verify the status after clearing.
    halted, ret = get_endpoint_halt(handle, ep)
    if ret < 0:
        sys.stderr.write("Error getting halt status after clear on endpoint 0x{:02x}, error {}\n".format(ep, libusb.strerror(ret)))
    else:
        print("Endpoint 0x{:02x} halt status after clear: {}".format(ep, halted))

    # Cleanup.
    libusb.release_interface(handle, iface)
    libusb.attach_kernel_driver(handle, iface)
    libusb.close(handle)
    libusb.free_device_list(devs, 1)
    libusb.exit(ctx)
    sys.exit(0)


if __name__ == '__main__':
    main()
