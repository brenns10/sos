/**
 * virtio declarations (mmio, queue)
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#define VIRTIO_MAGIC 0x74726976
#define VIRTIO_VERSION 0x2
#define VIRTIO_DEV_NET 0x1
#define VIRTIO_DEV_BLK 0x2
#define wrap(x, len) ((x) & ~(len))

/*
 * See Section 4.2.2 of VIRTIO 1.0 Spec:
 * http://docs.oasis-open.org/virtio/virtio/v1.0/cs04/virtio-v1.0-cs04.html
 */
typedef volatile struct __attribute__((packed)) {
	uint32_t MagicValue;
	uint32_t Version;
	uint32_t DeviceID;
	uint32_t VendorID;
	uint32_t DeviceFeatures;
	uint32_t DeviceFeaturesSel;
	uint32_t _reserved0[2];
	uint32_t DriverFeatures;
	uint32_t DriverFeaturesSel;
	uint32_t _reserved1[2];
	uint32_t QueueSel;
	uint32_t QueueNumMax;
	uint32_t QueueNum;
	uint32_t _reserved2[2];
	uint32_t QueueReady;
	uint32_t _reserved3[2];
	uint32_t QueueNotify;
	uint32_t _reserved4[3];
	uint32_t InterruptStatus;
	uint32_t InterruptACK;
	uint32_t _reserved5[2];
	uint32_t Status;
	uint32_t _reserved6[3];
	uint32_t QueueDescLow;
	uint32_t QueueDescHigh;
	uint32_t _reserved7[2];
	uint32_t QueueAvailLow;
	uint32_t QueueAvailHigh;
	uint32_t _reserved8[2];
	uint32_t QueueUsedLow;
	uint32_t QueueUsedHigh;
	uint32_t _reserved9[21];
	uint32_t ConfigGeneration;
	uint32_t Config[0];
} virtio_regs;

#define VIRTIO_STATUS_ACKNOWLEDGE (1)
#define VIRTIO_STATUS_DRIVER (2)
#define VIRTIO_STATUS_FAILED (128)
#define VIRTIO_STATUS_FEATURES_OK (8)
#define VIRTIO_STATUS_DRIVER_OK (4)
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET (64)

struct virtio_cap {
	char *name;
	uint32_t bit;
	bool support;
	char *help;
};

struct virtqueue_desc {
	uint64_t addr;
	uint32_t len;
/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT   1
/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE     2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT   4
	/* The flags as indicated above. */
	uint16_t flags;
	/* Next field if flags & NEXT */
	uint16_t next;
} __attribute__((packed));

struct virtqueue_avail {
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[0];
} __attribute__((packed));

struct virtqueue_used_elem {
	uint32_t id;
	uint32_t len;
} __attribute__((packed));

struct virtqueue_used {
#define VIRTQ_USED_F_NO_NOTIFY 1
	uint16_t flags;
	uint16_t idx;
	struct virtqueue_used_elem ring[0];
} __attribute__((packed));

/*
 * For simplicity, we lay out the virtqueue in contiguous memory on a single
 * page. See virtq_create for the layout and alignment requirements.
 */
struct virtqueue {
	/* Physical base address of the full data structure. */
	uint32_t phys;
	uint32_t len;
	uint32_t seen_used;
	uint32_t free_desc;

	volatile struct virtqueue_desc *desc;
	volatile struct virtqueue_avail *avail;
	volatile uint16_t *used_event;
	volatile struct virtqueue_used *used;
	volatile uint16_t *avail_event;
	void **desc_virt;
} __attribute__((packed));

struct virtio_blk_config {
	uint64_t capacity;
	uint32_t size_max;
	uint32_t seg_max;
	struct {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;
	uint32_t blk_size;
	struct {
		uint8_t physical_block_exp;
		uint8_t alignment_offset;
		uint16_t min_io_size;
		uint32_t opt_io_size;
	} topology;
	uint8_t writeback;
} __attribute__((packed));

struct virtio_net_config {
	uint8_t mac[6];
#define VIRTIO_NET_S_LINK_UP 1
#define VIRTIO_NET_S_ANNOUNCE 2
	uint16_t status;
	uint16_t max_virtqueue_pairs;
} __attribute__((packed));

#define VIRTIO_BLK_REQ_HEADER_SIZE 16
#define VIRTIO_BLK_REQ_FOOTER_SIZE 1
struct virtio_blk_req {
#define VIRTIO_BLK_T_IN       0
#define VIRTIO_BLK_T_OUT      1
#define VIRTIO_BLK_T_SCSI     2
#define VIRTIO_BLK_T_FLUSH    4
	uint32_t type;
	uint32_t reserved;
	uint64_t sector;
	uint8_t status;
	/* end standard fields, begin helpers */
	uint8_t _pad[3];
	struct process *waiting;
	uint32_t descriptor;
} __attribute__((packed));

#define VIRTIO_BLK_SECTOR_SIZE 512

#define VIRTIO_BLK_S_OK       0
#define VIRTIO_BLK_S_IOERR    1
#define VIRTIO_BLK_S_UNSUPP   2

struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1
	uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE 0
#define VIRTIO_NET_HDR_GSO_TCPV4 1
#define VIRTIO_NET_HDR_GSO_UDP 3
#define VIRTIO_NET_HDR_GSO_TCPV6 4
#define VIRTIO_NET_HDR_GSO_ECN 0x80
	uint8_t gso_type;
	uint16_t hdr_len;
	uint16_t gso_size;
	uint16_t csum_start;
	uint16_t csum_offset;
	uint16_t num_buffers;
} __attribute__((packed));
#define MAX_ETH_PKT_SIZE 1514

#define VIRTIO_NET_HDRLEN 10

#define VIRTIO_NET_Q_RX 0
#define VIRTIO_NET_Q_TX 1

struct virtio_net {
	virtio_regs *regs;
	volatile struct virtio_net_config *cfg;
	struct virtqueue *rx;
	struct virtqueue *tx;
};

/*
 * virtqueue routines
 */
struct virtqueue *virtq_create(uint32_t len);
uint32_t virtq_alloc_desc(struct virtqueue *virtq, void *addr);
void virtq_free_desc(struct virtqueue *virtq, uint32_t desc);
void virtq_add_to_device(volatile virtio_regs *regs, struct virtqueue *virtq, uint32_t queue_sel);

/*
 * General purpose routines for virtio drivers
 */
void virtio_check_capabilities(virtio_regs *device, struct virtio_cap *caps, uint32_t n);

/*
 */
int virtio_blk_init(virtio_regs *regs, uint32_t intid);
int virtio_net_init(virtio_regs *regs, uint32_t intid);
