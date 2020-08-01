#pragma once
#include <stdint.h>

#include "list.h"
#include "slab.h"

struct fs_node;
struct file;
struct file_ops;
struct fs;
struct fs_ops;

struct file_ops {
	int (*read)(int fd, void *dst, size_t amt);
};

struct file {
	struct file_ops *ops;
	struct fs_node *node;
};

struct fs_ops {
	int (*fs_list)(struct fs_node *node);
	struct file *(*fs_open)(struct fs_node *node);
};

struct fs {
	enum { FS_FAT,
	} fs_type;
	struct fs_ops *fs_ops;
	struct fs_node *fs_root;
};

#define FILENAME_MAX 128
struct fs_node {
	struct fs_node *parent;
	struct list_head list; /* for containing in the parent's list */
	struct list_head children;
	int nchildren;
	char name[FILENAME_MAX];
	unsigned short namelen;
	enum { FSN_FILE,     // regular file
	       FSN_DIR,      // directory which has also been loaded
	       FSN_LAZY_DIR, // directory which is not yet loaded
	} type;
	uint64_t location;
	struct fs *fs;
};

extern struct slab *fs_node_slab;
extern struct fs_node *fs_root;
void fs_init(void);
void fs_reset_dir(struct fs_node *node);
int fs_resolve(const char *path, struct fs_node **out);
