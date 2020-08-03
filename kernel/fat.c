#include "fat.h"
#include "blk.h"
#include "kernel.h"
#include "ksh.h"
#include "list.h"
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

#define FAT_EOF 0xFFFFFFFFFFFFFFFFL
#define FAT_ERR 0xFFFFFFFFFFFFFFFEL
char *fstype[3] = {
	[FAT12] = "FAT12",
	[FAT16] = "FAT16",
	[FAT32] = "FAT32",
};

struct fat_fs *fs_global;

#define clus_bytes(fs)                                                         \
	(((struct fat_fs *)(fs))->bpb->BPB_BytsPerSec *                        \
	 ((struct fat_fs *)(fs))->bpb->BPB_SecPerClus)
#define sec_bytes(fs) (((struct fat_fs *)(fs))->bpb->BPB_BytsPerSec)

uint32_t fat_cluster_to_block(struct fat_fs *fs, uint32_t clusno)
{
	return (fs->RootSec + fs->RootDirSectors +
	        (clusno - 2) * fs->bpb->BPB_SecPerClus);
}

#define FAT_MAX_SHORTNAME 11
/* Bitmap mapping saying whether a byte is an invalid character */
uint8_t fat_invalid_char[256 / 8] = {
	[0] = 0xff, [1] = 0xff, [2] = 0xff,  [3] = 0xff,  [4] = 0x4,
	[5] = 0xdc, [7] = 0xfc, [11] = 0x38, [15] = 0x10,
};
/**
 * Simultaneously determine if "input" is a valid short name, and if so, copy it
 * to the correct format of dest. Return <0 if the name is invalid, or 0 if the
 * name is valid. Callers should assume that the value of dest is not preserved,
 * even if the name is invalid. The validation step will check the following:
 * - length of total filename is no more than 11
 * - length of basename is no more than 8
 * - extension is no more than 3
 * - convert lower case to upper
 * - ensure no illegal characters are in name
 * The array at dest will be null terminated (dest[10] = '\0') to ensure that
 * printing out the filename is legal.
 *
 * Some sample calls:
 *
 * "hi world" -> -1
 * "file1" -> "FILE1      "
 */
int fat_valid_shortname(char *input, char *dest)
{
	uint32_t i, j, maxval;
	if (strlen(input) > FAT_MAX_SHORTNAME + 1) /* +1 to account for '.' */
		return -1;

	/* Copy over the base (without extension) */
	for (i = 0, j = 0; input[i] && (i < 8); i++) {
		if (input[i] == '.') {
			break;
		} else if (fat_invalid_char[input[i] >> 3] &
		           (1 << (input[i] & 0x7))) {
			return -EINVAL;
		} else if (input[i] >= 'a' && input[i] <= 'z') {
			dest[j++] = input[i] + ('A' - 'a');
		} else {
			dest[j++] = input[i];
		}
	}

	/* SPACE PAD the base */
	for (; j < 8; j++)
		dest[j] = ' ';

	if (input[i]) {
		if (input[i] != '.') {
			return -ENAMETOOLONG;
		}
		i++;
		maxval = i + 3;
		for (; input[i] && (i < maxval); i++) {
			if (fat_invalid_char[input[i] >> 3] &
			    (1 << (input[i] & 0x7))) {
				return -EINVAL;
			} else if (input[i] >= 'a' && input[i] <= 'z') {
				dest[j++] = input[i] + ('A' - 'a');
			} else {
				dest[j++] = input[i];
			}
		}
		if (input[i]) {
			return -ENAMETOOLONG; /* too long extension */
		}
	}

	/* SPACE PAD extension */
	for (; j < 11; j++)
		dest[j] = ' ';
	dest[11] = '\0'; /* NUL terminate for good measure */
	return 0;
}

int fat_extract_shortname(char *input, char *dest)
{
	uint32_t i = 0, j;
	for (j = 0; j < 8 && input[j] != ' '; j++)
		dest[i++] = input[j];
	if (input[8] != ' ')
		dest[i++] = '.';
	for (j = 8; j < 11 && input[j] != ' '; j++)
		dest[i++] = input[j];
	dest[i] = '\0';
	return i;
}

int fat_read_sector(struct fat_fs *fs, uint64_t sector, void *dst)
{
	int rv = 0;
	struct blkreq *req = fs->dev->ops->alloc(fs->dev);
	req->blkidx = sector;
	req->type = BLKREQ_READ;
	req->buf = dst;
	/* TODO: this could be different from the device block size */
	req->size = fs->bpb->BPB_BytsPerSec;
	fs->dev->ops->submit(fs->dev, req);
	wait_for(&req->wait);
	if (req->status != BLKREQ_OK)
		rv = -EIO;
	fs->dev->ops->free(fs->dev, req);
	return rv;
}

int fat_read_cluster(struct fat_fs *fs, uint64_t cluster, void *dst)
{
	int i, rv = 0;
	int nblk = clus_bytes(fs) / fs->dev->blksiz;
	struct blkreq *first, *req;
	enum blkreq_status status;

	first = fs->dev->ops->alloc(fs->dev);
	first->blkidx = fat_cluster_to_block(fs, cluster);
	first->type = BLKREQ_READ;
	first->buf = dst;
	first->size = fs->dev->blksiz;
	fs->dev->ops->submit(fs->dev, first);

	for (i = 1; i < nblk; i++) {
		req = fs->dev->ops->alloc(fs->dev);
		req->blkidx = first->blkidx + i * fs->dev->blksiz;
		req->type = BLKREQ_READ;
		req->buf = dst + i * fs->dev->blksiz;
		req->size = fs->dev->blksiz;
		list_insert_end(&first->reqlist, &req->reqlist);
		fs->dev->ops->submit(fs->dev, req);
	}

	status = blkreq_wait_all(first);
	if (status != BLKREQ_OK)
		rv = -EIO;
	blkreq_free_all(fs->dev, first);
	return rv;
}

int fat_print_root(struct fat_fs *fs)
{
	/* read first sector of root dir */
	const unsigned int bps = fs->bpb->BPB_BytsPerSec;
	struct fat_dirent *dirent = kmalloc(bps);
	char fn[12];
	uint32_t fstclus;
	int rv = 0;

	rv = fat_read_sector(fs, fs->RootSec, dirent);
	if (rv < 0) {
		kfree(dirent, bps);
		return rv;
	}

	for (uint32_t i = 0; i < (bps / sizeof(dirent[0])); i++) {
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
	kfree(dirent, bps);
	return rv;
}

int fat_list_chunk(struct fat_fs *fs, struct fs_node *node,
                   struct fat_dirent *dirent, unsigned int count)
{
	struct fs_node *child;
	uint8_t attr;

	for (uint32_t i = 0; i < count / sizeof(dirent[0]); i++) {
		if (dirent[i].DIR_Name[0] == 0xE5) {
			continue;
		}
		if (dirent[i].DIR_Name[0] == 0) {
			return 1;
		}
		if ((dirent[i].DIR_Attr & FAT_ATTR_LONG_NAME_MASK) ==
		    FAT_ATTR_LONG_NAME) {
			// TODO: support long names
			// printf("  [%u]: LONG_NAME\n", i);
		} else {
			attr = dirent[i].DIR_Attr;

			child = slab_alloc(fs_node_slab);
			child->parent = node;
			INIT_LIST_HEAD(child->children);
			child->nchildren = 0;
			child->namelen = fat_extract_shortname(
			        dirent[i].DIR_Name, child->name);
			if (attr & FAT_ATTR_DIRECTORY)
				child->type = FSN_LAZY_DIR;
			else
				child->type = FSN_FILE;
			child->size = dirent[i].DIR_FileSize;
			child->location = dirent[i].DIR_FstClusHI << 16 |
			                  dirent[i].DIR_FstClusLO;
			child->fs = (struct fs *)fs;

			list_insert_end(&node->children, &child->list);
			node->nchildren++;
		}
	}
	return 0;
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

uint64_t fat12_next_cluster(struct fat_fs *fs, uint64_t cluster)
{
	uint8_t *blk = NULL;
	uint32_t totcluster = BPB_TotSec(fs) / fs->bpb->BPB_SecPerClus;

	uint32_t offset = (uint32_t)cluster + (uint32_t)cluster / 2;
	uint32_t blkno =
	        offset / fs->bpb->BPB_BytsPerSec + fs->bpb->BPB_RsvdSecCnt;
	uint32_t byte = offset % fs->bpb->BPB_BytsPerSec;
	uint64_t rv = 0;

	blk = fat_read_blockpair(fs->dev, blkno);

	uint16_t val = *(uint16_t *)&blk[byte];
	if (cluster & 1) {
		val >>= 4;
	} else {
		val &= 0xFFF;
	}

	if (val >= 2 && val < totcluster) {
		rv = val;
	} else if (val < 2) {
		/* cluster is marked free, this is a bug! */
		puts("BUG: fat12_next_cluster(): cluster already free\n");
		rv = FAT_ERR;
	} else if (val >= totcluster && val < 0xFF7) {
		/* using reserved cluster number, bug! */
		puts("BUG: fat12_next_cluster(): reserved cluster number\n");
		rv = FAT_ERR;
	} else if (val == 0xFF7) {
		puts("BUG: fat12_next_cluster(): bad cluster\n");
		rv = FAT_ERR;
	} else if (val >= 0xFF8) {
		rv = FAT_EOF;
	}

	kfree(blk, 2 * fs->bpb->BPB_BytsPerSec);
	return rv;
}

uint64_t fat_next_cluster(struct fat_fs *fs, uint64_t cluster)
{
	switch (fs->type) {
	case FAT12:
		return fat12_next_cluster(fs, cluster);
	default:
		puts("unsupported fat type\n");
		return FAT_ERR;
	}
}

int fat_list_root(struct fat_fs *fs, struct fs_node *node)
{
	/* read first sector of root dir */
	const unsigned int bps = fs->bpb->BPB_BytsPerSec;
	struct fat_dirent *dirent = kmalloc(bps);
	uint32_t i;
	int rv = 0;

	for (i = 0; i < fs->bpb->BPB_RootEntCnt; i++) {
		rv = fat_read_sector(fs, fs->RootSec + i, dirent);
		if (rv < 0) {
			kfree(dirent, bps);
			/* Cleanup the node so that if we retry, we won't have
			 * duplicate entries. */
			fs_reset_dir(node);
			return rv;
		}
		rv = fat_list_chunk(fs, node, dirent, bps);
		if (rv == 1) {
			rv = 0;
			break;
		}
	}

	kfree(dirent, bps);
	node->type = FSN_DIR;
	return rv;
}

int fat_read(struct file *f, void *dst, size_t amt)
{
	const unsigned int clusiz = clus_bytes(f->node->fs);
	struct fat_file_private *priv = fat_priv(f);
	struct fat_fs *fs = (struct fat_fs *)f->node->fs;
	uint8_t *buf = kmalloc(clusiz);

	uint64_t endpos = f->pos + amt;
	int rv = 0;
	size_t bytes = 0;
	size_t blkstart, blkend;

	if (endpos > f->node->size) {
		endpos = f->node->size;
	}

	while (f->pos < endpos) {
		blkstart = (uint32_t)f->pos % clusiz;
		blkend = clusiz;
		if (endpos < (f->pos - blkstart + clusiz))
			blkend = (uint32_t)endpos % clusiz;

		if (blkstart == 0 && blkend == clusiz) {
			/* read directly into dst, bypassing copy */
			rv = fat_read_cluster(fs, priv->current_cluster,
			                      dst + bytes);
			if (rv < 0)
				goto out;
		} else {
			/* read into malloc buffer and copy */
			rv = fat_read_cluster(fs, priv->current_cluster, buf);
			if (rv < 0)
				goto out;
			memcpy(dst + bytes, buf + blkstart, blkend - blkstart);
		}
		bytes += blkend - blkstart;
		f->pos += bytes;

		if (blkend == clusiz)
			priv->current_cluster =
			        fat_next_cluster(fs, priv->current_cluster);
	}

	rv = bytes;
out:
	kfree(buf, clusiz);
	return rv;
}

int fat_close(struct file *f)
{
	fs_free_file(f);
	return 0;
}

struct file_ops fat_file_ops = {
	.read = fat_read,
	.close = fat_close,
};

int fat_list(struct fs_node *node)
{
	struct fat_fs *fs = node->fs;
	struct fat_dirent *dirent = kmalloc(clus_bytes(fs));
	int rv = 0;
	uint64_t clus;

	for (clus = node->location; clus != FAT_EOF;
	     clus = fat_next_cluster(fs, clus)) {
		if (clus == FAT_ERR) {
			kfree(dirent, clus_bytes(fs));
			fs_reset_dir(node);
			return -EIO;
		}

		rv = fat_read_cluster(fs, clus, dirent);
		if (rv != 0) {
			kfree(dirent, clus_bytes(fs));
			fs_reset_dir(node);
			return rv;
		}
		rv = fat_list_chunk(fs, node, dirent, clus_bytes(fs));
		if (rv == 1) {
			rv = 0;
			break;
		}
	}
	kfree(dirent, clus_bytes(fs));
	node->type = FSN_DIR;
	return rv;
}

struct file *fat_open(struct fs_node *node, int flags)
{
	struct file *file = fs_alloc_file();
	struct fat_file_private *priv = fat_priv(file);

	file->ops = &fat_file_ops;
	file->node = node;
	file->pos = 0;
	priv->first_cluster = node->location;
	priv->current_cluster = node->location;
	return file;
}

struct fs_ops fat_fs_ops = {
	.fs_list = fat_list,
	.fs_open = fat_open,
};

void fat_init(struct blkdev *dev)
{
	struct blkreq *req;
	struct fat_fs *fs = kmalloc(sizeof(struct fat_fs));
	uint32_t RootDirSectors, DataSec, CountofClusters;

	/*
	 * TODO: check whether BPB_BytsPerSec is less than device block size
	 */

	fs->dev = dev;
	fs->fs.fs_type = FS_FAT;
	fs->fs.fs_root = fs_root;
	fs->fs.fs_ops = &fat_fs_ops;

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

	/* Add root directory contents to root */
	fat_list_root(fs, fs_root);
	fs_global = fs;
	fs_root->fs = fs;
	return;

out:
	kfree(req->buf, dev->blksiz);
	dev->ops->free(dev, req);
	kfree(fs, sizeof(*fs));
}

static int cmd_fat(int argc, char **argv)
{
	struct blkdev *dev;
	if (argc != 1) {
		puts("usage: fat init BLKNAME\n");
		return 1;
	}
	dev = blkdev_get_by_name(argv[0]);
	if (!dev) {
		printf("no such blockdev \"%s\"\n", argv[0]);
		return 1;
	}
	fat_init(dev);
	return 0;
}

static void fat12_iter(struct fat_fs *fs)
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

static int cmd_fat_iter(int argc, char **argv)
{
	if (!fs_global) {
		puts("No file system initialized\n");
		return 0;
	}
	switch (fs_global->type) {
	case FAT12:
		fat12_iter(fs_global);
		break;
	default:
		puts("Unsupported FAT type\n");
		break;
	}
	return 0;
}

struct ksh_cmd fat_ksh_cmds[] = {
	KSH_CMD("init", cmd_fat, "initialize FAT filesystem"),
	KSH_CMD("iter", cmd_fat_iter, "iterate over file allocation table"),
	{ 0 },
};
