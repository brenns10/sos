/**
 * dhcp.c: Stupid DHCP implementation, activated by kernel shell command
 *
 * DHCP is probably not something which ought to be implemented by an operating
 * system kernel. But the alternative is building a lot more infrastructure to
 * support DHCP in userspace, which is not ideal for this OS at this time. So,
 * we implement it in kernel.
 *
 * This relies a bit on how network initialization works in SOS. Currently, the
 * "network interface" structure (struct netif) initializes the IP, gateway,
 * subnet, and dns fields to 0, and in the IP layer, we record static mappings
 * for the IP broadcast address 255.255.255.255 to map it to the MAC
 * ff:ff:ff:ff:ff:ff. This means that when we send a DHCP packet via IP it gets
 * properly handled down the chain.
 */
#include "kernel.h"
#include "net.h"
#include "string.h"

struct dhcp_data {
	uint32_t server_identifier;
	uint32_t subnet_mask;
	uint32_t router;
	uint32_t lease_time;
	uint32_t dns;
};

static int dhcp_discover(struct netif *netif)
{
	struct dhcp *dhcp;
	struct dhcp_option *dtype;
	int space = udp_reserve();
	struct packet *pkt = packet_alloc();
	pkt->al = (void *)pkt->data + space;

	dhcp = (struct dhcp *)pkt->al;
	dhcp->op = BOOTREQUEST;
	dhcp->htype = DHCP_HTYPE_ETHERNET;
	dhcp->hlen = 6; /* mac address = 6 bytes */
	dhcp->xid = htonl(0xDEADBEEF);
	dhcp->cookie = htonl(DHCP_MAGIC_COOKIE);
	memcpy(&dhcp->chaddr, &netif->mac, 6);

	dtype = (struct dhcp_option *)&dhcp->options;
	dtype->tag = DHCPOPT_MSG_TYPE;
	dtype->len = 1;
	dtype->data[0] = DHCPMTYPE_DHCPDISCOVER;
	dhcp->options[3] = 255; /* END option */
	pkt->end = (void *)&dhcp->options[4];

	udp_send(netif, pkt, 0, 0xFFFFFFFF, UDPPORT_DHCP_CLIENT,
	         UDPPORT_DHCP_SERVER);
}

static int dhcp_parse_offer(struct dhcp *dhcp, uint32_t len,
                            struct dhcp_data *opts)
{
	struct dhcp_option *opt = (struct dhcp_option *)&dhcp->options;
	uint32_t offset = sizeof(struct dhcp);
	uint32_t incr;

	opts->subnet_mask = 0;
	opts->router = 0;
	opts->dns = 0;
	opts->lease_time = 0;
	opts->server_identifier = 0;
	while (opt->tag != 255 && offset < len) {
		if (opt->tag != DHCPOPT_PAD)
			incr = opt->len + 2;
		else
			incr = 1;
		switch (opt->tag) {
		case DHCPOPT_PAD:
			break;
		case DHCPOPT_SUBNET_MASK:
			opts->subnet_mask = *(uint32_t *)&opt->data;
			break;
		case DHCPOPT_ROUTER:
			opts->router = *(uint32_t *)&opt->data;
			break;
		case DHCPOPT_DOMAIN_NAME_SERVER:
			opts->dns = *(uint32_t *)&opt->data;
			break;
		case DHCPOPT_LEASE_TIME:
			opts->lease_time = *(uint32_t *)&opt->data;
			break;
		case DHCPOPT_SERVER_IDENTIFIER:
			opts->server_identifier = *(uint32_t *)&opt->data;
			break;
		default:
			break;
		}
		offset += incr;
		opt = (struct dhcp_option *)((void *)opt + incr);
	}
	if (opts->subnet_mask && opts->router && opts->dns &&
	    opts->lease_time && opts->server_identifier) {
		return 0;
	}
	return -1;
}

struct packet *dhcp_handle_offer(struct netif *netif, struct packet *pkt)
{
	struct dhcp *dhcp = (struct dhcp *)pkt->al;
	struct dhcp *dhcpr;
	struct dhcp_data offer;
	struct dhcp_option *opt;
	struct packet *pktr;
	int optidx = 0;
	int rv, dhcplen;
	if (dhcp->op != BOOTREPLY) {
		printf("error: wrong op for DHCP reply, expected BOOTREPLY "
		       "(%u), got %u",
		       BOOTREPLY, dhcp->op);
		return NULL;
	}

	if (dhcp->options[0] != DHCPOPT_MSG_TYPE) {
		printf("error: expectected first option to be message type "
		       "(53), got %u\n",
		       dhcp->options[0]);
		return NULL;
	}
	if (dhcp->options[2] != DHCPMTYPE_DHCPOFFER) {
		printf("error: expected DHCP message type OFFER, got %u\n",
		       dhcp->options[2]);
		return NULL;
	}
	printf("receive offer from %I: %I\n", pkt->ip->dst,
	       *(uint32_t *)&dhcp->yiaddr);

	dhcplen = ntohs(pkt->udp->len) - sizeof(struct udphdr);
	rv = dhcp_parse_offer(dhcp, dhcplen, &offer);
	if (rv < 0)
		return NULL;

	pktr = packet_alloc();
	pktr->al = pktr->data + udp_reserve();
	dhcpr = (struct dhcp *)pktr->al;
	dhcpr->op = BOOTREQUEST;
	dhcpr->htype = DHCP_HTYPE_ETHERNET;
	dhcpr->hlen = 6; /* mac address = 6 bytes */
	dhcpr->xid = htonl(0xDEADBEEF);
	dhcpr->cookie = htonl(DHCP_MAGIC_COOKIE);
	dhcpr->secs = dhcp->secs;
	memcpy(&dhcpr->chaddr, &netif->mac, 6);
	*(uint32_t *)&dhcpr->yiaddr = *(uint32_t *)&dhcp->yiaddr;

	optidx = 0;
	opt = (struct dhcp_option *)&dhcpr->options[optidx];
	opt->tag = DHCPOPT_MSG_TYPE;
	opt->len = 1;
	opt->data[0] = DHCPMTYPE_DHCPREQUEST;
	optidx += opt->len + sizeof(struct dhcp_option);

	opt = (struct dhcp_option *)&dhcpr->options[optidx];
	opt->tag = DHCPOPT_SERVER_IDENTIFIER;
	opt->len = 4;
	*(uint32_t *)&opt->data = offer.server_identifier;
	optidx += opt->len + sizeof(struct dhcp_option);
	pktr->end = (void *)&dhcpr->options[optidx];
	return pktr;
}

int dhcp_handle_ack(struct netif *netif, struct packet *pkt)
{
	struct dhcp *dhcp = (struct dhcp *)pkt->al;
	struct dhcp_data offer;
	int rv, dhcplen;

	if (dhcp->op != BOOTREPLY) {
		printf("error: wrong op for DHCP reply, expected BOOTREPLY "
		       "(%u), got %u",
		       BOOTREPLY, dhcp->op);
		return -1;
	}
	if (dhcp->options[0] != DHCPOPT_MSG_TYPE) {
		printf("error: expectected first option to be message type "
		       "(53), got %u\n",
		       dhcp->options[0]);
		return -1;
	}
	if (dhcp->options[2] != DHCPMTYPE_DHCPACK) {
		printf("error: expected DHCP message type ACK (%u), got %u\n",
		       DHCPMTYPE_DHCPACK, dhcp->options[2]);
		return -1;
	}
	printf("receive ack from %I: %I\n", pkt->ip->dst,
	       *(uint32_t *)&dhcp->yiaddr);

	dhcplen = ntohs(pkt->udp->len) - sizeof(struct udphdr);
	rv = dhcp_parse_offer(dhcp, dhcplen, &offer);
	if (rv < 0)
		return NULL;

	netif->ip = *(uint32_t *)&dhcp->yiaddr;
	netif->gateway_ip = offer.router;
	netif->subnet_mask = offer.subnet_mask;
	netif->dns = offer.dns;

	printf("netif is configured with ip %I, gateway %I, subnet %I, dns "
	       "%I\n",
	       netif->ip, netif->gateway_ip, netif->subnet_mask, netif->dns);
	return 0;
}

int dhcp_cmd_discover(int argc, char **argv)
{
	struct packet *offer, *reply, *ack;

	puts("Send DHCP discover packet and wait for response...\n");
	interrupt_disable();
	dhcp_discover(&nif);
	offer = udp_wait(UDPPORT_DHCP_CLIENT); /* interrupts re-enabled */

	puts("Received response, handling it...\n");
	reply = dhcp_handle_offer(&nif, offer);
	packet_free(offer);
	if (!reply)
		return -1;

	puts("Send DHCP request and wait for response...\n");

	interrupt_disable();
	udp_send(&nif, reply, 0, 0xFFFFFFFF, UDPPORT_DHCP_CLIENT,
	         UDPPORT_DHCP_SERVER);
	ack = udp_wait(UDPPORT_DHCP_CLIENT); /* interrupts re-enabled */

	dhcp_handle_ack(&nif, ack);
	packet_free(ack);
}
