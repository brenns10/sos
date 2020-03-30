#include "kernel.h"
#include "net.h"
#include "string.h"

uint8_t broadcast_mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define MAC_SIZE 6

void eth_recv(struct netif *netif, struct packet *pkt)
{
	printf("eth_recv src=%M dst=%M ethertype=0x%x\n", &pkt->eth->src_mac,
	       &pkt->eth->dst_mac, ntohs(pkt->eth->ethertype));
	pkt->nl = pkt->ll + sizeof(struct etherframe);
	if (memcmp(pkt->eth->dst_mac, broadcast_mac, MAC_SIZE) == 0 ||
	    memcmp(pkt->eth->dst_mac, netif->mac, MAC_SIZE) == 0) {
		switch (ntohs(pkt->eth->ethertype)) {
		case ETHERTYPE_IP:
			ip_recv(netif, pkt);
			break;
		}
	} else {
		puts("received ethernet packet not destined for us\n");
	}
}

int eth_send(struct netif *netif, struct packet *pkt, uint16_t ethertype,
             uint8_t dst_mac[MAC_SIZE])
{
	pkt->ll = pkt->nl - sizeof(struct etherframe);
	if (pkt->ll < (void *)&pkt->data) {
		return -1;
	}

	memcpy(pkt->eth->src_mac, netif->mac, MAC_SIZE);
	memcpy(pkt->eth->dst_mac, dst_mac, MAC_SIZE);
	pkt->eth->ethertype = htons(ethertype);

	/* TODO: how do we recover struct netdev after tx? */
	virtio_net_send(netif->dev, pkt->ll, (pkt->end - pkt->ll));
	return 0;
}
