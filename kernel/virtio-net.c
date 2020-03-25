/*
 * Virtio network driver
 */
#include "kernel.h"
#include "virtio.h"

struct virtio_net netdev;
#define PACKET_PAGES 8192
#define NPACKETS 5
void *rxpackets[NPACKETS];
void *txpackets[NPACKETS];

struct virtio_cap net_caps[] = {
	{ "VIRTIO_NET_F_CSUM", 0, false,
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
	uint32_t pkt_size = MAX_ETH_PKT_SIZE + sizeof(struct virtio_net_hdr);
	uint32_t i;
	for (i = 0; i < NPACKETS; i++)
		ptr[i] = pages + i * pkt_size;
}

void add_to_virtqueue(void **ptr, int n, struct virtqueue *virtq)
{
	int i;
	uint32_t d;
	for (i = 0; i < n; i++) {
		d = virtq_alloc_desc(virtq, ptr[i]);
		virtq->desc[d].len = MAX_ETH_PKT_SIZE + sizeof(struct virtio_net_hdr);
		virtq->desc[d].flags = VIRTQ_DESC_F_WRITE;
		virtq->avail->ring[virtq->avail->idx + i] = d;
	}
	mb();
	virtq->avail->idx += n;
}

void virtio_net_isr(uint32_t intid)
{
	struct virtio_net *dev = &netdev;
	puts("virtio-net: INTERRUPT\n");
	WRITE32(dev->regs->InterruptACK, READ32(dev->regs->InterruptStatus));
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

	virtq_add_to_device(regs, netdev.rx, 0);
	virtq_add_to_device(regs, netdev.tx, 1);

	gic_register_isr(intid, 1, virtio_net_isr);
	gic_enable_interrupt(intid);
	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_DRIVER_OK);
	mb();
	printf("virtio-net 0x%x (intid %u): read! (status %x)\n",
			kmem_lookup_phys((void *)regs), intid,
			READ32(regs->Status));
}
