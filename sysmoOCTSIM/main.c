/*
 * Copyright (C) 2019 sysmocom -s.f.m.c. GmbH, Author: Kevin Redon <kredon@sysmocom.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <parts.h>
#include <hal_cache.h>
#include <hri_port_e54.h>

#include "atmel_start.h"
#include "atmel_start_pins.h"
#include "config/hpl_gclk_config.h"

#include "i2c_bitbang.h"
#include "octsim_i2c.h"
#include "ncn8025.h"
#include "iso7816_3.h"

#include "command.h"

// TODO put declaration in more global file
// TODO for now SIM7 is not present because used for debug
static struct usart_async_descriptor* SIM_peripheral_descriptors[] = {&SIM0, &SIM1, &SIM2, &SIM3, &SIM4, &SIM5, &SIM6, NULL};

/** number of bytes transmitted on the SIM peripheral */
static volatile bool SIM_tx_count[8];

static void SIM_rx_cb(const struct usart_async_descriptor *const io_descr)
{
}

/** called when the transmission is complete
 *  e.g. this is when the byte has been sent and there is no data to transmit anymore
 */
static void SIM_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	// find slotnr for corresponding USART
	uint8_t slotnr;
	for (slotnr = 0; slotnr < ARRAY_SIZE(SIM_peripheral_descriptors) && SIM_peripheral_descriptors[slotnr] != io_descr; slotnr++);

	// set flag
	if (slotnr < ARRAY_SIZE(SIM_peripheral_descriptors)) {
		SIM_tx_count[slotnr] = true;
	}
}

/** possible clock sources for the SERCOM peripheral
 *  warning: the definition must match the GCLK configuration
 */
static const uint8_t sercom_glck_sources[] = {GCLK_PCHCTRL_GEN_GCLK2_Val, GCLK_PCHCTRL_GEN_GCLK4_Val, GCLK_PCHCTRL_GEN_GCLK6_Val};

/** possible clock frequencies in MHz for the SERCOM peripheral
 *  warning: the definition must match the GCLK configuration
 */
static const double sercom_glck_freqs[] = {100E6 / CONF_GCLK_GEN_2_DIV, 100E6 / CONF_GCLK_GEN_4_DIV, 120E6 / CONF_GCLK_GEN_6_DIV};

/** the GCLK ID for the SERCOM SIM peripherals
 *  @note: used as index for PCHCTRL
 */
static const uint8_t SIM_peripheral_GCLK_ID[] = {SERCOM0_GCLK_ID_CORE, SERCOM1_GCLK_ID_CORE, SERCOM2_GCLK_ID_CORE, SERCOM3_GCLK_ID_CORE, SERCOM4_GCLK_ID_CORE, SERCOM5_GCLK_ID_CORE, SERCOM6_GCLK_ID_CORE, SERCOM7_GCLK_ID_CORE};

static void board_init()
{
	int i;

	for (i = 0; i < 4; i++)
		i2c_init(&i2c[i]);

	for (i = 0; i < 8; i++)
		ncn8025_init(i);

	cache_init();
	cache_enable(CMCC);

	/* increase drive strength of 20Mhz SIM clock output to 8mA
	 * (there are 8 inputs + traces to drive!) */
	hri_port_set_PINCFG_DRVSTR_bit(PORT, 0, 11);

	// enable SIM interfaces
	for (uint8_t i = 0; i < ARRAY_SIZE(SIM_peripheral_descriptors); i++) {
		if (NULL == SIM_peripheral_descriptors[i]) {
			continue;
		}
		usart_async_register_callback(SIM_peripheral_descriptors[i], USART_ASYNC_RXC_CB, SIM_rx_cb); // required for RX to work, even if the callback does nothing
		usart_async_register_callback(SIM_peripheral_descriptors[i], USART_ASYNC_TXC_CB, SIM_tx_cb); // to count the number of bytes transmitted since we are using it asynchronously
		usart_async_enable(SIM_peripheral_descriptors[i]);
	}
}

static int validate_slotnr(int argc, char **argv, int idx)
{
	int slotnr;
	if (argc < idx+1) {
		printf("You have to specify the slot number (0..7)\r\n");
		return -1;
	}
	slotnr = atoi(argv[idx]);
	if (slotnr < 0 || slotnr > 7) {
		printf("You have to specify the slot number (0..7)\r\n");
		return -1;
	}
	return slotnr;
}

/** change baud rate of card slot
 *  @param[in] slotnr slot number for which the baud rate should be set
 *  @param[in] baudrate baud rate in bps to set
 *  @return if the baud rate has been set, else a parameter is out of range
 */
static bool slot_set_baudrate(uint8_t slotnr, uint32_t baudrate)
{
	ASSERT(slotnr < ARRAY_SIZE(SIM_peripheral_descriptors));

	// calculate the error corresponding to the clock sources
	uint16_t bauds[ARRAY_SIZE(sercom_glck_freqs)];
	double errors[ARRAY_SIZE(sercom_glck_freqs)];
	for (uint8_t i = 0; i < ARRAY_SIZE(sercom_glck_freqs); i++) {
		double freq = sercom_glck_freqs[i]; // remember possible SERCOM frequency
		uint32_t min = freq / (2 * (255 + 1)); // calculate the minimum baud rate for this frequency
		uint32_t max = freq / (2 * (0 + 1)); // calculate the maximum baud rate for this frequency
		if (baudrate < min || baudrate > max) { // baud rate it out of supported range
			errors[i] = NAN;
		} else {
			uint16_t baud = round(freq / (2 * baudrate) - 1);
			bauds[i] = baud;
			double actual = freq / (2 * (baud + 1));
			errors[i] = fabs(1.0 - (actual / baudrate));
		}
	}

	// find the smallest error
	uint8_t best = ARRAY_SIZE(sercom_glck_freqs);
	for (uint8_t i = 0; i < ARRAY_SIZE(sercom_glck_freqs); i++) {
		if (isnan(errors[i])) {
			continue;
		}
		if (best >= ARRAY_SIZE(sercom_glck_freqs)) {
			best = i;
		} else if (errors[i] < errors[best]) {
			best = i;
		}
	}
	if (best >= ARRAY_SIZE(sercom_glck_freqs)) { // found no clock supporting this baud rate
		return false;
	}

	// set clock and baud rate
	struct usart_async_descriptor* slot = SIM_peripheral_descriptors[slotnr]; // get slot
	if (NULL == slot) {
		return false;
	}
	printf("(%u) switching SERCOM clock to GCLK%u (freq = %lu kHz) and baud rate to %lu bps (baud = %u)\r\n", slotnr, (best + 1) * 2, (uint32_t)(round(sercom_glck_freqs[best] / 1000)), baudrate, bauds[best]);
	while (!usart_async_is_tx_empty(slot)); // wait for transmission to complete (WARNING no timeout)
	usart_async_disable(slot); // disable SERCOM peripheral
	hri_gclk_clear_PCHCTRL_reg(GCLK, SIM_peripheral_GCLK_ID[slotnr], (1 << GCLK_PCHCTRL_CHEN_Pos)); // disable clock for this peripheral
	while (hri_gclk_get_PCHCTRL_reg(GCLK, SIM_peripheral_GCLK_ID[slotnr], (1 << GCLK_PCHCTRL_CHEN_Pos))); // wait until clock is really disabled
	// it does not seem we need to completely disable the peripheral using hri_mclk_clear_APBDMASK_SERCOMn_bit
	hri_gclk_write_PCHCTRL_reg(GCLK, SIM_peripheral_GCLK_ID[slotnr], sercom_glck_sources[best] | (1 << GCLK_PCHCTRL_CHEN_Pos)); // set peripheral core clock and re-enable it
	usart_async_set_baud_rate(slot, bauds[best]); // set the new baud rate
	usart_async_enable(slot); // re-enable SERCOM peripheral

	return true;
}

/** change ISO baud rate of card slot
 *  @param[in] slotnr slot number for which the baud rate should be set
 *  @param[in] clkdiv can clock divider
 *  @param[in] f clock rate conversion integer F
 *  @param[in] d baud rate adjustment factor D
 *  @return if the baud rate has been set, else a parameter is out of range
 */
static bool slot_set_isorate(uint8_t slotnr, enum ncn8025_sim_clkdiv clkdiv, uint16_t f, uint8_t d)
{
	// input checks
	ASSERT(slotnr < ARRAY_SIZE(SIM_peripheral_descriptors));
	if (clkdiv != SIM_CLKDIV_1 && clkdiv != SIM_CLKDIV_2 && clkdiv != SIM_CLKDIV_4 && clkdiv != SIM_CLKDIV_8) {
		return false;
	}
	if (!iso7816_3_valid_f(f)) {
		return false;
	}
	if (!iso7816_3_valid_d(d)) {
		return false;
	}

	// set clockdiv
	struct ncn8025_settings settings;
	ncn8025_get(slotnr, &settings);
	if (settings.clkdiv != clkdiv) {
		settings.clkdiv = clkdiv;
		ncn8025_set(slotnr, &settings);
	}

	// calculate desired frequency
	uint32_t freq = 20000000UL; // maximum frequency
	switch (clkdiv) {
	case SIM_CLKDIV_1:
		freq /= 1;
		break;
	case SIM_CLKDIV_2:
		freq /= 2;
		break;
	case SIM_CLKDIV_4:
		freq /= 4;
		break;
	case SIM_CLKDIV_8:
		freq /= 8;
		break;
	}

	// set baud rate
	uint32_t baudrate = (freq * d) / f; // calculate actual baud rate
	return slot_set_baudrate(slotnr, baudrate); // // set baud rate
}

DEFUN(sim_status, cmd_sim_status, "sim-status", "Get state of specified NCN8025")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	if (slotnr < 0)
		return;
	ncn8025_get(slotnr, &settings);
	printf("SIM%d: ", slotnr);
	ncn8025_dump(&settings);
	printf("\r\n");
}

DEFUN(sim_power, cmd_sim_power, "sim-power", "Enable/disable SIM card power")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	int enable;

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify 0=disable or 1=enable\r\n");
		return;
	}
	enable = atoi(argv[2]);
	ncn8025_get(slotnr, &settings);
	if (enable)
		settings.cmdvcc = true;
	else
		settings.cmdvcc = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_reset, cmd_sim_reset, "sim-reset", "Enable/disable SIM reset")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	int enable;

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify 0=disable or 1=enable\r\n");
		return;
	}
	enable = atoi(argv[2]);
	ncn8025_get(slotnr, &settings);
	if (enable)
		settings.rstin = true;
	else
		settings.rstin = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_clkdiv, cmd_sim_clkdiv, "sim-clkdiv", "Set SIM clock divider (1,2,4,8)")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);
	int clkdiv;

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify a valid divider (1,2,4,8)\r\n");
		return;
	}
	clkdiv = atoi(argv[2]);
	if (clkdiv != 1 && clkdiv != 2 && clkdiv != 4 && clkdiv != 8) {
		printf("You have to specify a valid divider (1,2,4,8)\r\n");
		return;
	}
	ncn8025_get(slotnr, &settings);
	switch (clkdiv) {
	case 1:
		settings.clkdiv = SIM_CLKDIV_1;
		break;
	case 2:
		settings.clkdiv = SIM_CLKDIV_2;
		break;
	case 4:
		settings.clkdiv = SIM_CLKDIV_4;
		break;
	case 8:
		settings.clkdiv = SIM_CLKDIV_8;
		break;
	}
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_voltage, cmd_sim_voltage, "sim-voltage", "Set SIM voltage (5/3/1.8)")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify a valid voltage (5/3/1.8)\r\n");
		return;
	}
	ncn8025_get(slotnr, &settings);
	if (!strcmp(argv[2], "5"))
		settings.vsel = SIM_VOLT_5V0;
	else if (!strcmp(argv[2], "3"))
		settings.vsel = SIM_VOLT_3V0;
	else if (!strcmp(argv[2], "1.8"))
		settings.vsel = SIM_VOLT_1V8;
	else {
		printf("You have to specify a valid voltage (5/3/1.8)\r\n");
		return;
	}
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_led, cmd_sim_led, "sim-led", "Set SIM LED (1=on, 0=off)")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);

	if (slotnr < 0)
		return;

	if (argc < 3) {
		printf("You have to specify 0=disable or 1=enable\r\n");
		return;
	}
	ncn8025_get(slotnr, &settings);
	if (atoi(argv[2]))
		settings.led = true;
	else
		settings.led = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_atr, cmd_sim_atr, "sim-atr", "Read ATR from SIM card")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);

	if (slotnr < 0 || slotnr >= ARRAY_SIZE(SIM_peripheral_descriptors) || NULL == SIM_peripheral_descriptors[slotnr]) {
		return;
	}

	// check if card is present (and read current settings)
	ncn8025_get(slotnr, &settings);
	if (!settings.simpres) {
		printf("(%d) error: no card present\r\n", slotnr);
		return;
	}

	// switch card off (assert reset and disable power)
	// note: ISO/IEC 7816-3:2006 section 6.4 provides the deactivation sequence, but not the minimum corresponding times
	settings.rstin = true;
	settings.cmdvcc = false;
	settings.led = true;
	ncn8025_set(slotnr, &settings);

	// TODO wait some time for card to be completely deactivated
	usart_async_flush_rx_buffer(SIM_peripheral_descriptors[slotnr]); // flush RX buffer to start from scratch

	
	// set clock to lowest frequency (20 MHz / 8 = 2.5 MHz)
	// note: according to ISO/IEC 7816-3:2006 section 5.2.3 the minimum value is 1 MHz, and maximum is 5 MHz during activation
	settings.clkdiv = SIM_CLKDIV_8;
	// set USART baud rate to match the interface (f = 2.5 MHz) and card default settings (Fd = 372, Dd = 1)
	slot_set_isorate(slotnr, settings.clkdiv, ISO7816_3_DEFAULT_FD, ISO7816_3_DEFAULT_DD);
	// set card voltage to 3.0 V (the most supported)
	// note: according to ISO/IEC 7816-3:2006 no voltage should damage the card, and you should cycle from low to high
	settings.vsel = SIM_VOLT_3V0;
	// provide power (the NCN8025 should perform the activation according to spec)
	// note: activation sequence is documented in ISO/IEC 7816-3:2006 section 6.2
	settings.cmdvcc = true;
	ncn8025_set(slotnr, &settings);

	// wait for Tb=400 cycles before re-asserting reset
	delay_us(400 * 10000 / 2500); // 400 cycles * 1000 for us, 2.5 MHz / 1000 for us

	// de-assert reset to switch card back on
	settings.rstin = false;
	ncn8025_set(slotnr, &settings);

	// wait for Tc=40000 cycles for transmission to start
	uint32_t cycles = 40000;
	while (cycles && !usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		delay_us(10);
		cycles -= 25; // 10 us = 25 cycles at 2.5 MHz
	}
	if (!usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		delay_us(12 * 372 / 1 / 2); // wait more than one byte (approximate freq down to 2 MHz)
	}
	// verify if one byte has been received
	if (!usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		printf("(%d) error: card not responsive\r\n", slotnr);
		return;
	}

	// read ATR (just do it until there is no traffic anymore)
	// TODO the ATR should be parsed to read the right number of bytes, instead we just wait until to end of WT
	printf("(%d) ATR: ", slotnr);
	uint8_t atr_byte;
	while (usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
		if (1 == io_read(&SIM_peripheral_descriptors[slotnr]->io, &atr_byte, 1)) {
			printf("%02x ", atr_byte);
		}
		uint16_t wt = ISO7816_3_DEFAULT_WT; // waiting time in ETU
		while (wt && !usart_async_is_rx_not_empty(SIM_peripheral_descriptors[slotnr])) {
			delay_us(149); // wait for 1 ETU (372 / 1 / 2.5 MHz = 148.8 us)
			wt--;
		}
	}
	printf("\r\n");

	/* disable LED */
	settings.led = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(sim_iccid, cmd_sim_iccid, "sim-iccid", "Read ICCID from SIM card")
{
	struct ncn8025_settings settings;
	int slotnr = validate_slotnr(argc, argv, 1);

	if (slotnr < 0 || slotnr >= ARRAY_SIZE(SIM_peripheral_descriptors) || NULL == SIM_peripheral_descriptors[slotnr]) {
		return;
	}

	// read current settings and check if card is present and powered
	ncn8025_get(slotnr, &settings);
	if (!settings.simpres) {
		printf("(%d) error: no card present\r\n", slotnr);
		return;
	}
	if (!settings.cmdvcc) {
		printf("(%d) error: card not powered\r\n", slotnr);
		return;
	}
	if (settings.rstin) {
		printf("(%d) error: card under reset\r\n", slotnr);
		return;
	}

	// enable LED
	if (!settings.led) {
		settings.led = true;
		ncn8025_set(slotnr, &settings);
	}

	// read MF
	printf("(%d) SELECT MF: ", slotnr);
	struct usart_async_descriptor* sim = SIM_peripheral_descriptors[slotnr];
	((Sercom *)sim->device.hw)->USART.CTRLB.bit.RXEN = 0;
	usart_async_flush_rx_buffer(sim); // flush RX buffer to start from scratch
	// write SELECT MF APDU
	const uint8_t select_mf[] = {0xa0, 0xa4, 0x00, 0x00, 0x02, 0x3f, 0x00};
	SIM_tx_count[slotnr] = false; // reset TX complete
	for (uint8_t i = 0; i < 5; i++) { // transmit command header
		printf("%02x ", select_mf[i]);
		while(!usart_async_is_tx_empty(sim)); // wait for previous byte to be transmitted (WARNING could block)
		if (1 != io_write(&sim->io, &select_mf[i], 1)) {
			printf("(%d) error: could not send command header\r\n", slotnr);
			return;
		}
	}
	uint16_t wt = ISO7816_3_DEFAULT_WT; // waiting time in ETU (actually it can be a lot more after the ATR, but we use the default)
	while (wt && !SIM_tx_count[slotnr]) { // wait until transmission is complete
		delay_us(149); // wait for 1 ETU (372 / 1 / 2.5 MHz = 148.8 us)
		wt--;
	}
	if (0 == wt) { // timeout reached
		printf("(%d) error: could not transmit data\r\n", slotnr);
		return;
	}
	((Sercom *)sim->device.hw)->USART.CTRLB.bit.RXEN = 1;
	//usart_async_flush_rx_buffer(sim); // flush RX buffer to start from scratch
	wt = ISO7816_3_DEFAULT_WT; // reset waiting time
	while (wt && !usart_async_is_rx_not_empty(sim)) { // wait for procedure byte
		delay_us(149); // wait for 1 ETU (372 / 1 / 2.5 MHz = 148.8 us)
		wt--;
	}
	if (0 == wt) { // timeout reached
		printf("(%d) error: card not responsive\r\n", slotnr);
		return;
	}
	uint8_t pb;
	if (1 != io_read(&sim->io, &pb, 1)) { // read procedure byte
		printf("(%d) error: could not read data\r\n", slotnr);
		return;
	}
	if (select_mf[1] != pb) { // the header is ACKed when the procedure by is equal to the instruction byte
		printf("(%d) error: header NACKed\r\n", slotnr);
		return;
	}
	SIM_tx_count[slotnr] = false; // reset TX complete
	for (uint8_t i = 5; i < 7; i++) { // transmit command data
		printf("%02x ", select_mf[i]);
		while(!usart_async_is_tx_empty(sim)); // wait for previous byte to be transmitted (WARNING could block)
		if (1 != io_write(&sim->io, &select_mf[i], 1)) {
			printf("(%d) error: could not send command data\r\n", slotnr);
			return;
		}
	}
	wt = ISO7816_3_DEFAULT_WT; // waiting time in ETU (actually it can be a lot more after the ATR, but we use the default)
	while (wt && !SIM_tx_count[slotnr]) { // wait until transmission is complete
		delay_us(149); // wait for 1 ETU (372 / 1 / 2.5 MHz = 148.8 us)
		wt--;
	}
	if (0 == wt) { // timeout reached
		printf("(%d) error: could not transmit data\r\n", slotnr);
		return;
	}
	usart_async_flush_rx_buffer(sim); // flush RX buffer to start from scratch
	// read SW
	uint8_t sw[2]; // to read the status words
	wt = ISO7816_3_DEFAULT_WT; // waiting time in ETU (actually it can be a lot more after the ATR, but we use the default)
	for (uint8_t i = 0; i < ARRAY_SIZE(sw); i++) {
		while (wt && !usart_async_is_rx_not_empty(sim)) {
			delay_us(149); // wait for 1 ETU (372 / 1 / 2.5 MHz = 148.8 us)
			wt--;
		}
		if (0 == wt) { // timeout reached
			printf("(%d) error: card not responsive\r\n", slotnr);
		} else {
			if (1 != io_read(&sim->io, &sw[i], 1)) { // read SW
				printf("(%d) error: could not read data\r\n", slotnr);
				return;
			}
			printf("%02x ", sw[i]);
			wt = ISO7816_3_DEFAULT_WT; // reset WT
		}
	}
	printf("\r\n");

	// disable LED
	settings.led = false;
	ncn8025_set(slotnr, &settings);
}

extern void testmode_init(void);

int main(void)
{
	atmel_start_init();

	usb_start();

	board_init();
	command_init("sysmoOCTSIM> ");
	command_register(&cmd_sim_status);
	command_register(&cmd_sim_power);
	command_register(&cmd_sim_reset);
	command_register(&cmd_sim_clkdiv);
	command_register(&cmd_sim_voltage);
	command_register(&cmd_sim_led);
	command_register(&cmd_sim_atr);
	command_register(&cmd_sim_iccid);
	testmode_init();

	printf("\r\n\r\nsysmocom sysmoOCTSIM\r\n");

	command_print_prompt();
	while (true) { // main loop
		command_try_recv();
	}
}
