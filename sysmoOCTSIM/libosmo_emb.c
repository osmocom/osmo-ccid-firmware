/* Integration of libosmocore into our bare-iron embedded target */

#include <string.h>
#include <stdio.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/msgb.h>


extern void *g_tall_ctx;
void *g_msgb_ctx;

/***********************************************************************
 * Timers
 ***********************************************************************/

#include <sys/time.h>
#include "driver_init.h"

volatile uint64_t jiffies;

void SysTick_Handler(void)
{
	jiffies++;
}

int _gettimeofday(struct timeval *tv, void *tz)
{
	tv->tv_sec = jiffies / 1000;
	tv->tv_usec = (jiffies % 1000) * 1000;
	return 0;
}

/***********************************************************************
 * Logging
 ***********************************************************************/

#include "logging.h"
#include <osmocom/core/logging.h>

static const struct log_info_cat log_info_cat[] = {
	[DUSB] = {
		.name = "USB",
		.description = "USB Transport",
		.enabled = 1,
		.loglevel = LOGL_NOTICE,
	},
	[DCCID] = {
		.name = "CCID",
		.description = "USB-CCID Protocol",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DISO7816] = {
		.name = "ISO7816",
		.description = "ISO7816-3 State machines",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DATR] = {
		.name = "ATR",
		.description = "ATR (Answer To Reset) FSM",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DTPDU] = {
		.name = "TPDU",
		.description = "TPDU FSM",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DPPS] = {
		.name = "PPS",
		.description = "PPS (Protocol and Parameter Selection) FSM",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
	[DCARD] = {
		.name = "CARD",
		.description = "Card FSM",
		.enabled = 1,
		.loglevel = LOGL_DEBUG,
	},
};

static const struct log_info log_info = {
	.cat = log_info_cat,
	.num_cat = ARRAY_SIZE(log_info_cat),
};

/* call-back function to the logging framework. We use this instead of the libosmocore
 * provided target, as libosmocore wants to put 4kB on the stack. */
static void stderr_raw_output(struct log_target *target, int subsys, unsigned int level,
			      const char *file, int line, int cont, const char *format, va_list ap)
{
	if (!cont) {
		/* TODO: Timestamp? */
		if (target->print_category)
			fprintf(stderr, "%s ", log_category_name(subsys));

		if (target->print_level)
			fprintf(stderr, "%s ", log_level_str(level));

		if (target->print_category_hex)
			fprintf(stderr, "<%4.4x> ", subsys);

		if (target->print_filename_pos == LOG_FILENAME_POS_HEADER_END) {
			const char *bn;
			switch (target->print_filename2) {
			case LOG_FILENAME_NONE:
				break;
			case LOG_FILENAME_PATH:
				fprintf(stderr, "%s:%d ", file, line);
				break;
			case LOG_FILENAME_BASENAME:
				bn = strrchr(file, '/');
				if (!bn || !bn[1])
					bn = file;
				else
					bn++;
				fprintf(stderr, "%s:%d ", bn, line);
				break;
			}
		}
	}
	vfprintf(stderr, format, ap);
	/* TODO: LOG_FILENAME_POS_LINE_END; we cannot modify the format string here :/ */
	int fmt_len = strlen(format);
	if (fmt_len && format[fmt_len-1] == '\n')
		fputc('\r', stderr);
}

static struct log_target *log_target_create_stderr_raw(void)
{
	struct log_target *target;

	target = log_target_create();
	if (!target)
		return NULL;

	target->type = LOG_TGT_TYPE_STDERR;
	target->raw_output = stderr_raw_output;
	target->print_category = true;
	target->print_level = true;

	return target;
}

void libosmo_emb_init(void)
{
	struct log_target *stderr_target;

	/* msgb */
	g_msgb_ctx = talloc_pool(g_tall_ctx, 20480);
	talloc_set_memlimit(g_msgb_ctx, 20480);
	msgb_talloc_ctx_init(g_msgb_ctx, 0);

	/* logging */
	log_init(&log_info, g_tall_ctx);
	stderr_target = log_target_create_stderr_raw();
	log_add_target(stderr_target);
	log_set_all_filter(stderr_target, 1);

	/* timer */
	SysTick_Config(SystemCoreClock / 1000);
}
