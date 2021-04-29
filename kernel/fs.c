#include <stdint.h>

#include "fs.h"
#include "kernel.h"
#include "ksh.h"
#include "list.h"
#include "string.h"
#include "mm.h"

struct slab *fs_node_slab;
struct slab *file_slab;
struct fs_node *fs_root;

void fs_reset_dir(struct fs_node *node)
{
	struct fs_node *child, *next;
	node->nchildren = 0;
	node->type = FSN_LAZY_DIR;
	list_for_each_entry_safe(child, next, &node->children, list)
	{
		slab_free(fs_node_slab, child);
		list_remove(&child->list);
	}
}

struct fs_node *fs_find_in_dir(struct fs_node *node, const char *name)
{
	struct fs_node *child;
	list_for_each_entry(child, &node->children, list)
	{
		if (strcmp(child->name, name) == 0) {
			return child;
		}
	}
	return NULL;
}

int fs_resolve(const char *path, struct fs_node **out)
{
	struct fs_node *cur, *next;
	const char *pathrem, *end;
	char *name;
	int rv = 0;

	// For now, paths must be absolute.
	if (path[0] == '\0' || path[0] != '/') {
		return -ENOENT;
	}

	if (!fs_root || fs_root->type == FSN_LAZY_DIR) {
		return -ENODEV;
	}

	cur = fs_root;
	pathrem = path + 1;
	name = kmalloc(FILENAME_MAX);
	for (;;) {
		/* Allow multiple slashes to separate things. This also skips
		 * the initial root directory slash. */
		while (*pathrem == '/')
			pathrem++;

		/* Check for another path component. If there is none, we're
		 * done. */
		if (*pathrem == '\0') {
			*out = cur;
			goto out;
		}

		/* We now know there must be another path component. Ensure that
		 * the current node is a directory */
		if (cur->type == FSN_LAZY_DIR)
			cur->fs->fs_ops->fs_list(cur);
		if (cur->type != FSN_DIR) {
			rv = -ENOTDIR;
			goto out;
		}

		/* Isolate just the next component of the path, and search for
		 * it within the current node's children. */
		end = strchrnul(pathrem, '/');
		if (end - pathrem >= FILENAME_MAX) {
			rv = -ENAMETOOLONG;
			goto out;
		}
		strlcpy(name, pathrem, end - pathrem + 1);
		next = fs_find_in_dir(cur, name);
		if (!next) {
			rv = -ENOENT;
			goto out;
		}

		cur = next;
		pathrem = end;
	}
out:
	kfree(name, FILENAME_MAX);
	return rv;
}

struct file *fs_alloc_file(void)
{
	return slab_alloc(file_slab);
}

void fs_free_file(struct file *f)
{
	slab_free(file_slab, f);
}

static int cmd_ls(int argc, char **argv)
{
	struct fs_node *node;
	struct fs_node *child;
	int rv;
	if (argc != 1) {
		puts("usage: ls FILE_OR_DIR\n");
		return 1;
	}
	rv = fs_resolve(argv[0], &node);
	if (rv != 0) {
		printf("error %d in fs_resolve\n", rv);
		return rv;
	}

	if (node->type == FSN_LAZY_DIR) {
		/*printf("node is lazy dir, executing 0x%x\n",
		       node->fs->fs_ops->fs_list);*/
		node->fs->fs_ops->fs_list(node);
	}
	if (node->type != FSN_DIR) {
		puts("error: not a directory\n");
		return 1;
	}
	list_for_each_entry(child, &node->children, list)
	{
		printf("%c %s (len=%d)\n", child->type == FSN_FILE ? 'f' : 'd',
		       child->name, (uint32_t)child->size);
	}

	return 0;
}

static int cmd_cat(int argc, char **argv)
{
	int rv = 0;
	struct fs_node *node;
	struct file *f;
	char *buf;
	int blksize = 1024;

	if (argc < 1 || argc > 2) {
		puts("usage: cat FILENAME [blksiz]\n");
		return 1;
	}
	if (argc == 2) {
		blksize = atoi(argv[1]);
	}
	rv = fs_resolve(argv[0], &node);
	if (rv < 0) {
		return rv;
	}
	f = node->fs->fs_ops->fs_open(node, O_RDONLY);
	buf = kmalloc(blksize);
	do {
		rv = f->ops->read(f, buf, blksize);
		if (rv > 0)
			nputs(buf, rv);
	} while (rv == blksize);
	f->ops->close(f);
	kfree(buf, blksize);
	return rv;
}

static int cmd_addline(int argc, char **argv)
{
	int rv = 0;
	struct fs_node *node;
	struct file *f;
	char *buf;
	int bytes;

	if (argc != 2) {
		puts("usage: addline FILENAME contents\n");
		return 1;
	}
	rv = fs_resolve(argv[0], &node);
	if (rv < 0)
		return rv;
	f = node->fs->fs_ops->fs_open(node, O_WRONLY | O_APPEND);
	buf = kmalloc(1024);
	bytes = snprintf(buf, 1024, "%s\n", argv[1]);
	f->ops->write(f, buf, bytes);
	kfree(buf, 1024);
	f->ops->close(f);
	return rv;
}

struct ksh_cmd fs_ksh_cmds[] = {
	KSH_CMD("ls", cmd_ls, "list directory"),
	KSH_CMD("cat", cmd_cat, "print file contents to console"),
	KSH_CMD("addline", cmd_addline, "add line to a file"),
	{ 0 },
};

void fs_init(void)
{
	fs_node_slab =
	        slab_new("fs_node", sizeof(struct fs_node), kmem_get_page);
	file_slab = slab_new("file", sizeof(struct file), kmem_get_page);
	fs_root = slab_alloc(fs_node_slab);
	strlcpy(fs_root->name, "/", sizeof(fs_root->name));
	fs_root->type = FSN_LAZY_DIR;
	fs_root->namelen = 1;
	fs_root->nchildren = 0;
	INIT_LIST_HEAD(fs_root->children);
	fs_root->location = 0xFFFFFFFFFFFFFFFFL;
	/* a special case, root will never be in a child list */
	fs_root->list.next = NULL;
	fs_root->list.prev = NULL;
	fs_root->parent = NULL;
	fs_root->fs = NULL;
}
