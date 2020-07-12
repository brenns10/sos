#include "sync.h"
#include "kernel.h"
#include "ksh.h"

DECLARE_SPINSEM(sem, 2);

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

static int cmd_strex(int argc, char **argv)
{
	register uint32_t val, res;
	register uint32_t *thesem = &sem;

	if (argc != 1) {
		puts("usage: sync ldrex VALUE\n");
		return -1;
	}
	val = atoi(argv[0]);
	strex(res, val, thesem);
	printf("Stored %d to sem. Result was %d\n", val, res);
	if (res == 1)
		puts(" EXCLUSIVE FAIL\n");
	puts(" be sure to do a strex or clrex\n");
	return 0;
}

static int cmd_ldrex(int argc, char **argv)
{
	register uint32_t val;
	register uint32_t *thesem = &sem;
	ldrex(val, thesem);
	printf("val = %d\n", val);
	puts("be sure to do a strex or clrex soon!\n");
	return 0;
}

static int cmd_clrex(int argc, char **argv)
{
	clrex();
	return 0;
}

static int cmd_trylock(int argc, char **argv)
{
	register uint32_t val, res;
	register uint32_t *thesem = &sem;
	clrex(); // just to be sure
	ldrex(val, thesem);
	if (val == 0) {
		clrex();
		puts("semaphore is busy\n");
		return 0;
	}
	val -= 1;
	strex(res, val, thesem);
	if (res == 1) {
		puts("exclusive write failed\n");
	} else {
		printf("success! value is now %d\n", val);
	}
	return 0;
}

struct ksh_cmd sync_ksh_cmds[] = {
	KSH_CMD("acquire", cmd_acquire, "acquire a semaphore (original = 2)"),
	KSH_CMD("release", cmd_release, "release the semaphore"),
	KSH_CMD("strex", cmd_strex, "do store exclusive on the semaphore"),
	KSH_CMD("ldrex", cmd_ldrex, "do load exclusive on the semaphore"),
	KSH_CMD("clrex", cmd_clrex, "clear exclusive access"),
	KSH_CMD("trylock", cmd_trylock, "try to acquire"),
	{ 0 },
};
