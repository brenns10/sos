/*
 * ksh.h: kernel shell
 */

typedef int (*ksh_func)(int argc, char **argv);

struct ksh_cmd {
	char *name;
	char *help;
	enum {
		/* nofmt */
		KSH_EMPTY,
		KSH_FUNC,
		KSH_SUB,
	} kind;
	union {
		ksh_func func;
		struct ksh_cmd *sub;
	};
};

#define KSH_CMD(in_name, in_func, in_help)                                     \
	{                                                                      \
		.name = in_name, .help = in_help, .kind = KSH_FUNC,            \
		.func = in_func                                                \
	}
#define KSH_SUB(in_name, in_sub, in_help)                                      \
	{                                                                      \
		.name = in_name, .help = in_help, .kind = KSH_SUB,             \
		.sub = in_sub                                                  \
	}

/**
 * Run the kernel shell.
 */
void ksh(void *arg);
#define KSH_SPIN  ((void *)1)
#define KSH_BLOCK ((void *)2)
