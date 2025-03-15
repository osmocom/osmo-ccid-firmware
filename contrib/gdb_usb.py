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

import gdb


EP_N_MASK = 0x0F
EP_DIR_MASK = 0x80
USB = 0x41000000
CONF_USB_D_MAX_EP_N = 4 # !!

eptstr = {
    0: "USB_D_EPTYPE_DISABLE",
    1: "USB_D_EPTYPE_CTRL",
    2: "USB_D_EPTYPE_ISOCH",
    3: "USB_D_EPTYPE_INT",
    4: "USB_D_EPTYPE_BULK",
    5: "USB_D_EPTYPE_DUAL",
    0x11: "USB_D_EPCFG_CTRL",
}

# bmAttributes
# define USB_EP_XTYPE_CTRL 0x0
# define USB_EP_XTYPE_ISOCH 0x1
# define USB_EP_XTYPE_BULK 0x2
# define USB_EP_XTYPE_INTERRUPT 0x3
# define USB_EP_XTYPE_MASK 0x3u


hwepty1 = {
    0x0: "Bank1 is disabled.",
    0x1: "Bank1 is enabled and configured as Control IN.",
    0x2: "Bank1 is enabled and configured as Isochronous IN.",
    0x3: "Bank1 is enabled and configured as Bulk IN.",
    0x4: "Bank1 is enabled and configured as Interrupt IN.",
    0x5: "Bank1 is enabled and configured as Dual-Bank OUT (Endpoint type is the same as the one defined in EPTYPE0)", }
# 0x6-0x7 Reserved
def USB_DEVICE_EPCFG_EPTYPE1(v): return (v >> 4) & 0x7


hwepty0 = {
    0x0: "Bank0 is disabled.",
    0x1: "Bank0 is enabled and configured as Control SETUP / Control OUT.",
    0x2: "Bank0 is enabled and configured as Isochronous OUT.",
    0x3: "Bank0 is enabled and configured as Bulk OUT.",
    0x4: "Bank0 is enabled and configured as Interrupt OUT.",
    0x5: "Bank0 is enabled and configured as Dual Bank IN (Endpoint type is the same as the one defined in EPTYPE1)", }
# 0x6-0x7 Reserved
def USB_DEVICE_EPCFG_EPTYPE0(v): return v & 0x7


def get_epdir(ep):
    return ep & EP_DIR_MASK


def get_epn(ep):
    return ep & EP_N_MASK


def _usb_d_dev_ept(epn, dir):
    if (epn == 0):
        ep_index = 0
    else:
        if dir:
            ep_index = (epn + CONF_USB_D_MAX_EP_N)
        else:
            ep_index = epn
    return gdb.parse_and_eval(f'dev_inst.ep[{ep_index}]')


def get_epcfg_typ(n, v):
    if n == 0:
        return v & 0x7
    if n == 1:
        return (v >> 4) & 0x7


def hri_usbendpoint_read_EPCFG_reg(epn):
    addr = USB+0x100+epn*0x20
    epcfg = gdb.parse_and_eval(f'*{addr}')
    return epcfg


CCID_USB_EPS = gdb.parse_and_eval(f'_ccid_df_funcd')
# print(USB_EPS["func_iface"])
for ept in ["func_ep_in", "func_ep_out", "func_ep_irq"]:
    cept = int(CCID_USB_EPS[ept])
    dir = 1 if get_epdir(cept) else 0
    epn = get_epn(cept)
    softep = gdb.parse_and_eval(f"*_usb_d_dev_ept({epn},{dir})")  # _usb_d_dev_ept(epn, dir)
    #  print(epn, dir)
    #  gdb.execute(f"p *_usb_d_dev_ept({epn},{dir})")
    epcfg = hri_usbendpoint_read_EPCFG_reg(epn)
    eptype = softep["flags"]["bits"]["eptype"]
    t0 = int(USB_DEVICE_EPCFG_EPTYPE0(epcfg))
    t1 = int(USB_DEVICE_EPCFG_EPTYPE1(epcfg))

    print(f"epn:{epn} {ept}:\tccid/soft:{int(cept):02x}:{int(softep["ep"]):02x}\tdir:{int(dir):02x} n:{int(epn):02x}",
          epcfg, eptstr[int(eptype)],  hwepty0[t0], "\t", hwepty1[t1])

USB_EPS = gdb.parse_and_eval(f'usb_d_inst.ep')
NUM_EPS = USB_EPS.type.sizeof // USB_EPS.type.target().sizeof
for iusb in range(0, NUM_EPS):
    iusbd = gdb.parse_and_eval(f'usb_d_inst.ep[{iusb}]')
    state = iusbd['xfer']['hdr']['state'].cast(gdb.lookup_type("enum usb_ep_state"))
    status = iusbd['xfer']['hdr']['status'].cast(gdb.lookup_type("enum usb_xfer_code"))
    print(iusb, "####", iusbd)
    print(state)
    print(status)
