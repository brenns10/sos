/*
 * Context tracking
 */
#include <stddef.h>
#include <stdint.h>

#include "kernel.h"
#include "string.h"

struct ctx {
	uint8_t type;
	uint8_t _pad1;
	uint16_t arg;
	uint32_t count;
};

#define CTX_NONE  0
#define CTX_KINIT 1
#define CTX_PROC  2
#define CTX_SYSC  3
#define CTX_SYSCR 4
#define CTX_IRQ   5

struct ctx *ctxarr = NULL;
size_t ctxidx = 0;

#define CTX_PAGES 1
#define CTX_CAP   ((CTX_PAGES * 4096) / sizeof(struct ctx))

static inline void cxtk_track(uint8_t type, uint16_t arg)
{
	if (ctxarr[ctxidx].type == type && ctxarr[ctxidx].arg == arg) {
		ctxarr[ctxidx].count += 1;
	} else {
		ctxidx = (ctxidx + 1) % CTX_CAP;
		ctxarr[ctxidx].type = type;
		ctxarr[ctxidx].arg = arg;
		ctxarr[ctxidx].count = 1;
	}
}

void cxtk_init(void)
{
	ctxidx = 0;
	ctxarr = (struct ctx *)kmem_get_pages(PAGE_SIZE * CTX_PAGES, 0);
	memset(ctxarr, 0, PAGE_SIZE * CTX_PAGES);
	cxtk_track(CTX_KINIT, 0);
}

void cxtk_track_syscall(void)
{
	cxtk_track(CTX_SYSC, (uint16_t)current->id);
}

void cxtk_track_syscall_return(void)
{
	cxtk_track(CTX_SYSCR, 0);
}

void cxtk_track_irq(uint8_t id)
{
	cxtk_track(CTX_IRQ, id);
}

void cxtk_track_proc(void)
{
	cxtk_track(CTX_PROC, (uint16_t)current->id);
}

void cxtk_report(void)
{
	bool began = false;
	size_t i;
	if (!ctxarr) {
		puts("context tracking not initialized\n");
		return;
	}

	puts("Context history:\n");
	for (i = (ctxidx + 1) % CTX_CAP; i != ctxidx; i = (i + 1) % CTX_CAP) {
		if (ctxarr[i].type == CTX_NONE) {
			if (began)
				puts("error: blank after tracking began\n");
			continue;
		}
		began = true;
		switch (ctxarr[i].type) {
		case CTX_KINIT:
			puts("kernel initialized\n");
			break;
		case CTX_PROC:
			printf("schedule process %u (x%u)\n", ctxarr[i].arg,
			       ctxarr[i].count);
			break;
		case CTX_SYSC:
			printf("  syscall (x%u)\n", ctxarr[i].count);
			break;
		case CTX_SYSCR:
			printf("  syscall return (x%u)\n", ctxarr[i].count);
			break;
		case CTX_IRQ:
			printf("   IRQ %u (x%u)\n", ctxarr[i].arg,
			       ctxarr[i].count);
			break;
		default:
			puts("error: unknown entry type\n");
		}
	}
	puts("End of context history\n");
	return;
}
