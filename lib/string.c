/*
 * String utilities
 */
#include "string.h"

uint32_t strlen(char *string)
{
	uint32_t len;
	for (len = 0; string[len]; len++){};
	return len;
}

int strcmp(char *lhs, char *rhs)
{
	uint32_t i;
	for (i = 0; lhs[i] == rhs[i]; i++)
		if (lhs[i] == '\0')
			return 0;
	return lhs[i] - rhs[i];
}
