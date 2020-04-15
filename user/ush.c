/**
 * User Shell
 */
#include "format.h"
#include "string.h"
#include "sys/socket.h"
#include "syscall.h"

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

static int cmd_exit(int argc, char **argv)
{
	exit(0);
}

static int cmd_run(int argc, char **argv)
{
	int rv;
	if (argc != 2) {
		puts("usage: run PROCNAME\n");
		return -1;
	}

	if ((rv = runproc(argv[1])) != 0)
		printf("failed: rv=0x%x\n", rv);
}

static int cmd_socket(int argc, char **argv)
{
	int rv;

	rv = socket(AF_INET, SOCK_DGRAM, 0);
	printf("socket() = %d\n", rv);
	return rv;
}

static int cmd_bind(int argc, char **argv)
{
	int rv, sockfd;
	struct sockaddr_in addr;

	if (argc != 4) {
		puts("usage: bind FD IP_AS_INT PORT\n");
		return -1;
	}

	sockfd = atoi(argv[1]);
	addr.sin_addr.s_addr = atoi(argv[2]);
	addr.sin_port = atoi(argv[3]);
	rv = bind(sockfd, &addr, sizeof(struct sockaddr_in));
	printf("bind() = %d\n", rv);
	return rv;
}

static int cmd_demo(int argc, char **argv)
{
	int i;
	for (i = 0; i < 10; i++)
		runproc("hello");
	puts("Launched all processes!\n");
}

static int help(int argc, char **argv);
struct cmd cmds[] = {
	{ .name = "echo",
	  .func = echo,
	  .help = "print each arg, useful for debugging" },
	{ .name = "help", .func = help, .help = "show this help message" },
	{ .name = "run", .func = cmd_run, .help = "run a process" },
	{ .name = "demo", .func = cmd_demo, .help = "run many processes" },
	{ .name = "socket", .func = cmd_socket, .help = "create socket" },
	{ .name = "bind", .func = cmd_bind, .help = "bind socket" },
	{ .name = "exit", .func = cmd_exit, .help = "exit this process" },
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

	puts("ush> ");

	do {
		input[i++] = getchar();
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

int main(void)
{
	int pid = getpid();
	printf("\nStephen's OS (user shell, pid=%u)\n", pid);
	puts("  This shell doesn't do much. Use the \"exit\" command to drop\n"
	     "  into a kernel shell which can do scary things!\n\n");
	while (true) {
		getline();
		tokenize();
		execute();
	}
	return 0;
}
