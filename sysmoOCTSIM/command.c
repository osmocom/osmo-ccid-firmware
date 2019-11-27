#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <utils.h>

#include "atmel_start.h"
#include "command.h"

struct cmd_state {
	const char *prompt;
	char buf[128];
	unsigned int buf_idx;
	const struct command_fn *cmd[32];
	unsigned int cmd_idx;
};

static struct cmd_state g_cmds;

int command_register(const struct command_fn *cmd)
{
	if (g_cmds.cmd_idx >= ARRAY_SIZE(g_cmds.cmd))
		return -1;
	g_cmds.cmd[g_cmds.cmd_idx++] = cmd;
	return 0;
}

DEFUN(help, help_cmd, "help", "Print command reference")
{
	unsigned int i;
	printf("Help:\r\n");
	printf(" Command          Help\r\n");
	printf(" ---------------- ----\r\n");
	for (i = 0; i < g_cmds.cmd_idx; i++)
		printf(" %-16s %s\r\n", g_cmds.cmd[i]->command, g_cmds.cmd[i]->help);
}

static void cmd_execute()
{
	char *argv[16];
	unsigned int i;
	int argc = 0;
	char *cur;

	printf("\r\n");
	memset(argv, 0, sizeof(argv));

	for (cur = strtok(g_cmds.buf, " "); cur; cur = strtok(NULL, " ")) {
		if (argc >= ARRAY_SIZE(argv))
			break;
		argv[argc++] = cur;
	}

	for (i = 0; i < g_cmds.cmd_idx; i++) {
		if (!strcmp(g_cmds.cmd[i]->command, argv[0])) {
			g_cmds.cmd[i]->fn(argc, argv);
			return;
		}
	}
	printf("Unknown command: '%s'\r\n", argv[0]);
}

static void cmd_buf_reset(void)
{
	memset(g_cmds.buf, 0, sizeof(g_cmds.buf));
	g_cmds.buf_idx = 0;
}

static void cmd_buf_append(char c)
{
	g_cmds.buf[g_cmds.buf_idx++] = c;
}

void command_print_prompt(void)
{
	printf(g_cmds.prompt);
}

void command_try_recv(void)
{
#ifdef ENABLE_DBG_UART7
	unsigned int i = 0;

	/* yield CPU after maximum of 10 received characters */
	while (usart_async_rings_is_rx_not_empty(&UART_debug) && (i < 10)) {
		int c = getchar();
		if (c < 0)
			return;
		if (c == '\r' || c == '\n' || g_cmds.buf_idx >= sizeof(g_cmds.buf)-1) {
			/* skip empty commands */
			if (g_cmds.buf_idx == 0)
				return;
			cmd_execute();
			cmd_buf_reset();
			printf(g_cmds.prompt);
			return;
		} else {
			/* print + append character */
			putchar(c);
			cmd_buf_append(c);
		}

		i++;
	}
#endif
}

void command_init(const char *prompt)
{
	g_cmds.prompt = prompt;
	command_register(&help_cmd);
}
