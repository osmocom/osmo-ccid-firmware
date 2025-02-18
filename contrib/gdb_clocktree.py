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

class ClockTreePrinter(gdb.Command):

    def __init__(self):
        super(ClockTreePrinter, self).__init__("print-clock-tree", gdb.COMMAND_USER)

        self.known_xosc1_freq = 12e6

        self.OSCCTRL_BASE = 0x40001000
        self.OSC32KCTRL_BASE = 0x40001400
        self.GCLK_BASE = 0x40001C00
        self.MCLK_BASE = 0x40000800

        self.dpll_sources = {
            0x0 : "GCLK", # Dedicated GCLK clock reference
            0x1 : "XOSC32", # XOSC32K clock reference (default)
            0x2 : "XOSC0", # XOSC0 clock reference
            0x3 : "XOSC1", # XOSC1 clock reference
            #Other - Reserved
        }
        self.clock_sources = {
            "XOSC0" : 0x00, # XOSC 0 oscillator output
            "XOSC1" : 0x01, # XOSC 1 oscillator output
            "GCLK_IN" : 0x02, # Generator input pad (GCLK_IO)
            "GCLK_GEN1" : 0x03, # Generic clock generator 1 output
            "OSCULP32K" : 0x04, # OSCULP32K oscillator output
            "XOSC32K" : 0x05, # XOSC32K oscillator output
            "DFLL" : 0x06, # DFLL48M oscillator output
            "DPLL0" : 0x07, # FDPLL200M0 output
            "DPLL1" : 0x08, # FDPLL200M1 output
            # 0x09-0x0F Reserved Reserved
        }

        self.generic_clocks = range(0, 12)  # GCLK0 to GCLK11
        self.main_clocks = [
            "CPU", "APBA", "APBB", "APBC", "APBD", "APBE"
        ]

        self._peripheral_ids = [
            # grep -rh _GCLK_ID 2>/dev/null | grep define |tr -s ' ' |sort -Vk3 | uniq | sed -E 's/^#define ([^ ]+)[[:space:]]+([0-9]+).*/\2 : "\1"/p' | uniq
            {0 : "OSCCTRL_GCLK_ID_DFLL48"},
            {1 : "OSCCTRL_GCLK_ID_FDPLL0"},
            {2 : "I2S_GCLK_ID_SIZE"},
            {2 : "OSCCTRL_GCLK_ID_FDPLL1"},
            {3 : "SERCOM0_GCLK_ID_SLOW"},
            {3 : "SERCOM1_GCLK_ID_SLOW"},
            {3 : "SERCOM2_GCLK_ID_SLOW"},
            {3 : "SERCOM3_GCLK_ID_SLOW"},
            {3 : "SERCOM4_GCLK_ID_SLOW"},
            {3 : "SERCOM5_GCLK_ID_SLOW"},
            {3 : "SERCOM6_GCLK_ID_SLOW"},
            {3 : "SERCOM7_GCLK_ID_SLOW"},
            {3 : "SDHC0_GCLK_ID_SLOW"},
            {3 : "SDHC1_GCLK_ID_SLOW"},
            {3 : "OSCCTRL_GCLK_ID_FDPLL032K"},
            {3 : "OSCCTRL_GCLK_ID_FDPLL132K"},
            {4 : "EIC_GCLK_ID"},
            {5 : "FREQM_GCLK_ID_MSR"},
            {7 : "SERCOM0_GCLK_ID_CORE"},
            {8 : "SERCOM1_GCLK_ID_CORE"},
            {9 : "TC0_GCLK_ID"},
            {9 : "TC1_GCLK_ID"},
            {10 : "USB_GCLK_ID"},
            {11 : "EVSYS_GCLK_ID_0"},
            {11 : "EVSYS_GCLK_ID_LSB"},
            {12 : "EVSYS_GCLK_ID_1"},
            {12 : "EVSYS_GCLK_ID_SIZE"},
            {13 : "EVSYS_GCLK_ID_2"},
            {14 : "EVSYS_GCLK_ID_3"},
            {15 : "EVSYS_GCLK_ID_4"},
            {16 : "EVSYS_GCLK_ID_5"},
            {17 : "EVSYS_GCLK_ID_6"},
            {18 : "EVSYS_GCLK_ID_7"},
            {19 : "EVSYS_GCLK_ID_8"},
            {20 : "EVSYS_GCLK_ID_9"},
            {21 : "EVSYS_GCLK_ID_10"},
            {22 : "EVSYS_GCLK_ID_11"},
            {22 : "EVSYS_GCLK_ID_MSB"},
            {23 : "SERCOM2_GCLK_ID_CORE"},
            {24 : "SERCOM3_GCLK_ID_CORE"},
            {25 : "TCC0_GCLK_ID"},
            {25 : "TCC1_GCLK_ID"},
            {26 : "TC2_GCLK_ID"},
            {26 : "TC3_GCLK_ID"},
            {27 : "CAN0_GCLK_ID"},
            {28 : "CAN1_GCLK_ID"},
            {29 : "TCC2_GCLK_ID"},
            {29 : "TCC3_GCLK_ID"},
            {30 : "TC4_GCLK_ID"},
            {30 : "TC5_GCLK_ID"},
            {31 : "PDEC_GCLK_ID"},
            {32 : "AC_GCLK_ID"},
            {33 : "CCL_GCLK_ID"},
            {34 : "SERCOM4_GCLK_ID_CORE"},
            {35 : "SERCOM5_GCLK_ID_CORE"},
            {36 : "SERCOM6_GCLK_ID_CORE"},
            {37 : "SERCOM7_GCLK_ID_CORE"},
            {38 : "TCC4_GCLK_ID"},
            {39 : "TC6_GCLK_ID"},
            {39 : "TC7_GCLK_ID"},
            {40 : "ADC0_GCLK_ID"},
            {41 : "ADC1_GCLK_ID"},
            {42 : "DAC_GCLK_ID"},
            {43 : "I2S_GCLK_ID_0"},
            {43 : "I2S_GCLK_ID_LSB"},
            {44 : "I2S_GCLK_ID_1"},
            {44 : "I2S_GCLK_ID_MSB"},
            {45 : "SDHC0_GCLK_ID"},
            {46 : "SDHC1_GCLK_ID"},
        ]
        self.peripheral_ids = defaultdict(list)
        for e in self._peripheral_ids:
            for k,v in e.items():
                self.peripheral_ids[k].append(v)

    def read_register(self, address):
        val = gdb.parse_and_eval(f"*(uint32_t *)({address})")
        return int(val)

    def read_register_field(self, reg_val, mask, shift):
        return (reg_val & mask) >> shift

    def get_clock_source_name(self, source_id):
        for name, id in self.clock_sources.items():
            if id == source_id:
                return name
        return f"UNKNOWN_SOURCE({source_id})"

    def get_xosc_frequency(self, xosc_num):
        """Get frequency of external oscillator"""
        xosc_ctrl = self.read_register(self.OSCCTRL_BASE + 0x14 + (xosc_num * 4))
        if self.read_register_field(xosc_ctrl, 0x2, 1) == 1:  # Enabled?#
            if self.read_register_field(xosc_ctrl, 0x4, 2) == 1:  # External crystal
                freq_range = self.read_register_field(xosc_ctrl, 0x00f00000, 20)
                if freq_range == 0:
                    return "0.4 to 2 MHz (external crystal)"
                elif freq_range == 1:
                    return "2 to 4 MHz (external crystal)"
                elif freq_range == 2:
                    return "4 to 8 MHz (external crystal)"
                elif freq_range == 3:
                    return "8 to 16 MHz (external crystal)"
                elif freq_range == 4:
                    return "16 to 32 MHz (external crystal)"
                else:
                    return "External clock (check configuration)"
            else:  # External clock
                return "External clock (check configuration)"
        return f"Disabled {xosc_ctrl:032b}"

    def get_xosc32k_frequency(self):
        """Get frequency of 32kHz internal oscillator"""
        osc32k_ctrl = self.read_register(self.OSC32KCTRL_BASE + 0x14)
        if osc32k_ctrl & 0x02:  # Enabled?
            return "32.768 kHz"
        return f"Disabled {osc32k_ctrl:b}"

    def get_osculp32k_frequency(self):
        """Get frequency of ultra low power 32kHz oscillator"""
        osculp32k_ctrl = self.read_register(self.OSC32KCTRL_BASE + 0x1C)
        en32k = self.read_register_field(osculp32k_ctrl, 0x02, 1)
        en1k = self.read_register_field(osculp32k_ctrl, 0x04, 2)

        retstr =  f"Disabled"
        if en32k == 1:
            retstr = "32.768 kHz (Ultra Low Power)"
        if en1k == 1:
            retstr += "+ 1 kHz (Ultra Low Power)"
        retstr += f" {osculp32k_ctrl:b}"
        return retstr

    def get_dfll_frequency(self):
        """Get DFLL frequency and configuration"""
        dfll_ctrl = self.read_register(self.OSCCTRL_BASE + 0x1c)
        if dfll_ctrl & 0x02:  # Enabled?
            dfll_config = self.read_register(self.OSCCTRL_BASE + 0x20)

            if dfll_config & 0x1:  # Closed loop
                pchctrl = self.read_register(self.GCLK_BASE + 0x80)  # PCHCTRL[0] for DFLL
                gclk_id = pchctrl & 0xf # self.read_register_field(pchctrl, 0x3e, 1)

                mult = self.read_register(self.OSCCTRL_BASE + 0x28) & 0xfff

                # This is simplified - would need to trace through GCLK configuration
                return f"Closed loop mode, Ref: GCLK{gclk_id}, Mult: {mult} (~48 MHz)"
            else:
                return "Open loop mode (~48 MHz)" + (" USBCRM" if dfll_config & 0x8 else "")
        return f"Disabled {dfll_ctrl:032b}"

    def get_dpll_frequency(self, dpll_num):
        """Get DPLL frequency and configuration"""
        dpll_base = self.OSCCTRL_BASE + 0x30 + (dpll_num * 0x14)

        dpll_ctrl_a = self.read_register(dpll_base)
        if dpll_ctrl_a & 0x02:  # Enabled?
            dpll_status = self.read_register(dpll_base + 0x10)
            # if dpll_status & 0x01:  # Locked?
            dpll_ctrl_b = self.read_register(dpll_base + 0x08)
            refclk = self.read_register_field(dpll_ctrl_b, 0xe0, 5)

            ref_src = ""
            if refclk == 0:
                ref_src = "GCLK"
            elif refclk == 1:
                ref_src = "XOSC32K"
            elif refclk == 2:
                ref_src = "XOSC0"
            elif refclk == 3:
                ref_src = "XOSC1"

            ratio = self.read_register(dpll_base + 0x04)
            ldr = self.read_register_field(ratio, 0x1FFF, 0)
            ldrfrac = self.read_register_field(ratio, 0xF000, 16)

            # Calculate actual ratio (simplified)
            actual_ratio = ldr + (ldrfrac / 16.0)

            refdiv = 2 * (self.read_register_field(dpll_ctrl_b, 0xffff0000, 16) +1)

            rets = f"{ "Locked" if dpll_status & 0x1 else "NOT locked"}, Ref: {ref_src}, Ref div: {refdiv}, Ratio: {actual_ratio:.2f}"

            if refclk == 3:
                rets += f" -> op freq {self.known_xosc1_freq/refdiv * (actual_ratio+1)}"

            return rets

        return f"Disabled {dpll_ctrl_a:032b}"

    def get_clock_frequency(self, source_id):
        """Get frequency of a clock source"""
        if source_id == self.clock_sources["XOSC0"]:
            return self.get_xosc_frequency(0)

        elif source_id == self.clock_sources["XOSC1"]:
            return self.get_xosc_frequency(1)

        elif source_id == self.clock_sources["XOSC32K"]:
            return self.get_xosc32k_frequency()

        elif source_id == self.clock_sources["OSCULP32K"]:
            return self.get_osculp32k_frequency()

        elif source_id == self.clock_sources["DFLL"]:
            return self.get_dfll_frequency()

        elif source_id == self.clock_sources["DPLL0"]:
            return self.get_dpll_frequency(0)

        elif source_id == self.clock_sources["DPLL1"]:
            return self.get_dpll_frequency(1)

        elif source_id == self.clock_sources["GCLK_GEN1"]:
            # This would be circular, so just indicate it's from another GCLK
            return "From GCLK Generator 1"

        return "Unknown"

    def print_generic_clock(self, gclk_num):
        # Each GCLKn has its own GENCTRL register
        genctrl_addr = self.GCLK_BASE + 0x20 + (gclk_num * 0x04)
        genctrl = self.read_register(genctrl_addr)

        enabled = genctrl & (0x1 << 8)
        if not enabled:
            print(f"  GCLK{gclk_num}: Disabled")
            return

        source_id = self.read_register_field(genctrl, 0x0F, 0)
        source_name = self.get_clock_source_name(source_id)

        div_val = self.read_register_field(genctrl, 0xFFFF, 16)
        div_mode = self.read_register_field(genctrl, (0x1 << 12), 8)

        if div_mode:  # 50/50 duty cycle divider
            actual_div = div_val * 2
        else:
            actual_div = div_val

        if actual_div == 0 or actual_div == 1:
            divider_str = "No division"
        else:
            divider_str = f"Divide by {actual_div}"

        print(f"  GCLK{gclk_num}: Enabled, Source: {source_name}, {divider_str}")

        # Print peripherals using this GCLK
        print(f"    Peripherals using GCLK{gclk_num}:")
        peripherals_found = False
        for periph_id in self.peripheral_ids.keys():
            pchctrl_addr = self.GCLK_BASE + 0x80 + (periph_id * 0x04)
            pchctrl = self.read_register(pchctrl_addr)

            enabled = pchctrl & (0x1 << 6)
            if not enabled:
                continue

            gen = self.read_register_field(pchctrl, 0x0f, 0)
            if gen == gclk_num:
                print(f"      - {'\n        '.join(self.peripheral_ids[periph_id])}")
                peripherals_found = True

        if not peripherals_found:
            print("      None")

    def print_main_clocks(self):
        """Print main clock configuration (CPU, AHB, APBx)"""
        hsdiv = self.read_register(self.MCLK_BASE + 0x04)
        cpudiv = self.read_register(self.MCLK_BASE + 0x05)
        div = cpudiv & 0xFF
        if div == 0 or div == 1:
            cpu_div = "No division"
        else:
            cpu_div = f"Divide by {div}"

        print(f"  CPU Clock: {cpu_div}")
        print(f"  HS div: {hsdiv & 0xFF}")

        bridge_names = ['H', 'A', 'B', 'C', 'D']

        for i, bridge in enumerate(bridge_names):
            mask = self.read_register(self.MCLK_BASE + 0x10 + (i * 4))
            print(f"  APB{bridge} Bridge: Mask 0x{mask:08X}")

            if mask != 0:
                print(f"    Enabled peripherals:")

                # grep -rh ".*define.*MCLK_A.*MASK.*Pos" 2>/dev/null |  grep -v _U_ | tr -s ' ' |sort -Vk3 | uniq | sort -k1.8,1.9 | sed -E 's/^#define ([^ ]+)[[:space:]]+([0-9]+).*/\2 : "\1"/p' | sed -s 's/_Pos//g' | uniq | sort -k2.11,2.13 -Vk1
                if bridge == 'H':
                    apb_peripherals = {
                        0 : "MCLK_AHBMASK_HPB0",
                        1 : "MCLK_AHBMASK_HPB1",
                        2 : "MCLK_AHBMASK_HPB2",
                        3 : "MCLK_AHBMASK_HPB3",
                        4 : "MCLK_AHBMASK_DSU",
                        5 : "MCLK_AHBMASK_HMATRIX",
                        6 : "MCLK_AHBMASK_NVMCTRL",
                        7 : "MCLK_AHBMASK_HSRAM",
                        8 : "MCLK_AHBMASK_CMCC",
                        9 : "MCLK_AHBMASK_DMAC",
                        10 : "MCLK_AHBMASK_USB",
                        11 : "MCLK_AHBMASK_BKUPRAM",
                        12 : "MCLK_AHBMASK_PAC",
                        13 : "MCLK_AHBMASK_QSPI",
                        14 : "MCLK_AHBMASK_GMAC",
                        15 : "MCLK_AHBMASK_SDHC0",
                        16 : "MCLK_AHBMASK_SDHC1",
                        17 : "MCLK_AHBMASK_CAN0",
                        18 : "MCLK_AHBMASK_CAN1",
                        19 : "MCLK_AHBMASK_ICM",
                        20 : "MCLK_AHBMASK_PUKCC",
                        21 : "MCLK_AHBMASK_QSPI_2X",
                        22 : "MCLK_AHBMASK_NVMCTRL_SMEEPROM",
                        23 : "MCLK_AHBMASK_NVMCTRL_CACHE",
                    }
                elif bridge == 'A':
                    apb_peripherals = {
                        0 : "MCLK_APBAMASK_PAC",
                        1 : "MCLK_APBAMASK_PM",
                        2 : "MCLK_APBAMASK_MCLK",
                        3 : "MCLK_APBAMASK_RSTC",
                        4 : "MCLK_APBAMASK_OSCCTRL",
                        5 : "MCLK_APBAMASK_OSC32KCTRL",
                        6 : "MCLK_APBAMASK_SUPC",
                        7 : "MCLK_APBAMASK_GCLK",
                        8 : "MCLK_APBAMASK_WDT",
                        9 : "MCLK_APBAMASK_RTC",
                        10 : "MCLK_APBAMASK_EIC",
                        11 : "MCLK_APBAMASK_FREQM",
                        12 : "MCLK_APBAMASK_SERCOM0",
                        13 : "MCLK_APBAMASK_SERCOM1",
                        14 : "MCLK_APBAMASK_TC0",
                        15 : "MCLK_APBAMASK_TC1",
                    }
                elif bridge == 'B':
                    apb_peripherals = {
                        0 : "MCLK_APBBMASK_USB",
                        1 : "MCLK_APBBMASK_DSU",
                        2 : "MCLK_APBBMASK_NVMCTRL",
                        4 : "MCLK_APBBMASK_PORT",
                        6 : "MCLK_APBBMASK_HMATRIX",
                        7 : "MCLK_APBBMASK_EVSYS",
                        9 : "MCLK_APBBMASK_SERCOM2",
                        10 : "MCLK_APBBMASK_SERCOM3",
                        11 : "MCLK_APBBMASK_TCC0",
                        12 : "MCLK_APBBMASK_TCC1",
                        13 : "MCLK_APBBMASK_TC2",
                        14 : "MCLK_APBBMASK_TC3",
                        16 : "MCLK_APBBMASK_RAMECC",
                    }
                elif bridge == 'C':
                    apb_peripherals = {
                        2 : "MCLK_APBCMASK_GMAC",
                        3 : "MCLK_APBCMASK_TCC2",
                        4 : "MCLK_APBCMASK_TCC3",
                        5 : "MCLK_APBCMASK_TC4",
                        6 : "MCLK_APBCMASK_TC5",
                        7 : "MCLK_APBCMASK_PDEC",
                        8 : "MCLK_APBCMASK_AC",
                        9 : "MCLK_APBCMASK_AES",
                        10 : "MCLK_APBCMASK_TRNG",
                        11 : "MCLK_APBCMASK_ICM",
                        13 : "MCLK_APBCMASK_QSPI",
                        14 : "MCLK_APBCMASK_CCL",
                    }
                elif bridge == 'D':
                    apb_peripherals = {
                        0 : "MCLK_APBDMASK_SERCOM4",
                        1 : "MCLK_APBDMASK_SERCOM5",
                        2 : "MCLK_APBDMASK_SERCOM6",
                        3 : "MCLK_APBDMASK_SERCOM7",
                        4 : "MCLK_APBDMASK_TCC4",
                        5 : "MCLK_APBDMASK_TC6",
                        6 : "MCLK_APBDMASK_TC7",
                        7 : "MCLK_APBDMASK_ADC0",
                        8 : "MCLK_APBDMASK_ADC1",
                        9 : "MCLK_APBDMASK_DAC",
                        10 : "MCLK_APBDMASK_I2S",
                        11 : "MCLK_APBDMASK_PCC",
                    }
                else:
                    apb_peripherals = {}

                # Check each bit in the mask
                for bit in range(32):
                    if mask & (1 << bit):
                        periph_name = apb_peripherals.get(bit, f"Unknown({bit})")
                        print(f"      - {periph_name}")

    def print_clock_tree(self):
        print("SAME54 Clock Tree")
        print("=================")

        print("\n Clock Sources:")
        print("----------------")
        for name, source_id in self.clock_sources.items():
            freq = self.get_clock_frequency(source_id)
            print(f"  {name}: {freq}")

        print("\n Generic Clocks:")
        print("----------------")
        for gclk_num in self.generic_clocks:
            self.print_generic_clock(gclk_num)

        print("\n Main Clock Buses:")
        print("------------------")
        self.print_main_clocks()

    def invoke(self, arg, from_tty):
        """GDB command invocation"""
        self.print_clock_tree()



ClockTreePrinter()