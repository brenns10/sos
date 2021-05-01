/*
 * ldisc.c - line discipline
 */
#include "ldisc.h"
#include "fs.h"
#include "kernel.h"
#include "list.h"
#include "string.h"
#include "sync.h"
#include "wait.h"

#define LLE_INIT_BUF 1024

#define get_flip_file(f) ((struct flip_file *)&((f)->priv[0]))

static inline void flip_add(struct flip_file *ff, struct flip_buffer *b)
{
	INIT_LIST_HEAD(b->list);
	_spin_acquire(&ff->lock);
	if (ff->current) {
		list_insert_end(&ff->buflist, &b->list);
	} else {
		ff->current = b;
		wait_list_awaken(&ff->wait);
	}
	_spin_release(&ff->lock);
}

static struct flip_buffer *flip_advance_list(struct flip_file *ff)
{
	struct flip_buffer *rv, *iter;
	rv = ff->current;
	ff->current = NULL;
	list_for_each_entry(iter, &ff->buflist, list)
	{
		list_remove(&iter->list);
		ff->current = iter;
		break;
	}
	return rv;
}

static struct flip_buffer *flip_maybe_get_buffer(struct flip_file *ff)
{
	struct flip_buffer *rv;
	int flags;
	spin_acquire_irqsave(&ff->lock, &flags);
	rv = ff->current;
	spin_release_irqrestore(&ff->lock, &flags);
	return rv;
}

static struct flip_buffer *flip_get_buffer(struct flip_file *ff)
{
	struct flip_buffer *rv;

	while (!(rv = flip_maybe_get_buffer(ff))) {
		wait_for(&ff->wait);
		wait_list_init(&ff->wait);
	}

	return rv;
}

struct flip_buffer *flip_buffer_new(void)
{
	struct flip_buffer *fb = kmalloc(sizeof(*fb));
	fb->buf = kmalloc(LLE_INIT_BUF);
	fb->size = LLE_INIT_BUF;
	fb->size_orig = LLE_INIT_BUF;
	fb->pos = 0;
	return fb;
}

void flip_buffer_free(struct flip_buffer *fb)
{
	kfree(fb->buf, LLE_INIT_BUF);
	kfree(fb, sizeof(*fb));
}

static int flip_read(struct file *f, void *dst, size_t amt)
{
	struct flip_file *ff = get_flip_file(f);
	struct flip_buffer *fb = flip_get_buffer(ff);
	size_t bytes = 0;
	size_t to_copy;

	while (fb && bytes < amt) {
		to_copy = min(fb->size - fb->pos, amt);
		memcpy(dst, fb->buf + fb->pos, to_copy);
		fb->pos += to_copy;
		bytes += to_copy;
		if (fb->pos == fb->size) {
			flip_buffer_free(fb);
			flip_advance_list(ff);
			fb = flip_maybe_get_buffer(ff);
		}
	}

	return bytes;
}

static int flip_write(struct file *f, void *src, size_t amt)
{
	nputs(src, amt);
	return amt;
}

static int flip_close(struct file *f)
{
	return 0;
}

struct file_ops flip_file_ops = {
	.read = flip_read,
	.write = flip_write,
	.close = flip_close,
};

struct file *flip_file_new(void)
{
	struct file *f = fs_alloc_file();
	struct flip_file *ff = get_flip_file(f);
	ff->current = NULL;
	INIT_LIST_HEAD(ff->buflist);
	wait_list_init(&ff->wait);
	INIT_SPINSEM(&ff->lock, 1);
	f->ops = &flip_file_ops;
	return f;
}

void lle_char(struct ldisc_line_edit *lle, char c)
{
	struct flip_file *ff = get_flip_file(lle->dest);
	if (c == '\x7f') {
		if (lle->fb->pos > 0) {
			lle->fb->pos--;
			puts("\b \b");
		}
	} else if (c == '\r' || c == '\n') {
		if (c == '\r')
			putc(c);
		c = '\n';
		putc(c);
		/* deliver buf to app */
		lle->fb->buf[lle->fb->pos++] = c;
		lle->fb->size = lle->fb->pos;
		lle->fb->pos = 0;
		flip_add(ff, lle->fb);
		lle->fb = flip_buffer_new();
	} else {
		/* add character to buffer, always leave space for a newline */
		if (lle->fb->pos < lle->fb->size - 1) {
			putc(c);
			lle->fb->buf[lle->fb->pos++] = c;
		}
	}
}
