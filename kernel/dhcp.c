#include "kernel.h"
#include "net.h"
#include "string.h"

int dhcp_discover(struct netif *netif)
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

int dhcp_cmd_discover(int argc, char **argv)
{
	dhcp_discover(&nif);
}
