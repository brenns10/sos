/**
 * Kernel Shell
 *
 * This shell is implemented as a kernel thread. This allows the shell to make
 * system calls, sleep as it waits for user input, etc. However, it may still
 * access kernel memory space. This is  S C A R Y  everyone, but we'll get used
 * to it.
 */
#include "kernel.h"
#include "string.h"
#include "sync.h"

static char input[256];
static char *tokens[16];
static int argc;
DECLARE_SPINSEM(sem, 2);

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

static int cmd_acquire(int argc, char **argv)
{
	spin_acquire(&sem);
}

static int cmd_release(int argc, char **argv)
{
	spin_release(&sem);
}

static int help(int argc, char **argv);
struct cmd cmds[] = {
	{ .name = "echo",
	  .func = echo,
	  .help = "print each arg, useful for debugging" },
	{ .name = "mkproc",
	  .func = cmd_mkproc,
	  .help = "create a new process with binary image IMG" },
	{ .name = "lsproc", .func = cmd_lsproc, .help = "list process IDs" },
	{ .name = "execproc", .func = cmd_execproc, .help = "run process PID" },
	{ .name = "acquire", .func = cmd_acquire, .help = "acquire sem" },
	{ .name = "release", .func = cmd_release, .help = "release sem" },
	{ .name = "dtb-ls",
	  .func = cmd_dtb_ls,
	  .help = "list device tree nodes" },
	{ .name = "dtb-prop",
	  .func = cmd_dtb_prop,
	  .help = "show properties for a node" },
	{ .name = "dtb-dump",
	  .func = cmd_dtb_dump,
	  .help = "dump the whole damn dtb" },
	{ .name = "timer-get-freq",
	  .func = cmd_timer_get_freq,
	  .help = "get timer frequency" },
	{ .name = "timer-get-count",
	  .func = cmd_timer_get_count,
	  .help = "get current timer value" },
	{ .name = "timer-get-ctl",
	  .func = cmd_timer_get_ctl,
	  .help = "get current timer ctl register" },
	{ .name = "blkread",
	  .func = virtio_blk_cmd_read,
	  .help="read block device sector"},
	{ .name = "blkwrite",
	  .func = virtio_blk_cmd_write,
	  .help="write block device sector"},
	{ .name = "blkstatus",
	  .func = virtio_blk_cmd_status,
	  .help="read block device status"},
	{ .name = "netstatus",
	  .func = virtio_net_cmd_status,
	  .help="read net device status"},
	{ .name = "dhcpdiscover",
	  .func = virtio_net_cmd_dhcpdiscover,
	  .help="send DHCPDISCOVER"},
	{ .name = "help", .func = help, .help = "show this help message" },
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
		input[i++] = getc_blocking();
		putc(input[i - 1]);
	} while (input[i - 1] != '\r' && i < sizeof(input));
	putc('\n');
	input[i - 1] = '\0';
}

static void tokenize(void)
{
	unsigned int start = 0, tok = 0, i;
	for (i = 0; input[i]; i++) {
		if (input[i] == ' ' || input[i] == '\t' || input[i] == '\r' ||
		    input[i] == '\n') {
			if (i != start) {
				/* only complete a token if non-empty */
				tokens[tok++] = &input[start];
				input[i] = '\0';
			}
			start = i + 1;
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

void ksh(void *arg)
{
	(void) arg; /* unused */
	puts("Stephen's OS, v" SOS_VERSION "\n");
	while (true) {
		getline();
		tokenize();
		execute();
	}
}
