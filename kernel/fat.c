#include "fat.h"
#include "blk.h"
#include "kernel.h"
#include "string.h"

#define BPB_FATSz(fat)                                                         \
	(fat->bpb->BPB_FATSz16 ? fat->bpb->BPB_FATSz16                         \
	                       : fat->bpb32->BPB_FATSz32)
#define BPB_TotSec(fat)                                                        \
	(fat->bpb->BPB_TotSec16 ? fat->bpb->BPB_TotSec16                       \
	                        : fat->bpb->BPB_TotSec32)
#define BS_FilSysType(fat)                                                     \
	(fs->type == FAT32 ? fat->bpb32->BS_FilSysType                         \
	                   : fat->bpb1216->BS_FilSysType)

char *fstype[3] = {
	[FAT12] = "FAT12",
	[FAT16] = "FAT16",
	[FAT32] = "FAT32",
};

struct fat_fs *fs_global;

void fat_init(struct blkdev *dev)
{
	struct blkreq *req;
	struct fat_fs *fs = kmalloc(sizeof(struct fat_fs));
	uint32_t RootDirSectors, DataSec, CountofClusters;

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

	fs->bpb = (struct fat_bpb *)req->buf;
	fs->bpbvoid = (void *)req->buf + sizeof(struct fat_bpb);
	dev->ops->free(dev, req);
	RootDirSectors = ((fs->bpb->BPB_RootEntCnt * 32) +
	                  (fs->bpb->BPB_BytsPerSec - 1)) /
	                 fs->bpb->BPB_BytsPerSec;
	DataSec = BPB_TotSec(fs) -
	          (fs->bpb->BPB_RsvdSecCnt +
	           (fs->bpb->BPB_NumFATs * BPB_FATSz(fs)) + RootDirSectors);
	CountofClusters = DataSec / fs->bpb->BPB_SecPerClus;

	if (CountofClusters < 4085) {
		fs->type = FAT12;
	} else if (CountofClusters < 65525) {
		fs->type = FAT16;
	} else {
		fs->type = FAT32;
	}
	fs->RootDirSectors = RootDirSectors;
	fs->DataSec = DataSec;
	fs->CountofClusters = CountofClusters;
	if (!strprefix(BS_FilSysType(fs), fstype[fs->type]))
		puts("NB: detected FAT type mismatch with recorded one\n");
	fs->FatSec1 = fs->bpb->BPB_RsvdSecCnt;
	fs->FatSec2 = fs->FatSec2 + BPB_FATSz(fs);
	fs->RootSec =
	        fs->bpb->BPB_RsvdSecCnt + fs->bpb->BPB_NumFATs * BPB_FATSz(fs);

	printf("  OEMName: \"%s\"\n", fs->bpb->BS_OEMName);
#define showint(name) printf("  " #name ": %u\n", fs->bpb->name)
	showint(BPB_BytsPerSec);
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

	printf("  CountofClusters: %u\n", CountofClusters);
	printf("  RootDirSectors: %u\n", RootDirSectors);
	printf("  DataSectors: %u\n", DataSec);
	printf("  Root Directory Sector: %u\n", fs->RootSec);
	printf("  We determined fstype: \"%s\"\n", fstype[fs->type]);
	fs_global = fs;
	return;

out:
	kfree(req->buf, dev->blksiz);
	dev->ops->free(dev, req);
	kfree(fs, sizeof(*fs));
}

void fat_list_root(struct fat_fs *fs)
{
	/* read first sector of root dir */
	struct fat_dirent *dirent = kmalloc(512);
	struct blkreq *req = fs->dev->ops->alloc(fs->dev);
	char fn[12];
	uint32_t fstclus;

	req->blkidx = fs->RootSec;
	req->type = BLKREQ_READ;
	req->buf = dirent;
	req->size = 512;
	fs->dev->ops->submit(fs->dev, req);
	wait_for(&req->wait);
	fs->dev->ops->free(fs->dev, req);

	for (uint32_t i = 0; i < (512 / 32); i++) {
		if (dirent[i].DIR_Name[0] == 0xE5) {
			continue; /* don't clutter with free entries */
		}
		if (dirent[i].DIR_Name[0] == 0) {
			printf("  [%u] No more data!\n", i);
			break;
		}
		if ((dirent[i].DIR_Attr & FAT_ATTR_LONG_NAME_MASK) ==
		    FAT_ATTR_LONG_NAME) {
			printf("  [%u]: LONG_NAME\n", i);
		} else {
			fstclus = dirent[i].DIR_FstClusHI << 16 |
			          dirent[i].DIR_FstClusLO;
			uint8_t attr = dirent[i].DIR_Attr;
			strlcpy(fn, dirent[i].DIR_Name, sizeof(fn));
			printf("  [%u]: \"%s\"%s%s%s%s%s%s: cluster %u\n", i,
			       fn, (attr & FAT_ATTR_READ_ONLY) ? " RO" : "",
			       (attr & FAT_ATTR_HIDDEN) ? " HID" : "",
			       (attr & FAT_ATTR_SYSTEM) ? " SYS" : "",
			       (attr & FAT_ATTR_VOLUME_ID) ? " VID" : "",
			       (attr & FAT_ATTR_DIRECTORY) ? " DIR" : "",
			       (attr & FAT_ATTR_ARCHIVE) ? " ARC" : "",
			       fstclus);
		}
	}
	kfree(dirent, 512);
}

void *fat_read_blockpair(struct blkdev *dev, uint64_t blkno)
{
	struct blkreq *req = dev->ops->alloc(dev);
	struct blkreq *req2 = dev->ops->alloc(dev);
	void *buf = kmalloc(1024);
	req->buf = buf;
	req->size = 512;
	req->type = BLKREQ_READ;
	req->blkidx = blkno;
	req2->buf = buf + 512;
	req2->size = 512;
	req2->type = BLKREQ_READ;
	req2->blkidx = blkno + 1;
	dev->ops->submit(dev, req);
	dev->ops->submit(dev, req2);
	wait_for(&req->wait);
	wait_for(&req2->wait);
	if (req->status != BLKREQ_OK || req2->status != BLKREQ_OK) {
		kfree(buf, 512);
		dev->ops->free(dev, req);
		dev->ops->free(dev, req2);
		return NULL;
	}
	dev->ops->free(dev, req);
	dev->ops->free(dev, req2);
	return buf;
}

void fat_iter_fat12(struct fat_fs *fs)
{
	int32_t freebegin = -1;
	uint32_t curblkno = 0; /* initial block is 0, we know FAT will be > 0 */
	uint32_t totcluster = BPB_TotSec(fs) / fs->bpb->BPB_SecPerClus;
	uint32_t i;
	uint8_t *blk = NULL;
	printf("  totcluster %u\n", totcluster);
	for (i = 0; i < totcluster; i++) {
		uint32_t offset = i + i / 2;
		uint32_t blkno = offset / fs->bpb->BPB_BytsPerSec +
		                 fs->bpb->BPB_RsvdSecCnt;
		uint32_t byte = offset % fs->bpb->BPB_BytsPerSec;

		if (blkno > curblkno) {
			if (blk)
				kfree(blk, 1024);
			blk = fat_read_blockpair(fs->dev, blkno);
			curblkno = blkno;
		}

		uint16_t val = *(uint16_t *)&blk[byte];
		if (i & 1) {
			val >>= 4;
		} else {
			val &= 0xFFF;
		}

		if (val == 0) {
			if (freebegin == -1)
				freebegin = i;
		} else if (val < totcluster) {
			if (freebegin != -1) {
				printf("  [%u-%u]: free\n", freebegin, i - 1);
				freebegin = -1;
			}
			printf("  [%u]: next cluster %u\n", i, val);
		} else if (val == 0xFF7) {
			if (freebegin != -1) {
				printf("  [%u-%u]: free\n", freebegin, i - 1);
				freebegin = -1;
			}
			printf("  [%u]: bad cluster (0x%x)\n", i, val);
		} else {
			if (freebegin != -1) {
				printf("  [%u-%u]: free\n", freebegin, i - 1);
				freebegin = -1;
			}
			printf("  [%u]: eof (%u)\n", i, val);
		}
	}
	if (freebegin != -1) {
		printf("  [%u-%u]: free\n", freebegin, i - 1);
		freebegin = -1;
	}
	kfree(blk, 1024);
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
	puts("INITIALIZE FAT DISK\n");
	fat_init(dev);
	puts("ROOT DIRECTORY CONTENTS:\n");
	fat_list_root(fs_global);
	puts("FILE ALLOCATION TABLE:\n");
	fat_iter_fat12(fs_global);
	return 0;
}
