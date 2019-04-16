#pragma once

struct command_fn {
	const char *command;
	const char *help;
	void (*fn)(int argc, char **argv);
};

#define DEFUN(funcname, cmdname, cmdstr, helpstr)	\
	static void funcname(int argc, char **argv);		\
	static struct command_fn cmdname = { 			\
		.command = cmdstr,				\
		.help = helpstr,				\
		.fn = funcname,					\
	};							\
	static void funcname(int argc, char **argv)

void command_init(const char *prompt);
int command_register(const struct command_fn *cmd);
void command_try_recv(void);
void command_print_prompt(void);
