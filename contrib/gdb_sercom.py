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


##grep -A15 name\>SERCOM ATSAME54N19A.svd | grep base
# <baseAddress>0x40003000</baseAddress>
# <baseAddress>0x40003400</baseAddress>
# <baseAddress>0x41012000</baseAddress>
# <baseAddress>0x41014000</baseAddress>
# <baseAddress>0x43000000</baseAddress>
# <baseAddress>0x43000400</baseAddress>
# <baseAddress>0x43000800</baseAddress>
# <baseAddress>0x43000C00</baseAddress>
# <baseAddress>0x40001800</baseAddress>


def gdbdl(addr):
	plong=gdb.lookup_type('long').pointer()
	val=gdb.Value(addr).cast(plong).dereference()
	return int(val)&0xffffffff


sercbases = [
0x40003000,
0x40003400,
0x41012000,
0x41014000,
0x43000000,
0x43000400,
0x43000800,
0x43000C00,
0x40001800]

USB_EPS=gdb.parse_and_eval (f'usb_d_inst.ep')
NUM_EPS= USB_EPS.type.sizeof // USB_EPS.type.target().sizeof

# 5 #### {
#   xfer = {
#     hdr = {
#       type = 0 '\000',
#       ep = 255 '\377',
#       state = 0 '\000',
#       status = 0 '\000'
#     },

for iusb in range(0,NUM_EPS):
	iusbd = gdb.parse_and_eval (f'usb_d_inst.ep[{iusb}]');
	state = iusbd['xfer']['hdr']['state'].cast(gdb.lookup_type("enum usb_ep_state"))
	status = iusbd['xfer']['hdr']['status'].cast(gdb.lookup_type("enum usb_xfer_code"))
	print(iusb, "####", iusbd)
	print(state)
	print(status)

def print_usblists():
	def _print_usblists(structptr):
		print(structptr)
		current = gdb.parse_and_eval(structptr)
		while current != 0:
			print(current)
			current = current['next']

	_print_usblists("(struct usbdf_driver *)usbdc.func_list.head")
	_print_usblists("(struct usbdc_req_handler *)usbdc.handlers.req_list.head")
	_print_usblists("(struct usbdc_change_handler *)usbdc.handlers.change_list.head")
	_print_usblists("(struct usbdc_sof_handler *)usbdc.handlers.sof_list.head")
print_usblists()


for i in range(0,7):
	# >>> py  print(gdb.parse_and_eval("((Sercom *)0x40003000)->USART.CTRLA.bit").type.fields()[0].name)
	#SWRST
	uartix = gdb.parse_and_eval (f"g_si.slot[{i}].cuart.u.asf4.usa_pd.device.hw")
	u = gdb.parse_and_eval(f"((Sercom *){uartix})->USART")

	for screg in u.type.fields():
		if 'reserved' in screg.name.lower():
			continue
		scregf = [x.name for x in u[str(screg.name)].type.fields()]
		# print(screg.name, scregf)
		if 'bit' in scregf:
			ctral_f = [fn.name for fn in u[str(screg.name)]["bit"].type.fields()]
			# print(ctral_f)
			vals = {x: u[screg.name]["bit"][x] for x in ctral_f}
			print(uartix, ' '.join([f"{k}: {int(v)}" for k,v in vals.items()]))
		# ctral_f = [fn.name for fn in u[str(screg.name)]["bit"].type.fields()]
		# vals = {x: str(u[screg.name]["bit"][x]) for x in ctral_f}
		# print(' '.join([f"{k}: {v}" for k,v in vals.items()]))


for i in range(0,7):
	uslotdata = gdb.parse_and_eval (f"*g_si.slot[{i}].cuart")
	print("###################", uslotdata)


def _get_enum_flen(ty, v):
	gdbty = gdb.lookup_type(ty)
	maxl = len(max(gdb.types.make_enum_dict(gdbty).keys()))
	return str(v.cast(gdbty)).rjust(20)

# iso7816_3_state_len = "{max(gdb.types.make_enum_dict(gdb.lookup_type("enum iso7816_3_state")).keys())
# atr_state_len = max(gdb.types.make_enum_dict(gdb.lookup_type("enum atr_state")).keys())
# tpdu_state_len = max(gdb.types.make_enum_dict(gdb.lookup_type("enum tpdu_state")).keys())
# pps_state_len = max(gdb.types.make_enum_dict(gdb.lookup_type("enum pps_state")).keys())

# d = gdb.parse_and_eval (f'(enum iso7816_3_state)g_si.slot[{i}].fi.state')
# a = gdb.parse_and_eval (f'(enum atr_state)((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).atr_fi.state')
# b = gdb.parse_and_eval (f'(enum tpdu_state)((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).tpdu_fi.state' )
# c = gdb.parse_and_eval (f'(enum pps_state)((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).pps_fi.state')

for i in range(0,7):
	# slot = gdb.execute ('p g_ci.slot[%i]' % (i) , False, True)
	# a = gdb.execute ('p (enum atr_state)((struct iso7816_3_priv*)g_si.slot[%i].fi.priv).atr_fi.state' % (i) , False, True).split('\n')[-2]#.rpartition('\n')
	# b = gdb.execute ('p (enum tpdu_state)((struct iso7816_3_priv*)g_si.slot[%i].fi.priv).tpdu_fi.state' % (i) , False, True).split('\n')[-2]#.rpartition('\n')
	# c = gdb.execute ('p (enum pps_state)((struct iso7816_3_priv*)g_si.slot[%i].fi.priv).pps_fi.state' % (i) , False, True).split('\n')[-2]#.rpartition('\n')
	# d = gdb.execute ('p (enum iso7816_3_state)g_si.slot[%i].fi.state' % (i) , False, True).split('\n')[-2]#.rpartition('\n')

	uslotdata = gdb.parse_and_eval (f"g_si.slot[{i}].cuart.u.asf4.usa_pd.device.hw")
	uslotdata_str = (f"data_reg: {hex(gdbdl(uslotdata+0x28) & 0xffffffff)}")


	g_slot = gdb.parse_and_eval(f'g_si.slot[{i}]')

	# slot = gdb.parse_and_eval(f'g_ci.slot[{i}]')
	# d = gdb.parse_and_eval (f'(enum iso7816_3_state)g_si.slot[{i}].fi.state')
	# a = gdb.parse_and_eval (f'(enum atr_state)((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).atr_fi.state')
	# b = gdb.parse_and_eval (f'(enum tpdu_state)((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).tpdu_fi.state' )
	# c = gdb.parse_and_eval (f'(enum pps_state)((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).pps_fi.state')


	d = gdb.parse_and_eval (f'g_si.slot[{i}].fi')
	a = gdb.parse_and_eval (f'((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).atr_fi')
	b = gdb.parse_and_eval (f'((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).tpdu_fi')
	c = gdb.parse_and_eval (f'((struct iso7816_3_priv*)g_si.slot[{i}].fi.priv).pps_fi')

	# sloevent = gdb.parse_and_eval (f'g_si.slot[{i}].cs.event')
	# sloeventdata = gdb.parse_and_eval (f'g_si.slot[{i}].cs.event_data')

	print(
		i,
		f"seq: {int(g_slot["seq"]):3d}",
		_get_enum_flen("enum iso7816_3_state", d['state']),
		_get_enum_flen("enum atr_state", a['state']),
		_get_enum_flen("enum tpdu_state", b['state']),
		_get_enum_flen("enum pps_state", c['state']),
		uslotdata_str,
		f"ev: {g_slot['cs']['event']} evdata: {g_slot['cs']['event_data']} ", end='', sep=None)


	# for i in sercbases:
	# 	print(f"data{i} {hex(gdbdl(i+0x28) & 0xffffffff)}")

	# gdb.execute ('x/20b (&SIM%d.rx->buf[SIM%d.rx->write_index & SIM%d.rx->size])-20' % (i,i,i) , True)

	xx = gdb.parse_and_eval ('g_si.slot[%i].cuart.u.asf4.usa_pd.tx_por' % (i))
	xy = gdb.parse_and_eval ('g_si.slot[%i].cuart.u.asf4.usa_pd.tx_buffer_length' % (i))
	idx = int(xx)
	idx2 = int(xy)
	nval = None
	bval = None

	if idx != idx2:
		nval = int(gdb.parse_and_eval ('g_si.slot[%i].cuart.u.asf4.usa_pd.tx_buffer[%i]' % (i, idx)))

	if idx > 0:
		bval = int(gdb.parse_and_eval ('g_si.slot[%i].cuart.u.asf4.usa_pd.tx_buffer[%i]' % (i, idx-1)))

	#print idx, idx2, bval
	# print(f"{i}: ", end='', sep=None)
	if nval is not None and bval is not None:
		print("last tx:0x" + "{:02x}".format(bval) + " next tx:0x" + "{:02x}".format(nval))
	elif nval is None and bval is not None:
		print("last tx:0x" + "{:02x}".format(bval) + " next tx:none" )
	elif nval is not None and bval is None:
		print("last tx:none next tx:0x" + "{:02x}".format(nval))
	else:
		print(f"wat?! {nval} {bval} {idx} {idx2}")
