#include "blk.h"
#include "kernel.h"
#include "string.h"
#include "wait.h"

static struct list_head blkdev_list;
static spinsem_t blkdev_list_lock;

void blkreq_init(struct blkreq *req)
{
	wait_list_init(&req->wait);
	INIT_LIST_HEAD(req->reqlist);
}

void blk_init(void)
{
	INIT_SPINSEM(&blkdev_list_lock, 1);
	INIT_LIST_HEAD(blkdev_list);
}

void blkdev_register(struct blkdev *dev)
{
	int flags;
	spin_acquire_irqsave(&blkdev_list_lock, &flags);
	list_insert_end(&blkdev_list, &dev->blklist);
	spin_release_irqrestore(&blkdev_list_lock, &flags);
	printf("blk: registered device \"%s\"\n", dev->name);
}

struct blkdev *blkdev_get_by_name(char *name)
{
	struct blkdev *dev;
	int flags;
	spin_acquire_irqsave(&blkdev_list_lock, &flags);
	list_for_each_entry(dev, &blkdev_list, blklist, struct blkdev)
	{
		if (strcmp(dev->name, name) == 0) {
			spin_release_irqrestore(&blkdev_list_lock, &flags);
			return dev;
		}
	}
	spin_release_irqrestore(&blkdev_list_lock, &flags);
	return NULL;
}

int blk_cmd_status(int argc, char **argv)
{
	struct blkdev *dev;
	if (argc != 2) {
		puts("usage: blkstatus BLKNAME\n");
		return 1;
	}
	dev = blkdev_get_by_name(argv[1]);
	if (!dev) {
		printf("no such blockdev \"%s\"", argv[1]);
		return 1;
	}
	printf("Block device \"%s\"\n", argv[1]);
	printf("    block size : %d\n", dev->blksiz);
	printf("    block count: %d\n", (uint32_t)dev->blkcnt);
	puts("Device info below:\n");
	dev->ops->status(dev);
	return 0;
}

int blk_cmd_read(int argc, char **argv)
{
	struct blkdev *dev;
	uint32_t rv = 0;
	struct blkreq *req;

	if (argc != 3) {
		puts("usage: blkstatus BLKNAME SECTOR\n");
		return 1;
	}
	dev = blkdev_get_by_name(argv[1]);
	if (!dev) {
		printf("no such blockdev \"%s\"", argv[1]);
		return 1;
	}

	req = dev->ops->alloc(dev);
	req->size = 512;
	req->buf = kmalloc(req->size);
	req->blkidx = atoi(argv[2]);
	dev->ops->submit(dev, req);
	wait_for(&req->wait);
	if (req->status != BLKREQ_OK) {
		puts("ERROR\n");
		rv = 1;
		goto cleanup;
	}
	printf("result: \"%s\"\n", req->buf);
cleanup:
	kfree(req->buf, req->size);
	dev->ops->free(dev, req);
	return rv;
}

int blk_cmd_write(int argc, char **argv)
{
	struct blkdev *dev;
	struct blkreq *req;
	uint32_t len, rv = 0;

	if (argc != 4) {
		puts("usage: blkwrite BLKNAME SECTOR STRING\n");
		return 1;
	}
	dev = blkdev_get_by_name(argv[1]);
	if (!dev) {
		printf("no such blockdev \"%s\"", argv[1]);
		return 1;
	}

	req = dev->ops->alloc(dev);
	req->size = 512;
	req->buf = kmalloc(req->size);
	req->blkidx = atoi(argv[2]);
	len = strlen(argv[3]);
	memcpy(req->buf, argv[3], len + 1);
	dev->ops->submit(dev, req);
	wait_for(&req->wait);
	if (req->status != BLKREQ_OK) {
		puts("ERROR\n");
		rv = 1;
		goto cleanup;
	}
	puts("written!\n");

cleanup:
	kfree(req->buf, req->size);
	dev->ops->free(dev, req);
	return rv;
}
