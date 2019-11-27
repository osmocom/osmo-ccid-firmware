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
#include <osmocom/core/timer.h>

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

#include "ccid_device.h"
#include "usb_descriptors.h"
extern struct ccid_slot_ops iso_fsm_slot_ops;
static struct ccid_instance g_ci;


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
static volatile struct ccid_state g_ccid_s;

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

	if (ep_q->in_progress)
		return 0;

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

	if (ep_q->in_progress)
		return 0;

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
	msgb_reset(msg);
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
	g_ccid_s.out_ep.in_progress = NULL;

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
static struct msgb *ccid_gen_notify_slot_status(uint8_t old_bm, uint8_t new_bm)
{
	uint8_t statusbytes[2] = {0};
	//struct msgb *msg = ccid_msgb_alloc();
	struct msgb *msg = msgb_alloc(300,"IRQ");
	struct ccid_rdr_to_pc_notify_slot_change *nsc = msgb_put(msg, sizeof(*nsc) + sizeof(statusbytes));
	nsc->bMessageType = RDR_to_PC_NotifySlotChange;

	for(int i = 0; i <8; i++) {
		uint8_t byteidx = i >> 2;
		uint8_t old_bit = old_bm & (1 << i);
		uint8_t new_bit = new_bm & (1 << i);
		uint8_t bv;
		if (old_bit == new_bit && new_bit == 0)
			bv = 0x00;
		else if (old_bit == new_bit && new_bit == 1)
			bv = 0x01;
		else if (old_bit != new_bit && new_bit == 0)
			bv = 0x02;
		else
			bv = 0x03;

		statusbytes[byteidx] |= bv << ((i % 4) << 1);
	}

	memcpy(&nsc->bmSlotCCState, statusbytes, sizeof(statusbytes));

	return msg;
}

/* check if any card detect state has changed */
static void poll_card_detect(void)
{
	uint8_t new_mask = 0;
	struct msgb *msg;
	unsigned int i;

	for (i = 0; i < 8; i++){
		bool level = ncn8025_interrupt_level(i);
		new_mask |= level << i;
		g_ci.slot_ops->icc_set_insertion_status(&g_ci.slot[i], level);
	}

	/* notify the user/host about any changes */
	if (g_ccid_s.card_insert_mask != new_mask) {
		printf("CARD_DET 0x%02x -> 0x%02x\r\n",
			g_ccid_s.card_insert_mask, new_mask);
		msg = ccid_gen_notify_slot_status(g_ccid_s.card_insert_mask, new_mask);
		msgb_enqueue_irqsafe(&g_ccid_s.irq_ep.list, msg);

		g_ccid_s.card_insert_mask = new_mask;
	}
}



/***********************************************************************
 * Command Line interface
 ***********************************************************************/


extern void testmode_init(void);
extern void libosmo_emb_init(void);
extern void libosmo_emb_mainloop(void);

#include "talloc.h"
#include "logging.h"

void *g_tall_ctx;


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

//#######################



static uint32_t clock_freqs[] = {
	2500000
};

static uint32_t data_rates[] = {
	6720
};
extern struct usb_desc_collection usb_fs_descs;



static int feed_ccid(void)
{
	struct usb_ep_q *ep_q = &g_ccid_s.out_ep;
	struct msgb *msg;
	int rc;

	msg = msgb_dequeue_irqsafe(&g_ccid_s.out_ep.list);
	if (!msg)
		return -1;

	ccid_handle_out(&g_ci, msg);
	return 1;
}

static int ccid_ops_send_in(struct ccid_instance *ci, struct msgb *msg)
{
	/* add just-received msg to tail of endpoint queue */
	OSMO_ASSERT(msg);

	/* append to list of pending-to-be-handed messages */
	llist_add_tail_at(&msg->list, &g_ccid_s.in_ep.list);
	submit_next_in();
	return 0;
}

static const struct ccid_ops c_ops = {
	.send_in = ccid_ops_send_in,
	.send_int = 0,
};

//#######################

#define NUM_OUT_BUF 7

int main(void)
{
	char sernr_buf[16*2+1];
	char rstcause_buf[RSTCAUSE_STR_SIZE];

#if 0
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk ; //| /* tracing*/
////CoreDebug_DEMCR_MON_EN_Msk; /* mon interupt catcher */


DWT->COMP0 = 0x40003028; /* sercom 0 data */
//DWT->COMP0 = 0x40003428; /* sercom 1 data */
DWT->MASK0 = 0; /* 0 */
DWT->FUNCTION0 = 0; /* has to be 0 for linked address */

DWT->COMP1 = 0xa5;     /* value */
DWT->MASK1 = 0;     /* match all bits */
DWT->FUNCTION1 =    (0b10 << DWT_FUNCTION_DATAVSIZE_Pos) |  /* DATAVSIZE 10 - dword */
#if 1
(0 << DWT_FUNCTION_DATAVADDR0_Pos) | /* Data Match linked addr pointer in COMP0 */
#else
(1 << DWT_FUNCTION_DATAVADDR0_Pos) | /* Data Match on any addr -> own number */
#endif
(1 << DWT_FUNCTION_DATAVMATCH_Pos) | /* DATAVMATCH Enable data comparation */
(0b0111 << DWT_FUNCTION_FUNCTION_Pos);  /* generate a watchpoint event on rw */

#endif

	atmel_start_init();
	get_chip_unique_serial_str(sernr_buf, sizeof(sernr_buf));
	get_rstcause_str(rstcause_buf);

	usb_start();

	board_init();
	command_init("sysmoOCTSIM> ");

	/* boost uart priority by setting all other irqs to uartprio+1 */
	for(int i = 0; i < PERIPH_COUNT_IRQn; i++)
		NVIC_SetPriority(i, 2);
	for(int i = SERCOM0_0_IRQn; i <= SERCOM7_3_IRQn; i++)
		NVIC_SetPriority(i, 1);

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

	//FIXME osmo_emb has a pool?
	msgb_talloc_ctx_init(g_tall_ctx, 0);

	libosmo_emb_init();

	LOGP(DUSB, LOGL_ERROR, "foobar usb\n");

	//prevent spurious interrupts before our driver structs are ready
	CRITICAL_SECTION_ENTER()
	ccid_instance_init(&g_ci, &c_ops, &iso_fsm_slot_ops, &usb_fs_descs.ccid.class,
			   data_rates, clock_freqs, "", 0);

	for(int i =0; i < NUM_OUT_BUF; i++){
		struct msgb *msg = msgb_alloc(300, "ccid");
		OSMO_ASSERT(msg);
		/* return the message back to the queue of free message buffers */
		llist_add_tail_at(&msg->list, &g_ccid_s.free_q);
	}
	submit_next_out();
	CRITICAL_SECTION_LEAVE()
#if 0
	/* CAN_RX */
	gpio_set_pin_function(PIN_PB12, GPIO_PIN_FUNCTION_OFF);
	gpio_set_pin_direction(PIN_PB12, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PIN_PB12, false);

	/* CAN_TX */
	gpio_set_pin_function(PIN_PB13, GPIO_PIN_FUNCTION_OFF);
	gpio_set_pin_direction(PIN_PB13, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PIN_PB13, false);
#endif

//	command_print_prompt();
	while (true) { // main loop
		command_try_recv();
		poll_card_detect();
		submit_next_irq();
		for (int i = 0; i < usb_fs_descs.ccid.class.bMaxSlotIndex; i++){
			g_ci.slot_ops->handle_fsm_events(&g_ci.slot[i], true);
		}
		feed_ccid();
		osmo_timers_update();
		int qs = llist_count_at(&g_ccid_s.free_q);
		if(qs > NUM_OUT_BUF)
			for (int i= 0; i < qs-NUM_OUT_BUF; i++){
				struct msgb *msg = msgb_dequeue_irqsafe(&g_ccid_s.free_q);
				if (msg)
				msgb_free(msg);
			}
		if(qs < NUM_OUT_BUF)
			for (int i= 0; i < NUM_OUT_BUF-qs; i++){
				struct msgb *msg = msgb_alloc(300,"ccid");
				OSMO_ASSERT(msg);
				/* return the message back to the queue of free message buffers */
				llist_add_tail_at(&msg->list, &g_ccid_s.free_q);
			}

	}
}
