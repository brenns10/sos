/*
 * Virtio network driver
 */
#include "kernel.h"
#include "virtio.h"
#include "packets.h"

struct virtio_net netdev;
#define PACKET_PAGES 8192
#define NPACKETS 5
void *rxpackets[NPACKETS];
void *txpackets[NPACKETS];
struct slab *nethdr_slab = NULL;

static inline uint32_t ntohl(uint32_t orig)
{
	return ((orig & 0xFF) << 24)
		| ((orig & 0xFF00) << 8)
		| ((orig & 0xFF0000) >> 8)
		| ((orig & 0xFF000000) >> 24);
}

static inline uint16_t ntohs(uint16_t orig)
{
	return ((orig & 0xFF) << 8 | (orig & 0xFF00) >> 8);
}

static inline uint32_t htonl(uint32_t orig)
{
	return ntohl(orig);
}

static inline uint32_t htons(uint16_t orig)
{
	return ntohs(orig);
}

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
};

void init_packets(void **ptr)
{
	void *pages = kmem_get_pages(PACKET_PAGES, 0);
	/* add 2 for align */
	uint32_t pkt_size = MAX_ETH_PKT_SIZE + VIRTIO_NET_HDRLEN + 2;
	uint32_t i;
	for (i = 0; i < NPACKETS; i++)
		ptr[i] = pages + i * pkt_size;
}

static void maybe_init_nethdr_slab(void)
{
	if (!nethdr_slab)
		nethdr_slab = slab_new(sizeof(struct virtio_net_hdr),
		                       kmem_get_page, kmem_free_page);
}

void add_to_virtqueue(void **ptr, int n, struct virtqueue *virtq)
{
	int i;
	uint32_t d;
	for (i = 0; i < n; i++) {
		d = virtq_alloc_desc(virtq, ptr[i]);
		virtq->desc[d].len = MAX_ETH_PKT_SIZE + VIRTIO_NET_HDRLEN;
		virtq->desc[d].flags = VIRTQ_DESC_F_WRITE;
		virtq->avail->ring[virtq->avail->idx + i] = d;
	}
	mb();
	virtq->avail->idx += n;
}

static inline void csum_init(uint32_t *csum)
{
	*csum = 0;
}

static inline void csum_add(uint32_t *csum, uint16_t *data, uint32_t n)
{
	uint32_t i;
	for (i = 0; i < n; i++) {
		*csum += data[i];
	}
}

static inline void csum_add_value(uint32_t *csum, uint16_t data)
{
	data = htons(data);
	csum_add(csum, &data, 1);
}

static inline uint16_t csum_finalize(uint32_t *csum)
{
	uint32_t add;
	while (*csum & 0xFFFF0000) {
		add = (*csum & 0xFFFF0000) >> 16;
		*csum &= 0x0000FFFF;
		*csum += add;
	}
	return ~((uint16_t) *csum);
}

void virtio_net_send(struct virtio_net *dev, void *data, uint32_t len)
{
	uint32_t d1, d2;
	struct virtio_net_hdr *hdr = (struct virtio_net_hdr *)slab_alloc(nethdr_slab);

	hdr->flags = 0;
	hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
	hdr->hdr_len = 0; /* not used unless we have segmentation offload */
	hdr->gso_size = 0; /* same */
	hdr->csum_start = 0;
	hdr->csum_offset = 0;
	hdr->num_buffers = 0xDEAD;

	d1 = virtq_alloc_desc(dev->tx, (void *)hdr);
	dev->tx->desc[d1].len = VIRTIO_NET_HDRLEN;
	dev->tx->desc[d1].flags = VIRTQ_DESC_F_NEXT;
	printf("virtio_net_hdr len = %u\n", dev->tx->desc[d1].len);

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

int virtio_net_send_dhcpdiscover(struct virtio_net *dev)
{
	uint32_t csum;
	struct virtio_net_config *cfg =
		(struct virtio_net_config *)dev->regs->Config;
	uint8_t *packet = txpackets[0] + 2;
	struct etherframe *etherframe = (struct etherframe *)packet;
	struct iphdr *iphdr = (struct iphdr *)(packet + sizeof(struct etherframe));
	struct udphdr *udphdr = (struct iphdr *)((void *)iphdr + sizeof(struct iphdr));
	struct dhcp *dhcp = (struct dhcp *)((void *)udphdr + sizeof(struct udphdr));

	struct dhcp_option *dtype;

	printf("packet at 0x%x\n", packet);

	memset(packet, 0, MAX_ETH_PKT_SIZE);
	dhcp->op = 1; /* BOOTREQUEST */
	dhcp->htype = 1; /* ethernet */
	dhcp->hlen = 6; /* mac address = 6 bytes */
	dhcp->xid = htonl(0xDEADBEEF);
	dhcp->cookie = htonl(DHCP_MAGIC_COOKIE);
	memcpy(&dhcp->chaddr, &cfg->mac, nelem(cfg->mac));

	dtype = &dhcp->options;
	dtype->tag = DHCPOPT_MSG_TYPE;
	dtype->len = 1;
	dtype->data[0] = DHCPMTYPE_DHCPDISCOVER;
	dhcp->options[3] = 255; /* END option */

	udphdr->src_port = htons(68);
	udphdr->dst_port = htons(67);
	/* our option space is a total of 4 bytes */
	udphdr->len = htons(sizeof(struct udphdr) + sizeof(struct dhcp) + 4);

	/* version 4, ihl is 5 words aka 20 bytes */
	iphdr->verihl = 5 | (4 << 4);
	iphdr->len = htons(ntohs(udphdr->len) + 20);
	iphdr->id = htons(1);
	iphdr->ttl = 8; /* don't let our crappy packets bounce around long */
	iphdr->proto = 17; /* udp yo */
	iphdr->src = htonl(0); /* who even am i? */
	iphdr->dst = htonl(0xFFFFFFFF); /* is anybody out there? */
	csum_init(&csum);
	csum_add(&csum, (uint16_t *)iphdr, sizeof(struct iphdr)/2);
	iphdr->csum = csum_finalize(&csum);

	/* now let's set the udp checksum */
	csum_init(&csum);
	csum_add(&csum, &iphdr->src, 2);
	csum_add(&csum, &iphdr->dst, 2);
	csum_add_value(&csum, htons((uint16_t)iphdr->proto << 8));
	csum_add(&csum, &udphdr->len, 1);
	csum_add(&csum, (uint16_t *)udphdr, ntohs(udphdr->len)/2);
	udphdr->csum = csum_finalize(&csum);

	memset(&etherframe->dst_mac, 0xFF, 6);
	memcpy(&etherframe->src_mac, &cfg->mac, nelem(cfg->mac));
	etherframe->ethertype = htons(0x0800); /* ip */

	virtio_net_send(dev, packet, ntohs(iphdr->len) + 14);
	return 0;
}

int virtio_net_cmd_dhcpdiscover(int argc, char **argv)
{
	return virtio_net_send_dhcpdiscover(&netdev);
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

void virtio_net_isr(uint32_t intid)
{
	struct virtio_net *dev = &netdev;
	uint32_t stat = READ32(dev->regs->InterruptStatus);
	printf("virtio-net: INTERRUPT status 0x%x\n", stat);
	WRITE32(dev->regs->InterruptACK, stat);
	if (dev->rx->seen_used != dev->rx->used->idx) {
		puts("new used in rx\n");
	}
	if (dev->tx->seen_used != dev->tx->used->idx) {
		puts("new used in tx\n");
	}
	gic_end_interrupt(intid);
}

int virtio_net_init(virtio_regs *regs, uint32_t intid)
{
	void *pages;
	volatile struct virtio_net_config *cfg =
		(struct virtio_net_config *)regs->Config;
	virtio_check_capabilities(regs, net_caps, nelem(net_caps));

	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_FEATURES_OK);
	mb();
	if (!(regs->Status & VIRTIO_STATUS_FEATURES_OK)) {
		puts("error: virtio-net did not accept our features\n");
		return -1;
	}

	printf("virtio-net has MAC %x:%x:%x:%x:%x:%x\n", cfg->mac[0], cfg->mac[1],
			cfg->mac[2], cfg->mac[3], cfg->mac[4], cfg->mac[5]);

	netdev.regs = regs;
	netdev.cfg = cfg;
	netdev.tx = virtq_create(128);
	netdev.rx = virtq_create(128);

	init_packets(rxpackets);
	init_packets(txpackets);
	add_to_virtqueue(rxpackets, NPACKETS, netdev.rx);

	virtq_add_to_device(regs, netdev.rx, VIRTIO_NET_Q_RX);
	virtq_add_to_device(regs, netdev.tx, VIRTIO_NET_Q_TX);

	gic_register_isr(intid, 1, virtio_net_isr);
	gic_enable_interrupt(intid);
	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_DRIVER_OK);
	mb();

	maybe_init_nethdr_slab();

	printf("virtio-net 0x%x (intid %u): read! (status %x)\n",
			kmem_lookup_phys((void *)regs), intid,
			READ32(regs->Status));
}
