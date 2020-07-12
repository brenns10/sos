/**
 * Kernel Shell
 *
 * This shell is implemented as a kernel thread. This allows the shell to make
 * system calls, sleep as it waits for user input, etc. However, it may still
 * access kernel memory space. This is  S C A R Y  everyone, but we'll get used
 * to it.
 */
#include "cxtk.h"
#include "fat.h"
#include "kernel.h"
#include "slab.h"
#include "string.h"
#include "sync.h"

#include "ksh.h"

static char input[256];
static char *tokens[16];
DECLARE_SPINSEM(sem, 2);
struct ctx kshctx;

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
	_spin_acquire(&sem);
	return 0;
}

static int cmd_release(int argc, char **argv)
{
	_spin_release(&sem);
	return 0;
}

static int cmd_slab_report(int argc, char **argv)
{
	slab_report_all();
	return 0;
}

static int cmd_cxtk_report(int argc, char **argv)
{
	cxtk_report();
	return 0;
}

static int cmd_resctx(int argc, char **argv)
{
	uint32_t val;
	if (argc != 1) {
		puts("usage: resctx VALUE_INTEGER\n");
		return 1;
	}
	val = (uint32_t)atoi(argv[0]);
	resctx(val, &kshctx);
	return 0;
}

static int cmd_udiv(int argc, char **argv)
{
	uint32_t dividend, divisor, result, remainder;
	if (argc != 2) {
		puts("usage: udiv DIVIDEND DIVISOR\n");
	}
	dividend = atoi(argv[0]);
	divisor = atoi(argv[1]);
	result = dividend / divisor;
	remainder = dividend % divisor;
	printf("= %u (rem %u)\n", result, remainder);
	return 0;
}

static int cmd_sdiv(int argc, char **argv)
{
	int32_t dividend, divisor, result, remainder;
	if (argc != 2) {
		puts("usage: sdiv DIVIDEND DIVISOR\n");
	}
	dividend = atoi(argv[0]);
	divisor = atoi(argv[1]);
	result = dividend / divisor;
	remainder = dividend % divisor;
	printf("= %d (rem %d)\n", result, remainder);
	return 0;
}

static int help(int argc, char **argv);
struct ksh_cmd cmds[] = {
	KSH_CMD("echo", echo, "print each arg, useful for debugging"),
	KSH_CMD("mkproc", cmd_mkproc,
	        "create a new process with binary image IMG"),
	KSH_CMD("lsproc", cmd_lsproc, "list process IDs"),
	KSH_CMD("execproc", cmd_execproc, "run process PID"),
	KSH_CMD("acquire", cmd_acquire, "acquire sem"),
	KSH_CMD("release", cmd_release, "release sem"),
	KSH_CMD("dtb-ls", cmd_dtb_ls, "list device tree nodes"),
	KSH_CMD("dtb-prop", cmd_dtb_prop, "show properties for a node"),
	KSH_CMD("dtb-dump", cmd_dtb_dump, "dump the whole damn dtb"),
	KSH_CMD("timer-get-freq", cmd_timer_get_freq, "get timer frequency"),
	KSH_CMD("timer-get-count", cmd_timer_get_count,
	        "get current timer value"),
	KSH_CMD("timer-get-ctl", cmd_timer_get_ctl,
	        "get current timer ctl register"),
	KSH_CMD("blkread", blk_cmd_read, "read block device sector"),
	KSH_CMD("blkwrite", blk_cmd_write, "write block device sector"),
	KSH_CMD("blkstatus", blk_cmd_status, "read block device status"),
	KSH_CMD("netstatus", virtio_net_cmd_status, "read net device status"),
	KSH_CMD("dhcpdiscover", dhcp_cmd_discover, "send DHCPDISCOVER"),
	KSH_CMD("help", help, "show this help message"),
	KSH_CMD("show-arptable", ip_cmd_show_arptable, "show the arp table"),
	KSH_CMD("slab-report", cmd_slab_report, "print all slab stats"),
	KSH_CMD("cxtk", cmd_cxtk_report, "print context switch report"),
	KSH_CMD("resctx", cmd_resctx, "demo for setctx/resctx"),
	KSH_CMD("fat", cmd_fat, "print fat info"),
	KSH_CMD("fatcat", cmd_fatcat, "print fat file"),
	KSH_CMD("udiv", cmd_udiv, "unsigned division"),
	KSH_CMD("sdiv", cmd_sdiv, "signed division"),
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
static void getline(void *arg)
{
	unsigned int i = 0;

	puts("ksh> ");

	do {
		if (arg == KSH_SPIN)
			input[i++] = getc_spinning();
		else
			input[i++] = getc_blocking();
		putc(input[i - 1]);
	} while (input[i - 1] != '\r' && i < sizeof(input));
	putc('\n');
	input[i - 1] = '\0';
}

static int tokenize(void)
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
	return tok;
}

struct ksh_lookup_res {
	struct ksh_cmd *cmd;
	int level;
	enum {
		/* nofmt */
		KSHRES_FOUND,
		KSHRES_NOTFOUND,
		KSHRES_INCOMPLETE,
	} status;
};

static struct ksh_lookup_res ksh_lookup(struct ksh_cmd *cmds, int argc,
                                        char **argv)
{
	unsigned int cmdidx;
	struct ksh_lookup_res res;

	/* Outer loop: each iteration means we're another level deeper into the
	 * nested menus.
	 */
	for (res.level = 0; res.level < argc; res.level++) {
		/* Inner loop, iterate over items within this menu.
		 */
		for (cmdidx = 0; cmds[cmdidx].kind != KSH_EMPTY; cmdidx++) {
			if (strcmp(argv[res.level], cmds[cmdidx].name) != 0)
				continue; /* inner loop */

			if (cmds[cmdidx].kind == KSH_FUNC) {
				res.cmd = &cmds[cmdidx];
				res.status = KSHRES_FOUND;
				return res;
			} else {
				/* Go to the next level of the outer loop */
				cmds = cmds[cmdidx].sub;
				break;
			}
		}

		/* If we're not at the end of this menu, that means we should
		 * descend another level into the outer loop */
		if (cmds[cmdidx].kind != KSH_EMPTY)
			continue; /* outer loop */

		/* Command not found in this menu. Return the menu we're
		 * currently in, its level, and and error code. */
		res.cmd = cmds;
		res.status = KSHRES_NOTFOUND;
		return res;
	}

	/* If we're here, we got to the end of the command, but we didn't find a
	 * leaf item in the menu. */
	res.cmd = cmds;
	res.status = KSHRES_INCOMPLETE;
	return res;
}

static int execute(struct ksh_cmd *cmds, int argc, char **argv)
{
	struct ksh_lookup_res res = ksh_lookup(cmds, argc, argv);

	if (res.status != KSHRES_FOUND) {
		if (res.status == KSHRES_NOTFOUND) {
			puts("command not found\n");
		} else {
			puts("incomplete command\n");
		}
		return -1;
	} else {
		/* Since sub-menus won't know how deep they are, we can't
		 * include the command itself in the arguments. Set argc and
		 * argv accordingly. */
		return res.cmd->func(argc - res.level - 1,
		                     argv + res.level + 1);
	}
}

void ksh(void *arg)
{
	int rv, argc;
	if ((rv = setctx(&kshctx)) != 0) {
		printf("I have been bamboozled!\nYou sent %d\n", rv);
	}
	puts("Stephen's OS, (kernel shell)\n");
	while (true) {
		getline(arg);
		argc = tokenize();
		execute(cmds, argc, tokens);
	}
}
