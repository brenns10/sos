/**
 * virtio declarations (mmio, queue)
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

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
	volatile struct virtqueue_desc *desc;
	volatile struct virtqueue_avail *avail;
	volatile uint16_t *used_event;
	volatile struct virtqueue_used *used;
	volatile uint16_t *avail_event;
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
} __attribute__((packed));

#define VIRTIO_BLK_SECTOR_SIZE 512

#define VIRTIO_BLK_S_OK       0
#define VIRTIO_BLK_S_IOERR    1
#define VIRTIO_BLK_S_UNSUPP   2