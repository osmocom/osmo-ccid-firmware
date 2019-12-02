#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/application.h>
#include <osmocom/core/msgb.h>
#include <osmocom/sim/sim.h>

#include "logging.h"
#include "cuart.h"
#include "iso7816_fsm.h"

static struct card_uart g_cuart;

enum test_state {
	ST_WAIT_ATR,
	ST_ATR_DONE,
	ST_IN_TPDU,
};
static enum test_state g_tstate = ST_WAIT_ATR;

static void fsm_user_cb(struct osmo_fsm_inst *fi, int event, int cause, void *data)
{
	printf("Handle FSM User Event %d: cause=%d, data=%p\n", event, cause, data);
	switch (event) {
	case ISO7816_E_ATR_DONE_IND:
		g_tstate = ST_ATR_DONE;
		break;
	case ISO7816_E_TPDU_DONE_IND:
		printf("======= TPDU: %s\n", msgb_hexdump(data));
		msgb_free(data);
		g_tstate = ST_ATR_DONE;
	default:
		break;
	}
}

static void *g_tall_ctx;

static void signal_handler(int signal)
{
	switch (signal) {
	case SIGUSR1:
		talloc_report_full(g_tall_ctx, stderr);
		break;
	}
}

int main(int argc, char **argv)
{
	struct osmo_fsm_inst *fi;
	uint8_t atr[64];
	int rc;

	g_tall_ctx = talloc_named_const(NULL, 0, "main");
	msgb_talloc_ctx_init(g_tall_ctx, 0);
	osmo_init_logging2(g_tall_ctx, &log_info);
	osmo_fsm_log_addr(false);

	signal(SIGUSR1, &signal_handler);

	if (argc < 2) {
		fprintf(stderr, "You must specify the UART tty device as argument\n");
		exit(2);
	}

	printf("Opening UART device %s\n", argv[1]);

	rc = card_uart_open(&g_cuart, "tty", argv[1]);
	if (rc < 0) {
		perror("opening UART");
		exit(1);
	}

	fi = iso7816_fsm_alloc(NULL, LOGL_DEBUG, "SIM0", &g_cuart, fsm_user_cb, NULL);
	OSMO_ASSERT(fi);

	/* activate reset, then power up */
	card_uart_ctrl(&g_cuart, CUART_CTL_RST, true);
	card_uart_ctrl(&g_cuart, CUART_CTL_POWER, true);
	osmo_fsm_inst_dispatch(fi, ISO7816_E_POWER_UP_IND, NULL);

	/* activate clock */
	card_uart_ctrl(&g_cuart, CUART_CTL_CLOCK, true);

	/* wait some time and release reset */
	usleep(10000);
	card_uart_ctrl(&g_cuart, CUART_CTL_RST, false);
	osmo_fsm_inst_dispatch(fi, ISO7816_E_RESET_REL_IND, NULL);

	/* process any events in polling mode for initial change */
	osmo_select_main(1);

	struct msgb *apdu;
	while (1) {
		/* check if the new state requires us to do something */
		switch (g_tstate) {
		case ST_ATR_DONE:
			apdu = msgb_alloc(512, "TPDU");
			msgb_put_u8(apdu, 0x00);
			msgb_put_u8(apdu, 0xa4);
			msgb_put_u8(apdu, 0x00);
			msgb_put_u8(apdu, 0x04);
			msgb_put_u8(apdu, 0x02);
			msgb_put_u8(apdu, 0x2f);
			msgb_put_u8(apdu, 0x00);
			osmo_fsm_inst_dispatch(fi, ISO7816_E_XCEIVE_TPDU_CMD, apdu);
			g_tstate = ST_IN_TPDU;
			break;
		default:
			break;
		}
		osmo_select_main(0);
	}

	exit(0);
}
