#pragma once

#include "packets.h"

struct netif {
	uint32_t ip;
	uint8_t mac[6];
	struct virtio_net *dev;

	uint32_t gateway_ip;
	uint32_t subnet_mask;
	uint32_t dns;
};

void eth_recv(struct netif *netif, struct packet *pkt);
int eth_send(struct netif *netif, struct packet *pkt, uint16_t ethertype,
             uint8_t dst_mac[6]);

void ip_recv(struct netif *netif, struct packet *pkt);
/* TODO: maybe netif shouldn't be in ip_send() */
int ip_send(struct netif *netif, struct packet *pkt, uint8_t proto,
            uint32_t src_ip, uint32_t dst_ip);
int ip_reserve(void);

void udp_recv(struct netif *netif, struct packet *pkt);
int udp_send(struct netif *netif, struct packet *pkt, uint32_t src_ip,
             uint32_t dst_ip, uint16_t src_port, uint16_t dst_port);
int udp_reserve(void);

uint32_t ntohl(uint32_t orig);
uint16_t ntohs(uint16_t orig);
uint32_t htonl(uint32_t orig);
uint32_t htons(uint16_t orig);
void csum_init(uint32_t *csum);
void csum_add(uint32_t *csum, uint16_t *data, uint32_t n);
void csum_add_value(uint32_t *csum, uint16_t data);
uint16_t csum_finalize(uint32_t *csum);
