/*
 * Context tracking
 */
#include <stddef.h>
#include <stdint.h>

#include "kernel.h"
#include "string.h"
#include "cxtk.h"

#ifdef CXTK

struct ctxrec {
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
#define CTX_SCHED 6

struct ctxrec *ctxarr = NULL;
size_t ctxidx = 0;

#define CTX_PAGES 1
#define CTX_CAP   ((CTX_PAGES * 4096) / sizeof(struct ctx))

static inline void cxtk_track(uint8_t type, uint32_t arg, uint8_t smallarg)
{
	int flags;
	irqsave(&flags);
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
	irqrestore(&flags);
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

void cxtk_track_schedule(void)
{
	cxtk_track(CTX_SCHED, 0, 0);
}

static inline void cxtk_report_single(struct ctxrec rec, bool *began)
{
	char *irqname;
	if (rec.type == CTX_NONE) {
		if (*began)
			puts("error: blank after tracking began\n");
		return;
	}
	*began = true;
	switch (rec.type) {
	case CTX_KINIT:
		puts("kernel initialized\n");
		break;
	case CTX_PROC:
		printf("schedule process %u (x%u)\n", rec.arg, rec.count);
		break;
	case CTX_SYSC:
		printf("  syscall (x%u)\n", rec.count);
		break;
	case CTX_SYSCR:
		printf("  syscall return (x%u)\n", rec.count);
		break;
	case CTX_IRQ:
		irqname = gic_get_name(rec.smallarg);
		irqname = irqname ? irqname : "unknown";
		printf("   IRQ %u \"%s\" interrupted 0x%x (x%u)\n",
		       rec.smallarg, irqname, rec.arg, rec.count);
		break;
	case CTX_SCHED:
		printf("   schedule() (x%u)\n", rec.count);
		break;
	default:
		puts("error: unknown entry type\n");
	}
}

void cxtk_report(void)
{
	bool began = false;
	size_t i, start;
	if (!ctxarr) {
		puts("context tracking not initialized\n");
		return;
	}

	puts("Context history:\n");
	start = (ctxidx + 1) % CTX_CAP;
	for (i = 0; i < CTX_CAP; i++) {
		cxtk_report_single(ctxarr[(start + i) % CTX_CAP], &began);
	}
	puts("End of context history\n");
	return;
}

#else

void cxtk_report(void)
{
	puts("context tracking not enabled in this build\n");
}

#endif
