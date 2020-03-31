/*
 * Virtio network driver
 */
#include "kernel.h"
#include "net.h"
#include "slab.h"
#include "string.h"
#include "virtio.h"

struct netif nif;
struct virtio_net netdev;
#define PACKET_PAGES 8192
#define NPACKETS     4
#define RESERVED     2048
struct packet *rxpackets[NPACKETS];
struct packet *txpackets[NPACKETS];
struct slab *nethdr_slab = NULL;

struct virtio_cap net_caps[] = {
	{ "VIRTIO_NET_F_CSUM", 0, true,
	  "Device handles packets with partial checksum. This “checksum "
	  "offload” is a common feature on modern network cards." },
	{ "VIRTIO_NET_F_GUEST_CSUM", 1, false,
	  "Driver handles packets with partial checksum." },
	{ "VIRTIO_NET_F_CTRL_GUEST_OFFLOADS", 2, false,
	  "Control channel offloads reconfiguration support." },
	{ "VIRTIO_NET_F_MAC", 5, true, "Device has given MAC address." },
	{ "VIRTIO_NET_F_GUEST_TSO4", 7, false, "Driver can receive TSOv4." },
	{ "VIRTIO_NET_F_GUEST_TSO6", 8, false, "Driver can receive TSOv6." },
	{ "VIRTIO_NET_F_GUEST_ECN", 9, false,
	  "Driver can receive TSO with ECN." },
	{ "VIRTIO_NET_F_GUEST_UFO", 10, false, "Driver can receive UFO." },
	{ "VIRTIO_NET_F_HOST_TSO4", 11, false, "Device can receive TSOv4." },
	{ "VIRTIO_NET_F_HOST_TSO6", 12, false, "Device can receive TSOv6." },
	{ "VIRTIO_NET_F_HOST_ECN", 13, false,
	  "Device can receive TSO with ECN." },
	{ "VIRTIO_NET_F_HOST_UFO", 14, false, "Device can receive UFO." },
	{ "VIRTIO_NET_F_MRG_RXBUF", 15, false,
	  "Driver can merge receive buffers." },
	{ "VIRTIO_NET_F_STATUS", 16, true,
	  "Configuration status field is available." },
	{ "VIRTIO_NET_F_CTRL_VQ", 17, false, "Control channel is available." },
	{ "VIRTIO_NET_F_CTRL_RX", 18, false,
	  "Control channel RX mode support." },
	{ "VIRTIO_NET_F_CTRL_VLAN", 19, false,
	  "Control channel VLAN filtering." },
	{ "VIRTIO_NET_F_GUEST_ANNOUNCE", 21, false,
	  "Driver can send gratuitous packets." },
	{ "VIRTIO_NET_F_MQ", 22, false,
	  "Device supports multiqueue with automatic receive steering." },
	{ "VIRTIO_NET_F_CTRL_MAC_ADDR", 23, false,
	  "Set MAC address through control channel" },
	VIRTIO_INDP_CAPS
};

struct packet *packet_alloc(void)
{
	struct packet *pkt = txpackets[0];
	pkt->ll = NULL;
	pkt->nl = NULL;
	pkt->tl = NULL;
	pkt->al = NULL;
	pkt->end = NULL;
	pkt->capacity = RESERVED - sizeof(struct packet);
	return pkt;
}

void packet_free(void)
{
}

void init_packets(void **ptr)
{
	void *pages = kmem_get_pages(PACKET_PAGES, 0);
	/* add 2 for align */
	uint32_t pkt_size = RESERVED;
	uint32_t i;
	for (i = 0; i < NPACKETS; i++) {
		ptr[i] = (struct packet *)(pages + i * pkt_size);
	}
}

static void maybe_init_nethdr_slab(void)
{
	if (!nethdr_slab)
		nethdr_slab =
		        slab_new(sizeof(struct virtio_net_hdr), kmem_get_page);
}

void add_to_virtqueue(struct packet **pkts, int n, struct virtqueue *virtq)
{
	int i;
	uint32_t d1, d2;
	struct virtio_net_hdr *hdr;
	for (i = 0; i < n; i++) {
		hdr = slab_alloc(nethdr_slab);
		d1 = virtq_alloc_desc(virtq, hdr);
		d2 = virtq_alloc_desc(virtq, &pkts[i]->data);
		virtq->desc[d1].len = VIRTIO_NET_HDRLEN;
		virtq->desc[d1].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
		virtq->desc[d1].next = d2;
		virtq->desc[d2].len = RESERVED - sizeof(struct packet);
		virtq->desc[d2].flags = VIRTQ_DESC_F_WRITE;
		pkts[i]->capacity = RESERVED - sizeof(struct packet);
		virtq->avail->ring[virtq->avail->idx + i] = d1;
	}
	mb();
	virtq->avail->idx += n;
}

void virtio_net_send(struct virtio_net *dev, void *data, uint32_t len)
{
	uint32_t d1, d2;
	struct virtio_net_hdr *hdr =
	        (struct virtio_net_hdr *)slab_alloc(nethdr_slab);

	hdr->flags = 0;
	hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
	hdr->hdr_len = 0;  /* not used unless we have segmentation offload */
	hdr->gso_size = 0; /* same */
	hdr->csum_start = 0;
	hdr->csum_offset = 0;
	hdr->num_buffers = 0xDEAD;

	d1 = virtq_alloc_desc(dev->tx, (void *)hdr);
	dev->tx->desc[d1].len = VIRTIO_NET_HDRLEN;
	dev->tx->desc[d1].flags = VIRTQ_DESC_F_NEXT;

	d2 = virtq_alloc_desc(dev->tx, data);
	dev->tx->desc[d2].len = len;
	dev->tx->desc[d2].flags = 0;

	dev->tx->desc[d1].next = d2;

	dev->tx->avail->ring[dev->tx->avail->idx] = d1;
	mb();
	dev->tx->avail->idx += 1;
	mb();
	WRITE32(dev->regs->QueueNotify, VIRTIO_NET_Q_TX);
}

void dhcp_enumerate_options(struct dhcp *dhcp, uint32_t len)
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

int virtio_net_cmd_status(int argc, char **argv)
{
	printf("virtio_net_dev at 0x%x\n",
	       kmem_lookup_phys((void *)netdev.regs));
	printf("    Status=0x%x\n", READ32(netdev.regs->Status));
	printf("    DeviceID=0x%x\n", READ32(netdev.regs->DeviceID));
	printf("    VendorID=0x%x\n", READ32(netdev.regs->VendorID));
	printf("    InterruptStatus=0x%x\n",
	       READ32(netdev.regs->InterruptStatus));
	printf("    MagicValue=0x%x\n", READ32(netdev.regs->MagicValue));
	printf("  tx queue:\n");
	printf("    avail.idx = %u\n", netdev.tx->avail->idx);
	printf("    used.idx = %u\n", netdev.tx->used->idx);
	WRITE32(netdev.regs->QueueSel, VIRTIO_NET_Q_TX);
	mb();
	printf("    ready = 0x%x\n", READ32(netdev.regs->QueueReady));
	printf("  rx queue:\n");
	printf("    avail.idx = %u\n", netdev.rx->avail->idx);
	printf("    used.idx = %u\n", netdev.rx->used->idx);
	WRITE32(netdev.regs->QueueSel, VIRTIO_NET_Q_RX);
	mb();
	printf("    ready = 0x%x\n", READ32(netdev.regs->QueueReady));
}

void virtio_handle_rxused(struct virtio_net *dev, uint32_t idx)
{
	uint32_t d1 = dev->rx->used->ring[idx].id;
	uint32_t d2 = dev->rx->desc[d1].next;
	uint32_t len = dev->rx->used->ring[idx].len;
	struct virtio_net_hdr *hdr =
	        (struct virtio_net_hdr *)dev->rx->desc_virt[d1];
	struct packet *pkt = (struct packet *)((void *)dev->rx->desc_virt[d2] -
	                                       sizeof(struct packet));
	pkt->ll = &pkt->data;
	pkt->end = (void *)&pkt->data + (len - VIRTIO_NET_HDRLEN);
	eth_recv(&nif, pkt);
	virtq_free_desc(dev->rx, d1);
	virtq_free_desc(dev->rx, d2);
	slab_free(nethdr_slab, hdr);
}

void virtio_handle_txused(struct virtio_net *dev, uint32_t idx)
{
}

void virtio_net_isr(uint32_t intid)
{
	uint32_t i;
	struct virtio_net *dev = &netdev;
	uint32_t stat = READ32(dev->regs->InterruptStatus);
	WRITE32(dev->regs->InterruptACK, stat);

	for (i = dev->rx->seen_used; i != dev->rx->used->idx;
	     i = wrap(i + 1, 32)) {
		virtio_handle_rxused(dev, i);
	}
	dev->rx->seen_used = dev->rx->used->idx;
	for (i = dev->tx->seen_used; i != dev->tx->used->idx;
	     i = wrap(i + 1, 32)) {
		virtio_handle_txused(dev, i);
	}
	dev->tx->seen_used = dev->tx->used->idx;
	gic_end_interrupt(intid);
}

int virtio_net_init(virtio_regs *regs, uint32_t intid)
{
	void *pages;
	volatile struct virtio_net_config *cfg =
	        (struct virtio_net_config *)regs->Config;
	virtio_check_capabilities(regs, net_caps, nelem(net_caps),
	                          "virtio-net");

	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_FEATURES_OK);
	mb();
	if (!(regs->Status & VIRTIO_STATUS_FEATURES_OK)) {
		puts("error: virtio-net did not accept our features\n");
		return -1;
	}

	netdev.regs = regs;
	netdev.cfg = cfg;
	netdev.tx = virtq_create(128);
	netdev.rx = virtq_create(128);

	nif.ip = 0;
	memcpy(&nif.mac, &cfg->mac, 6);
	nif.gateway_ip = 0;
	nif.subnet_mask = 0;
	nif.dev = &netdev;

	maybe_init_nethdr_slab();
	init_packets(rxpackets);
	init_packets(txpackets);
	add_to_virtqueue(rxpackets, NPACKETS, netdev.rx);

	virtq_add_to_device(regs, netdev.rx, VIRTIO_NET_Q_RX);
	virtq_add_to_device(regs, netdev.tx, VIRTIO_NET_Q_TX);

	gic_register_isr(intid, 1, virtio_net_isr);
	gic_enable_interrupt(intid);
	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_DRIVER_OK);
	mb();

	printf("virtio-net 0x%x (intid %u, MAC %M): ready!\n",
	       kmem_lookup_phys((void *)regs), intid, &cfg->mac);
}
