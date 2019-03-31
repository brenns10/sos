/*
 * salutations.c: a simple first process
 */
#include "syscall.h"
#include "format.h"

int main()
{
	display("Salutations world #1!\n");
	relinquish();

	display("Salutations world #2!\n");
	relinquish();

	display("Salutations world #3!\n");
	relinquish();

	return 0;
}
