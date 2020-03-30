/*
 * salutations.c: a simple first process
 */
#include "format.h"
#include "syscall.h"

int main()
{
	int pid = getpid();
	printf("[pid=%u] Salutations world #1!\n", pid);
	relinquish();

	printf("[pid=%u] Salutations world #2!\n", pid);
	relinquish();

	printf("[pid=%u] Salutations world #3!\n", pid);
	relinquish();

	return 0;
}
