/**
 * Kernel Shell
 */
#include "kernel.h"
#include "string.h"

static char input[256];
static char *tokens[16];
static int argc;

/*
 * Shell commands section. Each command is represented by a struct cmd, and
 * should have an implementation below, followed by an entry in the cmds array.
 */
struct cmd {
	char *name;
	char *help;
	int (*func)(int argc, char **argv);
};

static int echo(int argc, char **argv)
{
	for (unsigned int i = 0; argv[i]; i++)
		printf("Arg[%u]: \"%s\"\n", i, argv[i]);
	return 0;
}

static int help(int argc, char **argv);
struct cmd cmds[] = {
	{.name="echo", .func=echo, .help="print each arg, useful for debugging"},
	{.name="mkproc", .func=cmd_mkproc, .help="create a new process with binary image IMG"},
	{.name="lsproc", .func=cmd_lsproc, .help="list process IDs"},
	{.name="execproc", .func=cmd_execproc, .help="run process PID"},
	{.name="dtb-ls", .func=cmd_dtb_ls, .help="list device tree nodes"},
	{.name="dtb-prop", .func=cmd_dtb_prop, .help="show properties for a node"},
	{.name="dtb-dump", .func=cmd_dtb_dump, .help="dump the whole damn dtb"},
	{.name="help", .func=help, .help="show this help message"},
};

/*
 * help() goes after the cmds array so it can print out a listing
 */
static int help(int argc, char **argv)
{
	for (unsigned int i = 0; i < nelem(cmds); i++)
		printf("%s:\t%s\n", cmds[i].name, cmds[i].help);
	return 0;
}

/*
 * Parsing logic
 */
static void getline(void)
{
	unsigned int i = 0;

	puts("ksh> ");

	do {
		input[i++] = getc();
		putc(input[i-1]);
	} while (input[i-1] != '\r' && i < sizeof(input));
	putc('\n');
	input[i-1] = '\0';
}

static void tokenize(void)
{
	unsigned int start = 0, tok=0, i;
	for (i = 0; input[i]; i++) {
		if (input[i] == ' ' || input[i] == '\t' || input[i] == '\r' ||
				input[i] == '\n') {
			if (i != start) {
				/* only complete a token if non-empty */
				tokens[tok++] = &input[start];
				input[i] = '\0';
			}
			start = i+1;
		}
	}
	if (i != start) {
		tokens[tok++] = &input[start];
	}
	tokens[tok] = NULL;
	argc = tok;
}

static void execute(void)
{
	for (unsigned int i = 0; i < nelem(cmds); i++) {
		if (strcmp(tokens[0], cmds[i].name) == 0) {
			cmds[i].func(argc, tokens);
			return;
		}
	}
	printf("command not found: \"%s\"\n", tokens[0]);
}

void ksh(void)
{
	puts("Stephen's OS, v" SOS_VERSION "\n");
	while (true) {
		getline();
		tokenize();
		execute();
	}
}
