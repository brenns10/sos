/**
 * Block device driver based on virtio.
 */
#include "blk.h"
#include "format.h"
#include "kernel.h"
#include "list.h" /* for container_of */
#include "slab.h"
#include "string.h"
#include "sync.h"
#include "virtio.h"

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
	VIRTIO_INDP_CAPS
};

#define get_vblkreq(req) container_of(req, struct virtio_blk_req, blkreq)

struct slab *blkreq_slab = NULL;
struct list_head vdevs;
spinsem_t vdev_list_lock;

struct virtio_blk {
	virtio_regs *regs;
	struct virtio_blk_config *config;
	struct virtqueue *virtq;
	uint32_t intid;
	struct list_head list;
	struct blkdev blkdev;
};
#define get_vblkdev(dev) container_of(dev, struct virtio_blk, blkdev)

#define HI32(u64) ((uint32_t)((0xFFFFFFFF00000000ULL & (u64)) >> 32))
#define LO32(u64) ((uint32_t)(0x00000000FFFFFFFFULL & (u64)))

static void virtio_blk_handle_used(struct virtio_blk *dev, uint32_t usedidx)
{
	struct virtqueue *virtq = dev->virtq;
	uint32_t desc1, desc2, desc3;
	struct virtio_blk_req *req;

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

	virtq_free_desc(virtq, desc1);
	virtq_free_desc(virtq, desc2);
	virtq_free_desc(virtq, desc3);

	switch (req->status) {
	case VIRTIO_BLK_S_OK:
		req->blkreq.status = BLKREQ_OK;
		break;
	case VIRTIO_BLK_S_IOERR:
		req->blkreq.status = BLKREQ_ERR;
		break;
	default:
		puts("Unhandled status in virtio_blk irq\n");
		panic(NULL);
	}

	wait_list_awaken(&req->blkreq.wait);
	return;
bad_desc:
	puts("virtio-blk received malformed descriptors\n");
	return;
}

static struct virtio_blk *virtio_blk_get_dev_by_intid(uint32_t intid)
{
	struct virtio_blk *blk;
	int flags;
	spin_acquire_irqsave(&vdev_list_lock, &flags);
	list_for_each_entry(blk, &vdevs, list, struct virtio_blk)
	{
		if (blk->intid == intid) {
			spin_release_irqrestore(&vdev_list_lock, &flags);
			return blk;
		}
	}
	spin_release_irqrestore(&vdev_list_lock, &flags);
	return NULL;
}

static void virtio_blk_isr(uint32_t intid, struct ctx *ctx)
{
	int i, len;
	struct virtio_blk *dev = virtio_blk_get_dev_by_intid(intid);

	if (!dev) {
		puts("virtio-blk: received IRQ for unknown device!");
		panic(NULL);
		return; /* just to make it obvious we won't continue */
	}

	len = dev->virtq->len;

	WRITE32(dev->regs->InterruptACK, READ32(dev->regs->InterruptStatus));

	for (i = dev->virtq->seen_used; i != dev->virtq->used->idx;
	     i = wrap(i + 1, len)) {
		virtio_blk_handle_used(dev, i);
	}
	dev->virtq->seen_used = dev->virtq->used->idx;

	gic_end_interrupt(intid);
}

static void virtio_blk_send(struct virtio_blk *blk, struct virtio_blk_req *hdr)
{
	blk->virtq->avail->ring[blk->virtq->avail->idx] = hdr->descriptor;
	mb();
	blk->virtq->avail->idx += 1;
	mb();
	WRITE32(blk->regs->QueueNotify, 0);
}

static int virtio_blk_status(struct blkdev *dev)
{
	struct virtio_blk *blkdev = get_vblkdev(dev);
	printf("virtio_blk_dev at 0x%x\n",
	       kmem_lookup_phys((void *)blkdev->regs));
	printf("    Status=0x%x\n", READ32(blkdev->regs->Status));
	printf("    DeviceID=0x%x\n", READ32(blkdev->regs->DeviceID));
	printf("    VendorID=0x%x\n", READ32(blkdev->regs->VendorID));
	printf("    InterruptStatus=0x%x\n",
	       READ32(blkdev->regs->InterruptStatus));
	printf("    MagicValue=0x%x\n", READ32(blkdev->regs->MagicValue));
	printf("  Queue 0:\n");
	printf("    avail.idx = %u\n", blkdev->virtq->avail->idx);
	printf("    used.idx = %u\n", blkdev->virtq->used->idx);
	WRITE32(blkdev->regs->QueueSel, 0);
	mb();
	printf("    ready = 0x%x\n", READ32(blkdev->regs->QueueReady));
	return 0;
}

static struct blkreq *virtio_blk_alloc(struct blkdev *dev)
{
	struct virtio_blk_req *vblkreq = slab_alloc(blkreq_slab);
	blkreq_init(&vblkreq->blkreq);
	return &vblkreq->blkreq;
}

static void virtio_blk_free(struct blkdev *dev, struct blkreq *req)
{
	struct virtio_blk_req *vblkreq = get_vblkreq(req);
	slab_free(blkreq_slab, vblkreq);
}

static void virtio_blk_submit(struct blkdev *dev, struct blkreq *req)
{
	struct virtio_blk *blk = get_vblkdev(dev);
	struct virtio_blk_req *hdr = get_vblkreq(req);
	uint32_t d1, d2, d3, datamode = 0;

	if (req->type == BLKREQ_READ) {
		hdr->type = VIRTIO_BLK_T_IN;
		datamode = VIRTQ_DESC_F_WRITE; /* mark page writeable */
	} else {
		hdr->type = VIRTIO_BLK_T_OUT;
	}
	hdr->sector = req->blkidx;

	d1 = virtq_alloc_desc(blk->virtq, hdr);
	hdr->descriptor = d1;
	blk->virtq->desc[d1].len = VIRTIO_BLK_REQ_HEADER_SIZE;
	blk->virtq->desc[d1].flags = VIRTQ_DESC_F_NEXT;

	d2 = virtq_alloc_desc(blk->virtq, req->buf);
	blk->virtq->desc[d2].len = VIRTIO_BLK_SECTOR_SIZE;
	blk->virtq->desc[d2].flags = datamode | VIRTQ_DESC_F_NEXT;

	d3 = virtq_alloc_desc(blk->virtq,
	                      (void *)hdr + VIRTIO_BLK_REQ_HEADER_SIZE);
	blk->virtq->desc[d3].len = VIRTIO_BLK_REQ_FOOTER_SIZE;
	blk->virtq->desc[d3].flags = VIRTQ_DESC_F_WRITE;

	blk->virtq->desc[d1].next = d2;
	blk->virtq->desc[d2].next = d3;

	virtio_blk_send(blk, hdr);
}

static void maybe_virtio_mod_init(void)
{
	if (!blkreq_slab) {
		blkreq_slab =
		        slab_new("virtio_blk_req",
		                 sizeof(struct virtio_blk_req), kmem_get_page);
		INIT_LIST_HEAD(vdevs);
		INIT_SPINSEM(&vdev_list_lock, 1);
	}
}

struct blkdev_ops virtio_blk_ops = {
	.alloc = virtio_blk_alloc,
	.free = virtio_blk_free,
	.submit = virtio_blk_submit,
	.status = virtio_blk_status,
};

int virtio_blk_init(virtio_regs *regs, uint32_t intid)
{
	struct virtio_blk *vdev;
	struct virtqueue *virtq;
	int flags;
	uint32_t genbefore, genafter;

	maybe_virtio_mod_init();
	vdev = kmalloc(sizeof(struct virtio_blk));

	virtio_check_capabilities(regs, blk_caps, nelem(blk_caps),
	                          "virtio-blk");

	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_FEATURES_OK);
	mb();
	if (!(regs->Status & VIRTIO_STATUS_FEATURES_OK)) {
		puts("error: virtio-blk did not accept our features\n");
		return -1;
	}

	virtq = virtq_create(128);
	virtq_add_to_device(regs, virtq, 0);

	vdev->regs = regs;
	vdev->virtq = virtq;
	vdev->intid = intid;
	vdev->config = (struct virtio_blk_config *)&regs->Config;
	vdev->blkdev.ops = &virtio_blk_ops;
	vdev->blkdev.blksiz = VIRTIO_BLK_SECTOR_SIZE;
	/* capacity is 64 bit, configuration reg read is not atomic */
	do {
		genbefore = READ32(vdev->regs->ConfigGeneration);
		vdev->blkdev.blkcnt = READ64(vdev->config->capacity);
		genafter = READ32(vdev->regs->ConfigGeneration);
	} while (genbefore != genafter);
	snprintf(&vdev->blkdev.name, sizeof(vdev->blkdev.name), "vblk%d",
	         vdev->intid);

	spin_acquire_irqsave(&vdev_list_lock, &flags);
	list_insert(&vdevs, &vdev->list);
	spin_release_irqrestore(&vdev_list_lock, &flags);

	gic_register_isr(intid, 1, virtio_blk_isr, "virtio-blk");
	gic_enable_interrupt(intid);

	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_DRIVER_OK);
	mb();

	blkdev_register(&vdev->blkdev);
	return 0;
}
