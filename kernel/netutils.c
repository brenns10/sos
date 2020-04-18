/* network utilities */
#include "kernel.h"
#include "net.h"
#include "slab.h"
#include "string.h"
#include <stdint.h>

struct slab *pktslab = NULL;
#define PACKET_SIZE 2048

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

void packet_init(void)
{
	pktslab = slab_new("packet", PACKET_SIZE, kmem_get_page);
}

struct packet *packet_alloc(void)
{
	struct packet *pkt = (struct packet *)slab_alloc(pktslab);
	memset(pkt, 0, PACKET_SIZE);
	pkt->capacity = PACKET_CAPACITY;
	return pkt;
}

void packet_free(struct packet *pkt)
{
	slab_free(pktslab, (void *)pkt);
}
