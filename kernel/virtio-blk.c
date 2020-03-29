/**
 * Block device driver based on virtio.
 */
#include "kernel.h"
#include "slab.h"
#include "string.h"
#include "virtio.h"

struct virtio_cap indp_caps[] = {
	{ "VIRTIO_F_RING_INDIRECT_DESC", 28, false,
	  "Negotiating this feature indicates that the driver can use"
	  " descriptors with the VIRTQ_DESC_F_INDIRECT flag set, as"
	  " described in 2.4.5.3 Indirect Descriptors." },
	{ "VIRTIO_F_RING_EVENT_IDX", 29, false,
	  "This feature enables the used_event and the avail_event fields"
	  " as described in 2.4.7 and 2.4.8." },
	{ "VIRTIO_F_VERSION_1", 32, false,
	  "This indicates compliance with this specification, giving a"
	  " simple way to detect legacy devices or drivers."},
};

struct virtio_cap blk_caps[] = {
	{ "VIRTIO_BLK_F_SIZE_MAX", 1, false,
	  "Maximum size of any single segment is in size_max." },
	{ "VIRTIO_BLK_F_SEG_MAX", 2, false,
	  "Maximum number of segments in a request is in seg_max." },
	{ "VIRTIO_BLK_F_GEOMETRY", 4, false,
	  "Disk-style geometry specified in geometry." },
	{ "VIRTIO_BLK_F_RO", 5, false, "Device is read-only." },
	{ "VIRTIO_BLK_F_BLK_SIZE", 6, false,
	  "Block size of disk is in blk_size." },
	{ "VIRTIO_BLK_F_FLUSH", 9, false, "Cache flush command support." },
	{ "VIRTIO_BLK_F_TOPOLOGY", 10, false,
	  "Device exports information on optimal I/O alignment." },
	{ "VIRTIO_BLK_F_CONFIG_WCE", 11, false,
	  "Device can toggle its cache between writeback and "
	  "writethrough modes." },
};

struct slab *blkreq_slab;

struct virtio_blk {
	virtio_regs *regs;
	struct virtqueue *virtq;
	uint32_t intid;
} blkdev;

static void maybe_init_blkreq_slab(void)
{
	if (!blkreq_slab)
		blkreq_slab = slab_new(sizeof(struct virtio_blk_req),
		                       kmem_get_page, kmem_free_page);
}

#define HI32(u64) ((uint32_t)((0xFFFFFFFF00000000ULL & (u64)) >> 32))
#define LO32(u64) ((uint32_t)(0x00000000FFFFFFFFULL & (u64)))

static void virtio_blk_handle_used(struct virtio_blk *dev, uint32_t usedidx)
{
	struct virtqueue *virtq = dev->virtq;
	uint32_t desc1, desc2, desc3;
	struct virtio_blk_req *req;
	uint8_t *data;

	desc1 = virtq->used->ring[usedidx].id;
	if (!(virtq->desc[desc1].flags & VIRTQ_DESC_F_NEXT))
		goto bad_desc;
	desc2 = virtq->desc[desc1].next;
	if (!(virtq->desc[desc2].flags & VIRTQ_DESC_F_NEXT))
		goto bad_desc;
	desc3 = virtq->desc[desc2].next;
	if (virtq->desc[desc1].len != VIRTIO_BLK_REQ_HEADER_SIZE ||
	    virtq->desc[desc2].len != VIRTIO_BLK_SECTOR_SIZE ||
	    virtq->desc[desc3].len != VIRTIO_BLK_REQ_FOOTER_SIZE)
		goto bad_desc;

	req = virtq->desc_virt[desc1];
	data = virtq->desc_virt[desc2];

	virtq_free_desc(virtq, desc1);
	virtq_free_desc(virtq, desc2);
	virtq_free_desc(virtq, desc3);

	if (req->waiting) {
		req->waiting->flags.pr_ready = 1;
	} else {
		/* nobody is waiting for you :'( */
		slab_free(req);
		puts("virtio-blk got an orphaned descriptor\n");
	}
	return;
bad_desc:
	puts("virtio-blk received malformed descriptors\n");
	return;
}

static void virtio_blk_isr(uint32_t intid)
{
	/* TODO: support multiple block devices by examining intid */
	struct virtio_blk *dev = &blkdev;
	int i;
	int len = dev->virtq->len;

	WRITE32(dev->regs->InterruptACK, READ32(dev->regs->InterruptStatus));

	for (i = dev->virtq->seen_used; i != dev->virtq->used->idx;
	     i = wrap(i + 1, len)) {
		virtio_blk_handle_used(dev, i);
	}
	dev->virtq->seen_used = dev->virtq->used->idx;

	gic_end_interrupt(intid);
}

struct virtio_blk_req *virtio_blk_cmd(struct virtio_blk *blk, uint32_t type,
                                      uint32_t sector, uint8_t *data)
{
	struct virtio_blk_req *hdr = slab_alloc(blkreq_slab);
	uint32_t d1, d2, d3, datamode = 0;

	hdr->type = type;
	hdr->sector = sector;
	hdr->waiting = NULL;

	d1 = virtq_alloc_desc(blk->virtq, hdr);
	hdr->descriptor = d1;
	blk->virtq->desc[d1].len = VIRTIO_BLK_REQ_HEADER_SIZE;
	blk->virtq->desc[d1].flags = VIRTQ_DESC_F_NEXT;

	if (type == VIRTIO_BLK_T_IN)           /* if it's a read */
		datamode = VIRTQ_DESC_F_WRITE; /* mark page writeable */

	d2 = virtq_alloc_desc(blk->virtq, data);
	blk->virtq->desc[d2].len = VIRTIO_BLK_SECTOR_SIZE;
	blk->virtq->desc[d2].flags = datamode | VIRTQ_DESC_F_NEXT;

	d3 = virtq_alloc_desc(blk->virtq,
	                      (void *)hdr + VIRTIO_BLK_REQ_HEADER_SIZE);
	blk->virtq->desc[d3].len = VIRTIO_BLK_REQ_FOOTER_SIZE;
	blk->virtq->desc[d3].flags = VIRTQ_DESC_F_WRITE;

	blk->virtq->desc[d1].next = d2;
	blk->virtq->desc[d2].next = d3;
	return hdr;
}

void virtio_blk_send(struct virtio_blk *blk, struct virtio_blk_req *hdr)
{
	blk->virtq->avail->ring[blk->virtq->avail->idx] = hdr->descriptor;
	mb();
	blk->virtq->avail->idx += 1;
	mb();
	WRITE32(blk->regs->QueueNotify, 0);
}

struct virtio_blk_req *virtio_blk_read(struct virtio_blk *blk, uint32_t sector,
                                       uint8_t *data)
{
	return virtio_blk_cmd(blk, VIRTIO_BLK_T_IN, sector, data);
}

struct virtio_blk_req *virtio_blk_write(struct virtio_blk *blk, uint32_t sector,
                                        uint8_t *data)
{
	return virtio_blk_cmd(blk, VIRTIO_BLK_T_OUT, sector, data);
}

int virtio_blk_cmd_status(int argc, char **argv)
{
	printf("virtio_blk_dev at 0x%x\n",
	       kmem_lookup_phys((void *)blkdev.regs));
	printf("    Status=0x%x\n", READ32(blkdev.regs->Status));
	printf("    DeviceID=0x%x\n", READ32(blkdev.regs->DeviceID));
	printf("    VendorID=0x%x\n", READ32(blkdev.regs->VendorID));
	printf("    InterruptStatus=0x%x\n",
	       READ32(blkdev.regs->InterruptStatus));
	printf("    MagicValue=0x%x\n", READ32(blkdev.regs->MagicValue));
	printf("  Queue 0:\n");
	printf("    avail.idx = %u\n", blkdev.virtq->avail->idx);
	printf("    used.idx = %u\n", blkdev.virtq->used->idx);
	WRITE32(blkdev.regs->QueueSel, 0);
	mb();
	printf("    ready = 0x%x\n", READ32(blkdev.regs->QueueReady));
}

int virtio_blk_cmd_read(int argc, char **argv)
{
	uint32_t sector, rv = 0;
	struct virtio_blk_req *req;
	uint8_t *buffer;

	if (argc != 2) {
		puts("usage: read SECTOR\n");
		return 1;
	}

	buffer = kmem_get_page();
	sector = atoi(argv[1]);
	req = virtio_blk_read(&blkdev, sector, buffer);
	req->waiting = current;
	current->flags.pr_ready = 0;
	virtio_blk_send(&blkdev, req);
	block(current->context);
	if (req->status != VIRTIO_BLK_S_OK) {
		puts("ERROR\n");
		rv = 1;
		goto cleanup;
	}
	printf("result: \"%s\"\n", buffer);
cleanup:
	slab_free(req);
	kmem_free_page(buffer);
	return 0;
}

int virtio_blk_cmd_write(int argc, char **argv)
{
	uint32_t sector, len, rv = 0;
	uint8_t *buffer;
	struct virtio_blk_req *req;

	if (argc != 3) {
		puts("usage: blkwrite SECTOR STRING\n");
		return 1;
	}

	buffer = kmem_get_page();
	sector = atoi(argv[1]);
	len = strlen(argv[2]);
	memcpy(buffer, argv[2], len + 1);
	req = virtio_blk_write(&blkdev, sector, buffer);
	req->waiting = current;
	current->flags.pr_ready = 0;
	virtio_blk_send(&blkdev, req);
	block(current->context);
	if (req->status != VIRTIO_BLK_S_OK) {
		puts("ERROR\n");
		rv = 1;
		goto cleanup;
	}
	puts("written!\n");

cleanup:
	slab_free(req);
	kmem_free_page(buffer);
	return rv;
}

int virtio_blk_init(virtio_regs *regs, uint32_t intid)
{
	volatile struct virtio_blk_config *conf =
	        (struct virtio_blk_config *)regs->Config;
	struct virtqueue *virtq;
	uint32_t i;

	virtio_check_capabilities(regs, blk_caps, nelem(blk_caps));
	virtio_check_capabilities(regs, indp_caps, nelem(indp_caps));

	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_FEATURES_OK);
	mb();
	if (!(regs->Status & VIRTIO_STATUS_FEATURES_OK)) {
		puts("error: virtio-blk did not accept our features\n");
		return -1;
	}

	printf("virtio-blk has 0x%x %x sectors\n", HI32(conf->capacity),
	       LO32(conf->capacity));
	printf("virtio-blk queuenummax %u\n", READ32(regs->QueueNumMax));
	printf("virtio-blk Status %x\n", READ32(regs->Status));
	printf("virtio-blk InterruptStatus %x\n", regs->InterruptStatus);

	virtq = virtq_create(128);
	virtq_add_to_device(regs, virtq, 0);

	blkdev.regs = regs;
	blkdev.virtq = virtq;
	blkdev.intid = intid;

	gic_register_isr(intid, 1, virtio_blk_isr);
	gic_enable_interrupt(intid);

	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_DRIVER_OK);
	mb();
	printf("virtio-blk Status %x\n", READ32(regs->Status));

	maybe_init_blkreq_slab();
	printf("virtio-blk 0x%x (intid %u): ready!\n",
	       kmem_lookup_phys((void *)regs), intid);
}
