#include "kernel.h"

void data_abort(void)
{
	uint32_t dfsr, dfar;
	get_cpreg(dfsr, c5, 0, c0, 0);
	get_cpreg(dfar, c6, 0, c0, 0);
	switch (dfsr) {
	case 0x5:
		printf("Translation fault (section): 0x%x\n", dfar);
		break;
	case 0x7:
		printf("Translation fault (page): 0x%x\n", dfar);
		break;
	case 0x9:
		printf("Domain fault (section): 0x%x, domain=%u\n",
				dfar, (dfsr >> 4) & 0xF);
		break;
	case 0xB:
		printf("Domain fault (page): 0x%x, domain=%u\n",
				dfar, (dfsr >> 4) & 0xF);
		break;
	case 0xD:
		printf("Permission fault (section): 0x%x\n", dfar);
		break;
	case 0xF:
		printf("Permission fault (page): 0x%x\n", dfar);
	}
}
