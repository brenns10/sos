/*
 * ldisc.h - line discipline handling
 */
#pragma once

#include "fs.h"
#include "list.h"
#include "wait.h"

struct flip_buffer {
	char *buf;
	int size_orig;
	int size;
	int pos;
	struct list_head list;
};

struct flip_file {
	struct list_head buflist;
	struct flip_buffer *current;
	spinsem_t lock; /* protects buflist, current */

	struct waitlist wait;
};

struct ldisc_line_edit {
	struct flip_buffer *fb;
	struct file *dest;
	enum lle_state {
		LS_NORMAL,
		LS_ESCAPE,
	} state;
};

struct file *flip_file_new(void);
struct flip_buffer *flip_buffer_new(void);
void lle_char(struct ldisc_line_edit *lle, char c);
