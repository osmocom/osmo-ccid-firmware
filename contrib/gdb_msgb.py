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

class MsgbListPrinter(gdb.Command):
    """Prints all msgb entries in a libosmocore linked list"""

    def __init__(self):
        super(MsgbListPrinter, self).__init__("print-msgb-list", gdb.COMMAND_USER)

    # prettier full fledged: https://raw.githubusercontent.com/hotsphink/sfink-tools/master/conf/gdbinit.pahole.py
    def container_of(self, ptr, typename, member):
        ty = gdb.lookup_type(typename)
        # first matching field
        bofs = [f.bitpos//8 for f in ty.strip_typedefs().fields() if f.name == member][0]
        container_addr = ptr.cast(gdb.lookup_type('unsigned long')) - int(bofs)
        return container_addr.cast(ty.pointer())


    def invoke(self, args, from_tty):
        try:
            if not args:
                # print("Usage: print-msgb-list list_head")
                # return
                heads_names = [[f"g_ccid_s.{epn}.list"] for epn in ["in_ep", "irq_ep", "out_ep"]] #  f"*(struct msgb*)g_ccid_s.{epn}.in_progress"
                heads_names = [nested_item for item in heads_names for nested_item in item]  + [f"g_ccid_s.free_q"]
                heads = [gdb.parse_and_eval(i) for i in heads_names]

            else:
                heads_names = [args]
                heads = [gdb.parse_and_eval(args)]

            # print(heads)

            for hn, list_head in zip(heads_names, heads):
                print(hn)
                # list_head = gdb.parse_and_eval(args)
                if not list_head.address:
                    continue
                # print(list_head)
                try:
                    list_head = list_head["list"]
                    current = list_head
                except:
                    current = list_head['next']

                head_addr = list_head.address
                count = 0
                # print(current, list_head)

                while current != head_addr:
                    msgb = self.container_of(current, "struct msgb", "list")
                    print(f"msgb[{int(count):02}] at {int(current):08x}: <-{int(msgb["list"]["prev"]):08x} {int(msgb["list"]["next"]):08x}-> len: {int(msgb['len'])}")
                    # print(f"  data: {msgb['data']}")
                    # print(f"  head: {msgb['head']}")
                    # print(f"  tail: {msgb['tail']}")

                    current = msgb['list']['next']
                    count += 1
                    if current == 0x00100100 or current == 0x00200200: # poison values
                        break

                print(f"\nTotal msgb entries: {count}" if count else "empty!")

        except gdb.error as e:
            print(f"Error: {str(e)}")

MsgbListPrinter()

