#include "fat.h"
#include "alloc.h"
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

int fat_clusop(struct blkdev *dev, uint64_t block, void *dst, int op, int nblk)
{
	int i, rv = 0;
	struct blkreq *first, *req;
	enum blkreq_status status;

	first = dev->ops->alloc(dev);
	first->blkidx = block;
	first->type = op;
	first->buf = dst;
	first->size = dev->blksiz;
	dev->ops->submit(dev, first);

	for (i = 1; i < nblk; i++) {
		req = dev->ops->alloc(dev);
		req->blkidx = block + i;
		req->type = op;
		req->buf = dst + i * dev->blksiz;
		req->size = dev->blksiz;
		list_insert_end(&first->reqlist, &req->reqlist);
		dev->ops->submit(dev, req);
	}

	status = blkreq_wait_all(first);
	if (status != BLKREQ_OK)
		rv = -EIO;
	blkreq_free_all(dev, first);
	return rv;
}

static inline int fat_read_cluster(struct fat_fs *fs, uint64_t cluster,
                                   void *dst)
{
	int nblk = clus_bytes(fs) / fs->dev->blksiz;
	uint64_t block = fat_cluster_to_block(fs, cluster);
	return fat_clusop(fs->dev, block, dst, BLKREQ_READ, nblk);
}

static inline int fat_write_cluster(struct fat_fs *fs, uint64_t cluster,
                                    void *src)
{
	int nblk = clus_bytes(fs) / fs->dev->blksiz;
	uint64_t block = fat_cluster_to_block(fs, cluster);
	return fat_clusop(fs->dev, block, src, BLKREQ_WRITE, nblk);
}

int fat_read_sector(struct fat_fs *fs, uint64_t sector, void *dst)
{
	int nblk = sec_bytes(fs) / fs->dev->blksiz;
	return fat_clusop(fs->dev, sector * nblk, dst, BLKREQ_READ, nblk);
}

int fat_write_sector(struct fat_fs *fs, uint64_t sector, void *dst)
{
	int nblk = sec_bytes(fs) / fs->dev->blksiz;
	return fat_clusop(fs->dev, sector * nblk, dst, BLKREQ_WRITE, nblk);
}

void *fat_read_blockpair(struct blkdev *dev, uint64_t blkno)
{
	void *buf = kmalloc(2 * dev->blksiz);
	fat_clusop(dev, blkno, buf, BLKREQ_READ, 2);
	return buf;
}

void fat_write_blockpair(struct blkdev *dev, uint64_t blkno, void *buf)
{
	fat_clusop(dev, blkno, buf, BLKREQ_READ, 2);
}

/**
 * Read the file allocation table entry for cluster `clusno`.
 *
 * @param fs FAT12 filesystem to operate on
 * @param clusno the cluster whose entry should be read (12 bits)
 * @returns the entry for `clusno` (12 bits)
 */
static inline uint16_t fat12_read_fat(struct fat_fs *fs, uint16_t clusno)
{
	uint16_t byte = clusno + (clusno >> 1);
	uint16_t value = *(uint16_t *)&fs->fat[byte];
	if (clusno & 1) {
		return value >> 4;
	} else {
		return value & 0xFFF;
	}
}

/**
 * Set the in-memory file allocation table entry for cluster `clusno`.
 *
 * Returns the index of the first byte of the FAT which was modified. The last
 * byte of the FAT which was modified is the return value plus one. The caller
 * must be sure to write all modified blocks back to the filesystem in order for
 * the in-memory view to become consistent.
 *
 * @param fs FAT12 filesystem to operate on
 * @param clusno the cluster whose entry to write (12 bits)
 * @param value_to_write the value to write (12 bits)
 * @returns the index of the first byte modified within the FAT
 */
static inline uint16_t fat12_set_fat(struct fat_fs *fs, uint16_t clusno,
                                     uint16_t value_to_write)
{
	uint16_t byte = clusno + (clusno >> 1);
	uint16_t value = *(uint16_t *)&fs->fat[byte];
	if (clusno & 1) {
		value &= 0x000F;
		value |= (value_to_write << 4);
	} else {
		value &= 0xF000;
		value |= value_to_write;
	}
	*(uint16_t *)&fs->fat[byte] = value;
	return byte;
}

/**
 * Write the in-memory file allocation table to disk.
 *
 * @param fs FAT12 filesystem to operate on
 * @param first first byte which was modified
 * @param last last byte which was modified
 * @returns 0 or an error code
 */
static inline int fat12_write_fat(struct fat_fs *fs, uint16_t first,
                                  uint16_t last)
{
	uint32_t secsiz = sec_bytes(fs);
	uint32_t first_blk = first / secsiz;
	uint32_t last_blk = last / secsiz;
	int rv = 0;

	uint32_t FAT_sector = 1;
	printf("fat12_write_fat(): first=%d, last=%d, first_blk=%d"
	       " last_blk=%d\n",
	       first, last, first_blk, last_blk);
	rv = fat_clusop(fs->dev, FAT_sector + first_blk,
	                &fs->fat[first_blk * secsiz], BLKREQ_WRITE,
	                last_blk - first_blk + 1);
	if (rv < 0)
		return rv;
	FAT_sector += BPB_FATSz(fs);
	rv = fat_clusop(fs->dev, FAT_sector + first_blk,
	                &fs->fat[first_blk * secsiz], BLKREQ_WRITE,
	                last_blk - first_blk + 1);
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

int fat_update_size_chunk(struct fat_fs *fs, struct fs_node *node,
                          struct fat_dirent *dirent, unsigned int count,
                          uint32_t size)
{
	char name[12];

	for (uint32_t i = 0; i < count / sizeof(dirent[0]); i++) {
		if (dirent[i].DIR_Name[0] == 0xE5) {
			continue;
		}
		if (dirent[i].DIR_Name[0] == 0) {
			return -1;
		}
		if ((dirent[i].DIR_Attr & FAT_ATTR_LONG_NAME_MASK) ==
		    FAT_ATTR_LONG_NAME) {
			// TODO: support long names
			// printf("  [%u]: LONG_NAME\n", i);
		} else {
			fat_extract_shortname(dirent[i].DIR_Name, name);
			if (strcmp(name, node->name) == 0) {
				dirent[i].DIR_FileSize = size;
				node->size = size;
				return 1;
			}
		}
	}
	return 0;
}

uint64_t fat12_next_cluster(struct fat_fs *fs, uint64_t cluster)
{
	uint32_t totcluster = BPB_TotSec(fs) / fs->bpb->BPB_SecPerClus;
	uint64_t rv = 0;
	uint16_t val = fat12_read_fat(fs, cluster);

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

	return rv;
}

int fat12_set_next_cluster(struct fat_fs *fs, uint64_t cluster, uint64_t next)
{
	uint16_t first = fat12_set_fat(fs, (uint16_t)cluster, (uint16_t)next);
	return fat12_write_fat(fs, first, first + 1);
}

uint64_t fat12_alloc_cluster(struct fat_fs *fs, uint64_t prev)
{
	uint32_t totcluster = BPB_TotSec(fs) / fs->bpb->BPB_SecPerClus;
	uint32_t i;
	uint16_t val;
	int rv;

	for (i = 2; i < totcluster; i++) {
		val = fat12_read_fat(fs, i);

		if (val == 0) {
			/* Mark this cluster as allocated, 0xFFF is EOF */
			rv = fat12_set_next_cluster(fs, i, 0xFFF);
			if (rv < 0)
				return FAT_ERR;

			/* Mark i as next sector for prev */
			rv = fat12_set_next_cluster(fs, prev, i);
			if (rv < 0)
				return FAT_ERR;
			return (uint64_t)i;
		}
	}
	return FAT_EOF;
}

uint64_t fat_alloc_cluster(struct fat_fs *fs, uint64_t cluster)
{
	switch (fs->type) {
	case FAT12:
		return fat12_alloc_cluster(fs, cluster);
	default:
		puts("unsupported fat type\n");
		return FAT_ERR;
	}
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

int fat_update_size_root(struct fat_fs *fs, struct fs_node *node, uint64_t size)
{
	const unsigned int bps = fs->bpb->BPB_BytsPerSec;
	struct fat_dirent *dirent = kmalloc(bps);
	uint32_t i;
	int rv = 0;
	for (i = 0; i < fs->bpb->BPB_RootEntCnt; i++) {
		rv = fat_read_sector(fs, fs->RootSec + i, dirent);
		if (rv < 0)
			break;
		rv = fat_update_size_chunk(fs, node, dirent, bps, size);
		if (rv == 1) {
			rv = fat_write_sector(fs, fs->RootSec + i, dirent);
			break; // success
		} else if (rv == -1) {
			rv = -1;
			break; // fail
		}
		// otherwise, continue
	}
	kfree(dirent, bps);
	return rv;
}

int fat_update_size_nonroot(struct fat_fs *fs, struct fs_node *node,
                            uint64_t size)
{
	struct fat_dirent *dirent = kmalloc(clus_bytes(fs));
	int rv = 0;
	uint64_t clus;

	for (clus = node->parent->location; clus != FAT_EOF;
	     clus = fat_next_cluster(fs, clus)) {
		if (clus == FAT_ERR) {
			rv = -EIO;
			break;
		}

		rv = fat_read_cluster(fs, clus, dirent);
		if (rv != 0)
			break;
		rv = fat_update_size_chunk(fs, node, dirent, clus_bytes(fs),
		                           size);
		if (rv == 1) {
			rv = fat_write_cluster(fs, clus, dirent);
			break; // success
		} else if (rv == -1) {
			break; // fail
		}
		// otherwise, continue
	}
	kfree(dirent, clus_bytes(fs));
	return rv;
}

int fat_update_size(struct fat_fs *fs, struct fs_node *node, uint64_t size)
{
	if (node->parent == fs_root)
		return fat_update_size_root(fs, node, size);
	else
		return fat_update_size_nonroot(fs, node, size);
}

int fat_read(struct file *f, void *dst, size_t amt)
{
	const unsigned int clusiz = clus_bytes(f->node->fs);
	struct fat_file_private *priv = fat_priv(f);
	struct fat_fs *fs = (struct fat_fs *)f->node->fs;
	uint8_t *buf;

	uint64_t endpos = f->pos + amt;
	int rv = 0;
	size_t bytes = 0;
	size_t blkstart, blkend;

	if (!(f->flags & O_READ))
		return -EINVAL;

	buf = kmalloc(clusiz);

	if (endpos > f->node->size) {
		endpos = f->node->size;
	}

	while (f->pos < endpos) {
		blkstart = (uint32_t)f->pos % clusiz;
		blkend = clusiz;
		if (endpos < (f->pos + (blkend - blkstart)))
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
		f->pos += blkend - blkstart;

		if (blkend == clusiz) {
			priv->current_cluster =
			        fat_next_cluster(fs, priv->current_cluster);
		}
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

int fat_write(struct file *f, const uint8_t *src, size_t count)
{
	const unsigned int clusiz = clus_bytes(f->node->fs);
	struct fat_file_private *priv = fat_priv(f);
	struct fat_fs *fs = (struct fat_fs *)f->node->fs;
	uint8_t *buf;
	int rv = 0;

	uint32_t blkstart;
	uint32_t blkend;
	uint32_t bufidx = 0;

	if (!(f->flags & O_WRITE))
		return -EINVAL;

	buf = kmalloc(clusiz);

	while (bufidx < count) {
		blkstart = (uint32_t)f->pos % clusiz;
		blkend = blkstart + (count - bufidx);
		if (blkend > clusiz)
			blkend = clusiz;

		if (blkstart > 0 || blkend < clusiz) {
			// if we are not writing the full cluster, we need to
			// read it in, overwrite the necessary part, and then
			// write it back out
			rv = fat_read_cluster(fs, priv->current_cluster, buf);
			if (rv < 0)
				goto out;
			memcpy(buf + blkstart, src + bufidx, blkend - blkstart);
			rv = fat_write_cluster(fs, priv->current_cluster, buf);
			if (rv < 0)
				goto out;
		} else {
			// if we want to write a full cluster, it is easier
			rv = fat_write_cluster(fs, priv->current_cluster,
			                       src + bufidx);
		}
		f->pos += blkend - blkstart;
		bufidx += blkend - blkstart;
		if (blkend == clusiz) {
			uint64_t next_cluster =
			        fat_next_cluster(fs, priv->current_cluster);

			/* Allocate a new cluster if we are at the end of file.
			 * This routine internally updates the file allocation
			 * table. */
			if (next_cluster == FAT_EOF) {
				next_cluster = fat_alloc_cluster(
				        fs, priv->current_cluster);
				if (next_cluster == FAT_EOF ||
				    next_cluster == FAT_ERR) {
					rv = -EIO;
					goto out;
				}
			}

			priv->current_cluster = next_cluster;
		}
	}

out:
	kfree(buf, clusiz);
	if (f->pos > f->node->size)
		rv = fat_update_size(fs, f->node, f->pos);
	return rv;
}

struct file_ops fat_file_ops = {
	.read = fat_read,
	.write = fat_write,
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

uint64_t fat_get_last_clus(struct fs *fs, uint64_t clus, uint32_t size)
{

	const unsigned int clusiz = clus_bytes(fs);
	while (size > clusiz && clus != FAT_EOF) {
		clus = fat_next_cluster(fs, clus);
		size -= (size > clusiz ? clusiz : size);
	}
	if (size > clusiz || clus == FAT_EOF) {
		puts("BUG: fat_get_last_clus\n");
	}
	return clus;
}

struct file *fat_open(struct fs_node *node, int flags)
{
	struct file *file = fs_alloc_file();
	struct fat_file_private *priv = fat_priv(file);

	file->ops = &fat_file_ops;
	file->node = node;
	file->pos = 0;
	file->flags = flags;
	priv->first_cluster = node->location;
	priv->current_cluster = node->location;
	if (flags & O_APPEND) {
		priv->current_cluster =
		        fat_get_last_clus(node->fs, node->location, node->size);
		file->pos = node->size;
	}
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
	uint32_t RootDirSectors, DataSec, CountofClusters, FATpg;

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

	FATpg = (BPB_FATSz(fs) * sec_bytes(fs)) / 4096 + 1;
	fs->fat = kmem_get_pages(FATpg * 4096, 0);
	fat_clusop(dev, 1, fs->fat, BLKREQ_READ, BPB_FATSz(fs));

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
