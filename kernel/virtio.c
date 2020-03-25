/**
 * Implements virtio device drivers, particularly mmio ones.
 *
 * Reference:
 *
 * http://docs.oasis-open.org/virtio/virtio/v1.0/cs04/virtio-v1.0-cs04.html
 */

#include "virtio.h"
#include "kernel.h"
#include "slab.h"
#include "string.h"

struct virtqueue *virtq_create(uint32_t len)
{
	int i;
	uint32_t page_phys;
	uint32_t page_virt;
	struct virtqueue *virtq;

	/* compute offsets */
	uint32_t off_desc = ALIGN(sizeof(struct virtqueue), 16);
	uint32_t off_avail =
	        ALIGN(off_desc + len * sizeof(struct virtqueue_desc), 2);
	uint32_t off_used_event = (off_avail + sizeof(struct virtqueue_avail) +
	                           len * sizeof(uint16_t));
	uint32_t off_used = ALIGN(off_used_event + sizeof(uint16_t), 4);
	uint32_t off_avail_event = (off_used + sizeof(struct virtqueue_used) +
	                            len * sizeof(struct virtqueue_used_elem));
	uint32_t off_desc_virt =
	        ALIGN(off_avail_event + sizeof(uint16_t), sizeof(void *));
	uint32_t memsize = off_desc_virt + len * sizeof(void *);

	if (memsize > PAGE_SIZE) {
		printf("virtq_create: error, too big for a page\n");
		return NULL;
	}
	page_phys = alloc_pages(phys_allocator, PAGE_SIZE, 0);
	page_virt = alloc_pages(kern_virt_allocator, PAGE_SIZE, 0);
	kmem_map_pages(page_virt, page_phys, PAGE_SIZE,
	               PRW_UNA | EXECUTE_NEVER);

	virtq = (struct virtqueue *)page_virt;
	virtq->phys = page_phys;
	virtq->len = len;

	virtq->desc = (struct virtqueue_desc *)(page_virt + off_desc);
	virtq->avail = (struct virtqueue_avail *)(page_virt + off_avail);
	virtq->used_event = (uint16_t *)(page_virt + off_used_event);
	virtq->used = (struct virtqueue_used *)(page_virt + off_used);
	virtq->avail_event = (uint16_t *)(page_virt + off_avail_event);
	virtq->desc_virt = (void **)(page_virt + off_desc_virt);

	virtq->avail->idx = 0;
	virtq->used->idx = 0;
	virtq->seen_used = virtq->used->idx;
	virtq->free_desc = 0;

	for (i = 0; i < len; i++) {
		virtq->desc[i].next = i + 1;
	}

	return virtq;
}

uint32_t virtq_alloc_desc(struct virtqueue *virtq, void *addr)
{
	uint32_t desc = virtq->free_desc;
	uint32_t next = virtq->desc[desc].next;
	if (next == virtq->len)
		puts("ERROR: ran out of virtqueue descriptors\n");
	virtq->free_desc = next;

	virtq->desc[desc].addr = kmem_lookup_phys(addr);
	virtq->desc_virt[desc] = addr;
	return desc;
}

void virtq_free_desc(struct virtqueue *virtq, uint32_t desc)
{
	virtq->desc[desc].next = virtq->free_desc;
	virtq->free_desc = desc;
	virtq->desc_virt[desc] = NULL;
}

void virtq_add_to_device(volatile virtio_regs *regs, struct virtqueue *virtq,
                         uint32_t queue_sel)
{
	WRITE32(regs->QueueSel, queue_sel);
	mb();
	WRITE32(regs->QueueNum, virtq->len);
	WRITE32(regs->QueueDescLow,
	        virtq->phys + ((void *)virtq->desc - (void *)virtq));
	WRITE32(regs->QueueDescHigh, 0);
	WRITE32(regs->QueueAvailLow,
	        virtq->phys + ((void *)virtq->avail - (void *)virtq));
	WRITE32(regs->QueueAvailHigh, 0);
	WRITE32(regs->QueueUsedLow,
	        virtq->phys + ((void *)virtq->used - (void *)virtq));
	WRITE32(regs->QueueUsedHigh, 0);
	mb();
	WRITE32(regs->QueueReady, 1);
}

void virtio_check_capabilities(virtio_regs *regs, struct virtio_cap *caps,
                               uint32_t n)
{
	uint32_t i;
	uint32_t bank = 0;
	uint32_t bit = 0;
	uint32_t reg;
	for (i = 0; i < n; i++) {
		bank = caps[i].bit / 32;
		bit = caps[i].bit % 32;
		WRITE32(regs->DeviceFeaturesSel, bank);
		mb();
		reg = READ32(regs->DeviceFeatures);
		if (reg & (1 << caps[i].bit)) {
			if (caps[i].support) {
				WRITE32(regs->DriverFeaturesSel, bank);
				WRITE32(regs->DriverFeatures,
					READ32(regs->DriverFeatures) | (1 << caps[i].bit));
			} else {
				printf("virtio supports unsupported option %s "
				       "(%s)\n",
				       caps[i].name, caps[i].help);
			}
		}
	}
}

static int virtio_dev_init(uint32_t virt, uint32_t intid)
{
	virtio_regs *regs = (virtio_regs *)virt;

	if (READ32(regs->MagicValue) != VIRTIO_MAGIC) {
		printf("error: virtio at 0x%x had wrong magic value 0x%x, "
		       "expected 0x%x\n",
		       virt, regs->MagicValue, VIRTIO_MAGIC);
		return -1;
	}
	if (READ32(regs->Version) != VIRTIO_VERSION) {
		printf("error: virtio at 0x%x had wrong version 0x%x, expected "
		       "0x%x\n",
		       virt, regs->Version, VIRTIO_VERSION);
		return -1;
	}
	if (READ32(regs->DeviceID) == 0) {
		/*On QEMU, this is pretty common, don't print a message */
		/*printf("warn: virtio at 0x%x has DeviceID=0, skipping\n",
		 * virt);*/
		return -1;
	}

	/* First step of initialization: reset */
	WRITE32(regs->Status, 0);
	mb();
	/* Hello there, I see you */
	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_ACKNOWLEDGE);
	mb();

	/* Hello, I am a driver for you */
	WRITE32(regs->Status, READ32(regs->Status) | VIRTIO_STATUS_DRIVER);
	mb();

	switch (READ32(regs->DeviceID)) {
	case VIRTIO_DEV_BLK:
		return virtio_blk_init(regs, intid);
	case VIRTIO_DEV_NET:
		return virtio_net_init(regs, intid);
	default:
		printf("unsupported virtio device ID 0x%x\n",
		       READ32(regs->DeviceID));
	}
}

void virtio_init(void)
{
	/* TODO: we know these addresses due to manually reading device tree,
	 * but we should automate that */
	uint32_t page_virt = alloc_pages(kern_virt_allocator, 0x4000, 0);
	kmem_map_pages(page_virt, 0x0a000000U, 0x4000, PRW_UNA | EXECUTE_NEVER);

	for (int i = 0; i < 32; i++)
		virtio_dev_init(page_virt + 0x200 * i, 32 + 0x10 + i);
}
