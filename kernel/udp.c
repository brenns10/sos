#include "kernel.h"
#include "net.h"

void udp_recv(struct netif *netif, struct packet *pkt)
{
	printf("udp_recv src=%u dst=%u\n", ntohs(pkt->udp->src_port),
	       ntohs(pkt->udp->dst_port));
}

int udp_send(struct netif *netif, struct packet *pkt, uint32_t src_ip,
             uint32_t dst_ip, uint16_t src_port, uint16_t dst_port)
{
	uint32_t csum;
	pkt->tl = pkt->al - sizeof(struct udphdr);

	pkt->udp->src_port = htons(src_port);
	pkt->udp->dst_port = htons(dst_port);
	pkt->udp->len = htons(pkt->end - pkt->tl);
	pkt->udp->csum = 0;
	csum_init(&csum);
	csum_add(&csum, &src_ip, 2);
	csum_add(&csum, &dst_ip, 2);
	csum_add_value(&csum, ntohs((uint16_t)IPPROTO_UDP));
	csum_add(&csum, &pkt->udp->len, 1);
	csum_add(&csum, (uint16_t *)pkt->udp, ntohs(pkt->udp->len) / 2);
	pkt->udp->csum = csum_finalize(&csum);
	ip_send(netif, pkt, IPPROTO_UDP, src_ip, dst_ip);
}

int udp_reserve(void)
{
	return ip_reserve() + sizeof(struct udphdr);
}
