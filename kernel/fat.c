#include "fat.h"
#include "blk.h"
#include "kernel.h"

void fat_init(struct blkdev *dev)
{
	struct blkreq *req;
	struct fat_fs *fs = kmalloc(sizeof(struct fat_fs));
	struct fat_bpb *bpb;
	fs->dev = dev;

	req = dev->ops->alloc(dev);
	req->type = BLKREQ_READ;
	req->buf = kmalloc(dev->blksiz);
	req->blkidx = 0;
	req->size = dev->blksiz;
	dev->ops->submit(dev, req);
	wait_for(&req->wait);
	if (req->status != BLKREQ_OK)
		goto out;

	bpb = (struct fat_bpb *)req->buf;
	printf("OEMName: \"%s\"\n", bpb->BS_OEMName);
#define showint(name) printf(#name ": %u\n", bpb->name)
	showint(BS_BytsPerSec);
	showint(BPB_SecPerClus);
	showint(BPB_RsvdSecCnt);
	showint(BPB_NumFATs);
	showint(BPB_RootEntCnt);
	showint(BPB_TotSec16);
	showint(BPB_Media);
	showint(BPB_FATSz16);
	showint(BPB_SecPerTrk);
	showint(BPB_NumHeads);
	showint(BPB_HiddSec);
	showint(BPB_TotSec32);

out:
	kfree(req, dev->blksiz);
	dev->ops->free(dev, req);
	kfree(fs, sizeof(*fs));
}

int cmd_fat(int argc, char **argv)
{
	struct blkdev *dev;
	if (argc != 2) {
		puts("usage: fat BLKNAME\n");
		return 1;
	}
	dev = blkdev_get_by_name(argv[1]);
	if (!dev) {
		printf("no such blockdev \"%s\"\n", argv[1]);
		return 1;
	}
	fat_init(dev);
	return 0;
}
