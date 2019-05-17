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
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <parts.h>
#include <errno.h>

#include <osmocom/core/utils.h>

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

static void ccid_app_init(void);

static void board_init()
{
	int i;

	for (i = 0; i < 4; i++)
		i2c_init(&i2c[i]);

	for (i = 0; i < 8; i++)
		ncn8025_init(i);

	cache_init();
	cache_enable(CMCC);
	calendar_enable(&CALENDAR_0);

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

	ccid_app_init();
}

/***********************************************************************
 * CCID Driver integration
 ***********************************************************************/

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/msgb.h>
#include "linuxlist_atomic.h"
#include "ccid_df.h"

struct usb_ep_q {
	const char *name;
	/* msgb queue of pending to-be-transmitted (IN/IRQ) or completed received (OUT)
	 * USB transfers */
	struct llist_head list;
	/* currently ongoing/processed msgb (USB transmit or receive */
	struct msgb *in_progress;
};

struct ccid_state {
	/* msgb queue of free msgs */
	struct llist_head free_q;

	/* msgb queue of pending to-be-transmitted (IN EP) */
	struct usb_ep_q in_ep;
	/* msgb queue of pending to-be-transmitted (IRQ EP) */
	struct usb_ep_q irq_ep;
	/* msgb queue of completed received (OUT EP) */
	struct usb_ep_q out_ep;

	/* bit-mask of card-insert status, as determined from NCN8025 IRQ output */
	uint8_t card_insert_mask;
};
static struct ccid_state g_ccid_s;

static void ccid_out_read_compl(const uint8_t ep, enum usb_xfer_code code, uint32_t transferred);
static void ccid_in_write_compl(const uint8_t ep, enum usb_xfer_code code, uint32_t transferred);
static void ccid_irq_write_compl(const uint8_t ep, enum usb_xfer_code code, uint32_t transferred);

static void usb_ep_q_init(struct usb_ep_q *ep_q, const char *name)
{
	ep_q->name = name;
	INIT_LLIST_HEAD(&ep_q->list);
	ep_q->in_progress = NULL;
}

static void ccid_app_init(void)
{
	/* initialize data structures */
	INIT_LLIST_HEAD(&g_ccid_s.free_q);
	usb_ep_q_init(&g_ccid_s.in_ep, "IN");
	usb_ep_q_init(&g_ccid_s.irq_ep, "IRQ");
	usb_ep_q_init(&g_ccid_s.out_ep, "OUT");

	/* OUT endpoint read complete callback (irq context) */
	ccid_df_register_callback(CCID_DF_CB_READ_OUT, (FUNC_PTR)&ccid_out_read_compl);
	/* IN endpoint write complete callback (irq context) */
	ccid_df_register_callback(CCID_DF_CB_WRITE_IN, (FUNC_PTR)&ccid_in_write_compl);
	/* IRQ endpoint write complete callback (irq context) */
	ccid_df_register_callback(CCID_DF_CB_WRITE_IRQ, (FUNC_PTR)&ccid_irq_write_compl);
}

/* irqsafe version of msgb_enqueue */
struct msgb *msgb_dequeue_irqsafe(struct llist_head *q)
{
	struct msgb *msg;
	CRITICAL_SECTION_ENTER()
	msg = msgb_dequeue(q);
	CRITICAL_SECTION_LEAVE()
	return msg;
}

void msgb_enqueue_irqsafe(struct llist_head *q, struct msgb *msg)
{
	CRITICAL_SECTION_ENTER()
	msgb_enqueue(q, msg);
	CRITICAL_SECTION_LEAVE()
}

/* submit the next pending (if any) message for the IN EP */
static int submit_next_in(void)
{
	struct usb_ep_q *ep_q = &g_ccid_s.in_ep;
	struct msgb *msg;
	int rc;

	OSMO_ASSERT(!ep_q->in_progress);
	msg = msgb_dequeue_irqsafe(&ep_q->list);
	if (!msg)
		return 0;

	ep_q->in_progress = msg;
	rc = ccid_df_write_in(msgb_data(msg), msgb_length(msg));
	if (rc != ERR_NONE) {
		printf("EP %s failed: %d\r\n", ep_q->name, rc);
		return -1;
	}
	return 1;

}

/* submit the next pending (if any) message for the IRQ EP */
static int submit_next_irq(void)
{
	struct usb_ep_q *ep_q = &g_ccid_s.irq_ep;
	struct msgb *msg;
	int rc;

	OSMO_ASSERT(!ep_q->in_progress);
	msg = msgb_dequeue_irqsafe(&ep_q->list);
	if (!msg)
		return 0;

	ep_q->in_progress = msg;
	rc = ccid_df_write_irq(msgb_data(msg), msgb_length(msg));
	/* may return HALTED/ERROR/DISABLED/BUSY/ERR_PARAM/ERR_FUNC/ERR_DENIED */
	if (rc != ERR_NONE) {
		printf("EP %s failed: %d\r\n", ep_q->name, rc);
		return -1;
	}
	return 1;
}

static int submit_next_out(void)
{
	struct usb_ep_q *ep_q = &g_ccid_s.out_ep;
	struct msgb *msg;
	int rc;

	OSMO_ASSERT(!ep_q->in_progress);
	msg = msgb_dequeue_irqsafe(&g_ccid_s.free_q);
	if (!msg)
		return -1;
	ep_q->in_progress = msg;

	rc = ccid_df_read_out(msgb_data(msg), msgb_tailroom(msg));
	if (rc != ERR_NONE) {
		/* re-add to the list of free msgb's */
		llist_add_tail_at(&g_ccid_s.free_q, &msg->list);
		return 0;
	}
	return 1;
}

/* OUT endpoint read complete callback (irq context) */
static void ccid_out_read_compl(const uint8_t ep, enum usb_xfer_code code, uint32_t transferred)
{
	struct msgb *msg = g_ccid_s.out_ep.in_progress;

	/* add just-received msg to tail of endpoint queue */
	OSMO_ASSERT(msg);
	/* update msgb with the amount of data received */
	msgb_put(msg, transferred);
	/* append to list of pending-to-be-handed messages */
	llist_add_tail_at(&msg->list, &g_ccid_s.out_ep.list);

	/* submit another [free] msgb to receive the next transfer */
	submit_next_out();
}

/* IN endpoint write complete callback (irq context) */
static void ccid_in_write_compl(const uint8_t ep, enum usb_xfer_code code, uint32_t transferred)
{
	struct msgb *msg = g_ccid_s.in_ep.in_progress;

	OSMO_ASSERT(msg);
	/* return the message back to the queue of free message buffers */
	llist_add_tail_at(&msg->list, &g_ccid_s.free_q);
	g_ccid_s.in_ep.in_progress = NULL;

	/* submit the next pending to-be-transmitted msgb (if any) */
	submit_next_in();
}

/* IRQ endpoint write complete callback (irq context) */
static void ccid_irq_write_compl(const uint8_t ep, enum usb_xfer_code code, uint32_t transferred)
{
	struct msgb *msg = g_ccid_s.irq_ep.in_progress;

	OSMO_ASSERT(msg);
	/* return the message back to the queue of free message buffers */
	llist_add_tail_at(&msg->list, &g_ccid_s.free_q);
	g_ccid_s.irq_ep.in_progress = NULL;

	/* submit the next pending to-be-transmitted msgb (if any) */
	submit_next_irq();
}

#include "ccid_proto.h"
struct msgb *ccid_gen_notify_slot_status(const uint8_t *bitmask, uint8_t bm_len)
{
	//struct msgb *msg = ccid_msgb_alloc();
	struct msgb *msg = msgb_alloc(64, "IRQ");
	struct ccid_rdr_to_pc_notify_slot_change *nsc = msgb_put(msg, sizeof(*nsc) + bm_len);
	nsc->bMessageType = RDR_to_PC_NotifySlotChange;
	memcpy(&nsc->bmSlotCCState, bitmask, bm_len);

	return msg;
}

/* check if any card detect state has changed */
static void poll_card_detect(void)
{
	uint8_t new_mask = 0;
	struct msgb *msg;
	unsigned int i;

	for (i = 0; i < 8; i++) {
		bool irq_level = ncn8025_interrupt_level(i);
		if (irq_level)
			new_mask &= ~(1 << i);
		else
			new_mask |= (1 << i);
	}

	/* notify the user/host about any changes */
	if (g_ccid_s.card_insert_mask != new_mask) {
		printf("CARD_DET 0x%02x -> 0x%02x\r\n",
			g_ccid_s.card_insert_mask, new_mask);
		msg = ccid_gen_notify_slot_status(&new_mask, 1);
		msgb_enqueue_irqsafe(&g_ccid_s.irq_ep.list, msg);

		g_ccid_s.card_insert_mask = new_mask;
	}
}



/***********************************************************************
 * Command Line interface
 ***********************************************************************/

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
	return slot_set_baudrate(slotnr, baudrate); // set baud rate
}

/** write data to card
 *  @param[in] slotnr slot number on which to send data
 *  @param[in] data data to be transmitted
 *  @param[in] length length of data to be transmitted
 *  @return error code
 */
static int slot_card_write(uint8_t slotnr, const uint8_t* data, uint16_t length)
{
	// input checks
	ASSERT(slotnr < ARRAY_SIZE(SIM_peripheral_descriptors));
	if (0 == length || NULL == data) {
		return ERR_INVALID_ARG;
	}

	struct usart_async_descriptor* sim = SIM_peripheral_descriptors[slotnr];
	((Sercom *)sim->device.hw)->USART.CTRLB.bit.RXEN = 0; // disable receive (to avoid the echo back)
	SIM_tx_count[slotnr] = false; // reset TX complete
	for (uint16_t i = 0; i < length; i++) { // transmit data
		while(!usart_async_is_tx_empty(sim)); // wait for previous byte to be transmitted (WARNING blocking)
		if (1 != io_write(&sim->io, &data[i], 1)) { // put but in transmit buffer
			return ERR_IO;
		}
	}
	while (!SIM_tx_count[slotnr]); // wait until transmission is complete (WARNING blocking)
	((Sercom *)sim->device.hw)->USART.CTRLB.bit.RXEN = 1; // enable receive again

	return ERR_NONE;
}

/** read data from card
 *  @param[in] slotnr slot number on which to send data
 *  @param[out] data buffer for read data to be stored
 *  @param[in] length length of data to be read
 *  @param[in] wt Waiting Time in ETU
 *  @return error code
 *  TODO fix WT/ETU duration
 */
static int slot_card_read(uint8_t slotnr, uint8_t* data, uint16_t length, uint32_t wt)
{
	// input checks
	ASSERT(slotnr < ARRAY_SIZE(SIM_peripheral_descriptors));
	if (0 == length || NULL == data) {
		return ERR_INVALID_ARG;
	}

	struct usart_async_descriptor* sim = SIM_peripheral_descriptors[slotnr];

	((Sercom *)sim->device.hw)->USART.CTRLB.bit.RXEN = 1; // ensure RX is enabled
	uint32_t timeout = wt; // reset waiting time
	for (uint16_t i = 0; i < length; i++) { // read all data
		while (timeout && !usart_async_is_rx_not_empty(sim)) { // verify if data is present
			delay_us(149); // wait for 1 ETU (372 / 1 / 2.5 MHz = 148.8 us)
			timeout--;
		}
		if (0 == timeout) { // timeout reached
			return ERR_TIMEOUT;
		}
		timeout = wt; // reset waiting time
		if (1 != io_read(&sim->io, &data[i], 1)) { // read one byte
			return ERR_IO;
		}
	}

	return ERR_NONE;
}

/** transfer TPDU
 *  @param[in] slotnr slot number on which to transfer the TPDU
 *  @param[in] header TPDU header to send
 *  @param[io] data TPDU data to transfer
 *  @param[in] data_length length of TPDU data to transfer
 *  @param[in] write if the data should be written (true) or read (false)
 * TODO fix WT
 * TODO the data length can be deduce from the header
 */
static int slot_tpdu_xfer(uint8_t slotnr, const uint8_t* header, uint8_t* data, uint16_t data_length, bool write)
{
	// input checks
	ASSERT(slotnr < ARRAY_SIZE(SIM_peripheral_descriptors));
	if (NULL == header || (data_length > 0 && NULL == data)) {
		return ERR_INVALID_ARG;
	}

	int rc;
	struct usart_async_descriptor* sim = SIM_peripheral_descriptors[slotnr]; // get USART peripheral
	usart_async_flush_rx_buffer(sim); // flush RX buffer to start from scratch

	// send command header
	printf("(%d) TPDU: ", slotnr);
	for (uint8_t i = 0; i < 5; i++) {
		printf("%02x ", header[i]);
	}
	rc = slot_card_write(slotnr, header, 5); // transmit header
	if (ERR_NONE != rc) {
		printf("error in command header transmit (errno = %d)\r\n", rc);
		return rc;
	}

	// read procedure byte, and handle data
	uint8_t pb = 0x60; // wait more procedure byte
	uint16_t data_i = 0; // progress in the data transfer
	while (0x60 == pb) { // wait for SW
		rc = slot_card_read(slotnr, &pb, 1, ISO7816_3_DEFAULT_WT);
		if (ERR_NONE != rc) {
			printf("error while receiving PB/SW1 (errno = %d)\r\n", rc);
			return rc;
		}
		printf("%02x ", pb);
		if (0x60 == pb) { // NULL byte
			// just wait more time
		} else if ((0x60 == (pb & 0xf0)) || (0x90 == (pb & 0xf0))) { // SW1 byte
			// left the rest of the code handle it
		} else if (header[1] == pb) { // ACK byte
			// transfer rest of the data
			if (data_i >= data_length) {
				printf("error no more data to transfer\r\n");
				return ERR_INVALID_DATA;
			}
			if (write) { // transmit remaining command data
				rc = slot_card_write(slotnr, &data[data_i], data_length - data_i); // transmit command data
				if (ERR_NONE != rc) {
					printf("error in command data transmit (errno = %d)\r\n", rc);
					return rc;
				}
			} else { // receive remaining command data
				rc = slot_card_read(slotnr, &data[data_i], data_length - data_i, ISO7816_3_DEFAULT_WT);
				if (ERR_NONE != rc) {
					printf("error in command data receive (errno = %d)\r\n", rc);
					return rc;
				}
			}
			for (uint16_t i = data_i; i < data_length; i++) {
				printf("%02x ", data[i]);
			}
			data_i = data_length; // remember we transferred the data
			pb = 0x60; // wait for SW1
		} else if (header[1] == (pb ^ 0xff)) { // ACK byte
			// transfer only one byte
			if (data_i >= data_length) {
				printf("error no more data to transfer\r\n");
				return ERR_INVALID_DATA;
			}
			if (write) { // transmit command data byte
				rc = slot_card_write(slotnr, &data[data_i], 1); // transmit command data
				if (ERR_NONE != rc) {
					printf("error in command data transmit (errno = %d)\r\n", rc);
					return rc;
				}
			} else { // receive command data byte
				rc = slot_card_read(slotnr, &data[data_i], 1, ISO7816_3_DEFAULT_WT);
				if (ERR_NONE != rc) {
					printf("error in command data receive (errno = %d)\r\n", rc);
					return rc;
				}
			}
			printf("%02x ", data[data_i]);
			data_i += 1; // remember we transferred one data byte
			pb = 0x60; // wait for SW1
		} else { // invalid byte
			return ERR_INVALID_DATA;
		}
	}

	// read SW2
	uint8_t sw2;
	rc = slot_card_read(slotnr, &sw2, 1, ISO7816_3_DEFAULT_WT);
	if (ERR_NONE != rc) {
		printf("error in receiving SW2 (errno = %d)\r\n", rc);
		return rc;
	}
	printf("%02x", sw2);

	printf("\r\n");
	return ERR_NONE;
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

	// select MF
	printf("(%d) SELECT MF\r\n", slotnr);
	const uint8_t select_header[] = {0xa0, 0xa4, 0x00, 0x00, 0x02}; // see TS 102.221 sec. 11.1.1
	const uint8_t select_data_mf[] = {0x3f, 0x00}; // see TS 102.221 sec. 13.1
	int rc = slot_tpdu_xfer(slotnr, select_header, (uint8_t*)select_data_mf, ARRAY_SIZE(select_data_mf), true); // transfer TPDU
	if (ERR_NONE != rc) {
		printf("error while SELECT MF (errno = %d)\r\n", rc);
	}
	// ignore response data

	// select EF_ICCID
	printf("(%d) SELECT EF_ICCID\r\n", slotnr);
	const uint8_t select_data_ef_iccid[] = {0x2f, 0xe2}; // see TS 102.221 sec. 13.2
	rc = slot_tpdu_xfer(slotnr, select_header, (uint8_t*)select_data_ef_iccid, ARRAY_SIZE(select_data_ef_iccid), true); // transfer TPDU
	if (ERR_NONE != rc) {
		printf("error while SELECT EF_ICCID (errno = %d)\r\n", rc);
	}
	// ignore response data

	// read EF_ICCID
	printf("(%d) READ EF_ICCID\r\n", slotnr);
	uint8_t iccid[10];
	uint8_t read_binary[] = {0xa0, 0xb0, 0x00, 0x00, ARRAY_SIZE(iccid)}; // see TS 102.221 sec. 11.1.3
	rc = slot_tpdu_xfer(slotnr, read_binary, iccid, ARRAY_SIZE(iccid), false); // transfer TPDU
	if (ERR_NONE != rc) {
		printf("error while READ ICCID (errno = %d)\r\n", rc);
	}
	// ignore response data

	printf("(%d) ICCID: ", slotnr);
	for (uint8_t i = 0; i < ARRAY_SIZE(iccid); i++) {
		uint8_t nibble = iccid[i] & 0xf;
		if (0xf == nibble) {
			break;
		}
		printf("%x", nibble);
		nibble = iccid[i] >> 4;
		if (0xf == nibble) {
			break;
		}
		printf("%x", nibble);
	}
	printf("\r\n");

	// disable LED
	settings.led = false;
	ncn8025_set(slotnr, &settings);
}

DEFUN(get_time, cmd_get_time, "get-time", "Read Time from RTC")
{
	struct calendar_date_time dt;
	calendar_get_date_time(&CALENDAR_0, &dt);
	printf("%04u-%02u-%02u %02u:%02u:%02u\r\n", dt.date.year, dt.date.month, dt.date.day,
		dt.time.hour, dt.time.min, dt.time.sec);
}

#include <osmocom/core/timer.h>
static struct osmo_timer_list t;
static void tmr_cb(void *data)
{
	printf("timer fired!\r\n");
}
DEFUN(test_timer, cmd_test_timer, "test-timer", "Test osmo_timer")
{
	printf("Setting up timer for 3s...\n\r");
	osmo_timer_setup(&t, &tmr_cb, NULL);
	osmo_timer_schedule(&t, 3, 0);
}


extern void testmode_init(void);
extern void libosmo_emb_init(void);
extern void libosmo_emb_mainloop(void);

#include "talloc.h"
#include "logging.h"
#include <osmocom/core/msgb.h>
void *g_tall_ctx;

DEFUN(_talloc_report, cmd_talloc_report, "talloc-report", "Generate a talloc report")
{
	talloc_report_full(g_tall_ctx, stdout);
}

DEFUN(talloc_test, cmd_talloc_test, "talloc-test", "Test the talloc allocator")
{
	for (int i = 0; i < 10; i++)
		talloc_named_const(g_tall_ctx, 10, "sibling");
}

DEFUN(v_talloc_free, cmd_talloc_free, "talloc-free", "Release all memory")
{
	talloc_free(g_tall_ctx);
	g_tall_ctx = NULL;
}

/* Section 9.6 of SAMD5x/E5x Family Data Sheet */
static int get_chip_unique_serial(uint8_t *out, size_t len)
{
	uint32_t *out32 = (uint32_t *)out;
	if (len < 16)
		return -EINVAL;

	out32[0] = *(uint32_t *)0x008061fc;
	out32[1] = *(uint32_t *)0x00806010;
	out32[2] = *(uint32_t *)0x00806014;
	out32[3] = *(uint32_t *)0x00806018;

	return 0;
}

/* same as get_chip_unique_serial but in hex-string format */
static int get_chip_unique_serial_str(char *out, size_t len)
{
	uint8_t buf[16];
	int rc;

	if (len < 16*2 + 1)
		return -EINVAL;

	rc = get_chip_unique_serial(buf, sizeof(buf));
	if (rc < 0)
		return rc;
	osmo_hexdump_buf(out, len, buf, sizeof(buf), NULL, false);
	return 0;
}

#define RSTCAUSE_STR_SIZE	64
static void get_rstcause_str(char *out)
{
	uint8_t val = hri_rstc_read_RCAUSE_reg(RSTC);
	*out = '\0';
	if (val & RSTC_RCAUSE_POR)
		strcat(out, "POR ");
	if (val & RSTC_RCAUSE_BODCORE)
		strcat(out, "BODCORE ");
	if (val & RSTC_RCAUSE_BODVDD)
		strcat(out, "BODVDD ");
	if (val & RSTC_RCAUSE_NVM)
		strcat(out, "NVM ");
	if (val & RSTC_RCAUSE_EXT)
		strcat(out, "EXT ");
	if (val & RSTC_RCAUSE_WDT)
		strcat(out, "WDT ");
	if (val & RSTC_RCAUSE_SYST)
		strcat(out, "SYST ");
	if (val & RSTC_RCAUSE_BACKUP)
		strcat(out, "BACKUP ");
}

int main(void)
{
	char sernr_buf[16*2+1];
	char rstcause_buf[RSTCAUSE_STR_SIZE];

	atmel_start_init();
	get_chip_unique_serial_str(sernr_buf, sizeof(sernr_buf));
	get_rstcause_str(rstcause_buf);

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
	command_register(&cmd_talloc_test);
	command_register(&cmd_talloc_report);
	command_register(&cmd_talloc_free);
	command_register(&cmd_get_time);
	command_register(&cmd_test_timer);

	printf("\r\n\r\n"
		"=============================================================================\n\r"
		"sysmoOCTSIM firmware " GIT_VERSION "\n\r"
		"(C) 2018-2019 by sysmocom - s.f.m.c. GmbH and contributors\n\r"
		"=============================================================================\n\r");
	printf("Chip ID: %s\r\n", sernr_buf);
	printf("Reset cause: %s\r\n", rstcause_buf);

	talloc_enable_null_tracking();
	g_tall_ctx = talloc_named_const(NULL, 0, "global");
	printf("g_tall_ctx=%p\r\n", g_tall_ctx);

	libosmo_emb_init();

	LOGP(DUSB, LOGL_ERROR, "foobar usb\n");

	command_print_prompt();
	while (true) { // main loop
		command_try_recv();
		poll_card_detect();
		osmo_timers_update();
	}
}
