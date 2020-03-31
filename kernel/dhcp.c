#include "kernel.h"
#include "net.h"
#include "string.h"

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

static void dhcp_enumerate_options(struct dhcp *dhcp, uint32_t len)
{
	struct dhcp_option *opt = (struct dhcp_option *)&dhcp->options;
	uint32_t offset = sizeof(struct dhcp);
	uint32_t incr;

	while (opt->tag != 255 && offset < len) {
		if (opt->tag != DHCPOPT_PAD)
			incr = opt->len + 2;
		else
			incr = 1;
		switch (opt->tag) {
		case DHCPOPT_PAD:
			break;
		case DHCPOPT_SUBNET_MASK:
			printf("subnet mask: %I\n", *(uint32_t *)&opt->data);
			break;
		case DHCPOPT_ROUTER:
			printf("router: %I (and %u more)\n",
			       *(uint32_t *)&opt->data, opt->len / 4 - 1);
			break;
		case DHCPOPT_DOMAIN_NAME_SERVER:
			printf("domain name server: %I (and %u more)\n",
			       *(uint32_t *)&opt->data, opt->len / 4 - 1);
			break;
		case DHCPOPT_LEASE_TIME:
			printf("lease time: %u\n", *(uint32_t *)&opt->data);
			break;
		case DHCPOPT_MSG_TYPE:
			printf("message type: %u\n", opt->data[0]);
			break;
		case DHCPOPT_SERVER_IDENTIFIER:
			printf("server identifier: %u\n",
			       *(uint32_t *)&opt->data);
			break;
		default:
			printf("unhandled option: %u\n", opt->tag);
			break;
		}
		offset += incr;
		opt = (struct dhcp_option *)((void *)opt + incr);
	}
}

static void dhcp_handle_offer(struct packet *pkt)
{
	struct dhcp *dhcp = (struct dhcp *)pkt->al;
	if (dhcp->op != BOOTREPLY) {
		printf("  wrong op for DHCP reply, expected BOOTREPLY, got "
		       "%u\n",
		       dhcp->op);
		return;
	}

	if (dhcp->options[0] != DHCPOPT_MSG_TYPE) {
		printf("   error: expectected first option to be message type "
		       "(53), got %u\n",
		       dhcp->options[0]);
		return;
	}
	if (dhcp->options[2] == DHCPMTYPE_DHCPOFFER) {
		puts("  DHCP OFFER HAS ARRIVED, BINGO!\n");
	}
	dhcp_enumerate_options(dhcp,
	                       ntohs(pkt->udp->len) - sizeof(struct udphdr));
}
int dhcp_cmd_discover(int argc, char **argv)
{
	struct packet *offer;

	interrupt_disable();
	dhcp_discover(&nif);
	offer = udp_wait(UDPPORT_DHCP_CLIENT); /* interrupts re-enabled */

	dhcp_handle_offer(offer);
	packet_free(offer);
}
