/* network utilities */
#include <stdint.h>

uint32_t ntohl(uint32_t orig)
{
	return ((orig & 0xFF) << 24) | ((orig & 0xFF00) << 8) |
	       ((orig & 0xFF0000) >> 8) | ((orig & 0xFF000000) >> 24);
}

uint16_t ntohs(uint16_t orig)
{
	return ((orig & 0xFF) << 8 | (orig & 0xFF00) >> 8);
}

uint32_t htonl(uint32_t orig)
{
	return ntohl(orig);
}

uint32_t htons(uint16_t orig)
{
	return ntohs(orig);
}

void csum_init(uint32_t *csum)
{
	*csum = 0;
}

void csum_add(uint32_t *csum, uint16_t *data, uint32_t n)
{
	uint32_t i;
	for (i = 0; i < n; i++) {
		*csum += data[i];
	}
}

void csum_add_value(uint32_t *csum, uint16_t data)
{
	csum_add(csum, &data, 1);
}

uint16_t csum_finalize(uint32_t *csum)
{
	uint32_t add;
	while (*csum & 0xFFFF0000) {
		add = (*csum & 0xFFFF0000) >> 16;
		*csum &= 0x0000FFFF;
		*csum += add;
	}
	return ~((uint16_t)*csum);
}
