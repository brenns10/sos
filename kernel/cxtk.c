/*
 * Context tracking
 */
#include <stddef.h>
#include <stdint.h>

#include "kernel.h"
#include "string.h"

struct ctx {
	uint8_t type;
	uint8_t smallarg;
	uint16_t count;
	uint32_t arg;
};

#define CTX_NONE  0
#define CTX_KINIT 1
#define CTX_PROC  2
#define CTX_SYSC  3
#define CTX_SYSCR 4
#define CTX_IRQ   5
#define CTX_BLK   6

struct ctx *ctxarr = NULL;
size_t ctxidx = 0;

#define CTX_PAGES 1
#define CTX_CAP   ((CTX_PAGES * 4096) / sizeof(struct ctx))

static inline void cxtk_track(uint8_t type, uint32_t arg, uint8_t smallarg)
{
	if (ctxarr[ctxidx].type == type && ctxarr[ctxidx].arg == arg &&
	    ctxarr[ctxidx].smallarg == smallarg) {
		ctxarr[ctxidx].count += 1;
	} else {
		ctxidx = (ctxidx + 1) % CTX_CAP;
		ctxarr[ctxidx].type = type;
		ctxarr[ctxidx].arg = arg;
		ctxarr[ctxidx].smallarg = smallarg;
		ctxarr[ctxidx].count = 1;
	}
}

void cxtk_init(void)
{
	ctxidx = 0;
	ctxarr = (struct ctx *)kmem_get_pages(PAGE_SIZE * CTX_PAGES, 0);
	memset(ctxarr, 0, PAGE_SIZE * CTX_PAGES);
	cxtk_track(CTX_KINIT, 0, 0);
}

void cxtk_track_syscall(void)
{
	cxtk_track(CTX_SYSC, current->id, 0);
}

void cxtk_track_syscall_return(void)
{
	cxtk_track(CTX_SYSCR, 0, 0);
}

void cxtk_track_irq(uint8_t id, uint32_t instr)
{
	cxtk_track(CTX_IRQ, instr, id);
}

void cxtk_track_proc(void)
{
	cxtk_track(CTX_PROC, current->id, 0);
}

void cxtk_track_block(void)
{
	cxtk_track(CTX_BLK, 0, 0);
}

void cxtk_report(void)
{
	bool began = false;
	char *irqname;
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
			irqname = gic_get_name(ctxarr[i].smallarg);
			irqname = irqname ? irqname : "unknown";
			printf("   IRQ %u \"%s\" interrupted 0x%x (x%u)\n",
			       ctxarr[i].smallarg, irqname, ctxarr[i].arg,
			       ctxarr[i].count);
			break;
		case CTX_BLK:
			printf("   block (x%u)\n", ctxarr[i].count);
			break;
		default:
			puts("error: unknown entry type\n");
		}
	}
	puts("End of context history\n");
	return;
}
