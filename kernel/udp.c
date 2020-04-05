#include "kernel.h"
#include "net.h"

struct udp_wait_entry {
	struct process *proc;
	struct packet *rcv;
	uint16_t port;
	struct list_head list;
};

DECLARE_LIST_HEAD(waitlist);

/*
 * Wait for a packet to come in on "port".
 *
 * YOU MUST HAVE ALREADY DISABLED INTERRUPTS BEFORE CALLING THIS FUNCTION.
 *
 * Why? We are about to go into a sleep which we can only wake from by the
 * receipt of a packet. Presumably, you have already sent out the packet which
 * will trigger this receipt. If intrerupts are enabled, the packet could be
 * received and handled BEFORE we have entered our sleep.
 */
struct packet *udp_wait(uint16_t port)
{
	struct udp_wait_entry entry;
	entry.proc = current;
	entry.port = port;
	entry.rcv = NULL;
	list_insert(&waitlist, &entry.list);

	current->flags.pr_ready = 0;

	interrupt_enable();
	block(current->context);

	list_remove(&entry.list);
	return entry.rcv;
}

void udp_recv(struct netif *netif, struct packet *pkt)
{
	struct udp_wait_entry *entry;
	printf("udp_recv src=%u dst=%u\n", ntohs(pkt->udp->src_port),
	       ntohs(pkt->udp->dst_port));
	pkt->al = pkt->tl + sizeof(struct udphdr);
	list_for_each_entry(entry, &waitlist, list, struct udp_wait_entry)
	{
		if (entry->port == ntohs(pkt->udp->dst_port)) {
			entry->rcv = pkt;
			entry->proc->flags.pr_ready = true;
			return;
		}
	}
	puts("nobody was waiting for this packet, freeing\n");
	packet_free(pkt);
}

int udp_send(struct netif *netif, struct packet *pkt, uint32_t src_ip,
             uint32_t dst_ip, uint16_t src_port, uint16_t dst_port)
{
	uint32_t csum;
	uint32_t len_rounded;
	pkt->tl = pkt->al - sizeof(struct udphdr);

	pkt->udp->src_port = htons(src_port);
	pkt->udp->dst_port = htons(dst_port);
	pkt->udp->len = htons(pkt->end - pkt->tl);
	len_rounded = pkt->end - pkt->tl;
	len_rounded = (len_rounded / 2) + (len_rounded % 2);
	pkt->udp->csum = 0;
	csum_init(&csum);
	csum_add(&csum, &src_ip, 2);
	csum_add(&csum, &dst_ip, 2);
	csum_add_value(&csum, ntohs((uint16_t)IPPROTO_UDP));
	csum_add(&csum, &pkt->udp->len, 1);
	csum_add(&csum, (uint16_t *)pkt->udp, len_rounded);
	pkt->udp->csum = csum_finalize(&csum);
	ip_send(netif, pkt, IPPROTO_UDP, src_ip, dst_ip);
}

int udp_reserve(void)
{
	return ip_reserve() + sizeof(struct udphdr);
}
