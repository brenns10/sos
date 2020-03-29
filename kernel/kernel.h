/*
 * Declarations worth keeping :)
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "alloc.h"
#include "format.h"
#include "list.h"

/* Useful macros for the kernel to use */
#define nelem(x) (sizeof(x) / sizeof(x[0]))

#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))

#define WRITE32(_reg, _val) do { \
		register uint32_t __myval__ = (_val); \
		*(volatile uint32_t *)&(_reg) = __myval__; \
	} while (0)
#define READ32(_reg) (*(volatile uint32_t*)&(_reg))

struct process;

#define SOS_VERSION "0.1"

/*
 * Linker symbols for virtual memory addresses.
 */
extern uint8_t code_start[];
extern uint8_t code_end[];
extern uint8_t data_start[];
extern uint8_t data_end[];
extern uint8_t stack_start[];
extern uint8_t stack_end[];
extern uint8_t unused_start[];
extern uint8_t dynamic_start[];
/* NB: this is actually a uint32_t array because it points to a table of words
 */
extern uint32_t first_level_table[];

/*
 * Address of the UART is stored as a variable.
 */
extern uint32_t uart_base;

/*
 * Some well-known addresses which we determine at runtime.
 */
extern void *second_level_table;

extern void *fiq_stack;
extern void *irq_stack;
extern void *abrt_stack;
extern void *undf_stack;
extern void *svc_stack;

/*
 * Basic I/O (see uart.s and format.c for details)
 */
void puts(char *string);
void putc(char c);
char getc(void);
int getc_blocking(void);
void uart_init(void);
void uart_wait(struct process *p);
void uart_isr(uint32_t intid);
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

/*
 * Initialize kernel memory system (see mem.c for details)
 */
void kmem_init(uint32_t phys, bool verbose);

/*
 * Return pages of kernel memory, already mapped and everything!
 */
void *kmem_get_pages(uint32_t bytes, uint32_t align);
void *kmem_get_page(void);

/*
 * Free that memory.
 */
void kmem_free_pages(void *virt_ptr, uint32_t len);
void kmem_free_page(void *ptr);

/*
 * Look up the physical address corresponding to a virtual address
 */
uint32_t kmem_lookup_phys(void *virt_ptr);
uint32_t umem_lookup_phys(struct process *p, void *virt_ptr);

/*
 * Map physical memory into the virtual memory space.
 */
void kmem_map_pages(uint32_t virt, uint32_t phys, uint32_t len, uint32_t attrs);
void umem_map_pages(struct process *p, uint32_t virt, uint32_t phys,
                    uint32_t len, uint32_t attrs);

/*
 * Unmap memory
 */
void kmem_unmap_pages(uint32_t virt, uint32_t len);
void umem_unmap_pages(struct process *p, uint32_t virt, uint32_t len);

/*
 * Print it!
 */
void kmem_print(uint32_t start, uint32_t stop);
void umem_print(struct process *p, uint32_t start, uint32_t stop);

extern void *phys_allocator;
extern void *kern_virt_allocator;

/*
 * MMU Constants
 */
#define SLD__AP2 (1 << 9)
#define SLD__AP1 (1 << 5)
#define SLD__AP0 (1 << 4)

/* first level descriptor types */
#define FLD_UNMAPPED 0x00
#define FLD_COARSE 0x01
#define FLD_SECTION 0x02

#define FLD_MASK 0x03

/* second level descriptor types */
#define SLD_UNMAPPED 0x00
#define SLD_LARGE 0x01
#define SLD_SMALL 0x02

#define SLD_MASK 0x03

#define SLD_NG (1 << 11)

/* access control for second level */
#define NOT_GLOBAL (0x1 << 11)
#define PRW_UNA (SLD__AP0)            /* AP=0b001 */
#define PRW_URO (SLD__AP1)            /* AP=0b010 */
#define PRW_URW (SLD__AP1 | SLD__AP0) /* AP=0b011 */
#define PRO_UNA (SLD__AP2 | SLD__AP0) /* AP=0b101 */
#define PRO_URO (SLD__AP2 | SLD__AP1) /* AP=0b110 */
#define EXECUTE_NEVER 0x01

/*
 * System info debugging command (see sysinfo.c for details)
 */
void sysinfo(void);

/*
 * Set up the stack for all the other modes ahead of time, so we can use them.
 */
void setup_stacks(void *location);

/*
 * C Entry Points
 */
void data_abort(uint32_t lr);

/**
 * Load coprocessor register into dst.
 */
#define get_cpreg(dst, CRn, op1, CRm, op2)                                     \
	__asm__ __volatile__("mrc p15, " #op1 ", %[rd], " #CRn ", " #CRm       \
	                     ", " #op2                                         \
	                     : [rd] "=r"(dst)                                  \
	                     :                                                 \
	                     :)

/**
 * Set coprocessor register from src.
 */
#define set_cpreg(src, CRn, op1, CRm, op2)                                     \
	__asm__ __volatile__("mcr p15, " #op1 ", %[rs], " #CRn ", " #CRm       \
	                     ", " #op2                                         \
	                     : [rs] "+r"(src)                                  \
	                     :                                                 \
	                     :)

/**
 * Get 64-bit cpreg
 */
#define get_cpreg64(dstlo, dsthi, CRm, op3)                                    \
	__asm__ __volatile__("mrrc p15, " #op3 ", %[Rt], %[Rt2], " #CRm        \
	                     : [Rt] "=r"(dstlo), [Rt2] "=r"(dsthi)             \
	                     :                                                 \
	                     :)

#define set_cpreg64(srclo, srchi, CRm, op3)                                    \
	__asm__ __volatile__("mcrr p15, " #op3 ", %[Rt], %[Rt2], " #CRm        \
	                     : [Rt] "+r"(srclo), [Rt2] "+r"(srchi)             \
	                     :                                                 \
	                     :)

/**
 * Load SPSR
 */
#define get_spsr(dst)                                                          \
	__asm__ __volatile__("mrs %[rd], spsr"                                 \
	                     : [rd] "=r" (dst)                                 \
	                     : : )

/**
 * Load CPSR
 */
#define get_cpsr(dst)                                                          \
	__asm__ __volatile__("mrs %[rd], cpsr"                                 \
	                     : [rd] "=r" (dst)                                 \
	                     : : )

#define ARM_MODE_USER 0x10U
#define ARM_MODE_FIQ  0x11U
#define ARM_MODE_IRQ  0x12U
#define ARM_MODE_SVC  0x13U
#define ARM_MODE_ABRT 0x17U
#define ARM_MODE_UNDF 0x1BU
#define ARM_MODE_SYS  0x1FU
#define ARM_MODE_MASK 0x1FU

/**
 * Load stack pointer
 */
#define get_sp(dst) __asm__ __volatile__("mov %[rd], sp" : [rd] "=r"(dst) : :)

/**
 * This "process" is hardly a process. It runs in user mode, but shares its
 * memory space with the kernel. It has a separate stack and separate registers,
 * and it can call a "relinquish()" method which swi's back into svc mode. This
 * simulates a process without actually doing anything.
 */
struct process {
	/* For kernel thread, the stack */
	void *kstack;

	/**
	 * Context kept for context switching:
	 *
	 * - Preferred return address, Saved program status register
	 * - v1-v8
	 * - a2-a4,r12
	 * - a1
	 * - SP_usr, LR_usr
	 *
	 * A total of 17 words.
	 */
	uint32_t context[17];

	struct {
		int pr_ready : 1;  /* ready to be scheduled? */
		int pr_syscall : 1; /* is the process suspended by syscall? */
		int pr_kernel : 1; /* is a kernel thread? */
	} flags;

	/** Global process list entry. */
	struct list_head list;

	/** Basically a pid */
	uint32_t id;

	/** Size of the process image file. */
	uint32_t size;

	/** Physical address of process image. */
	uint32_t phys;

	/** Allocator for the process address space. */
	void *vmem_allocator;

	/** First-level page table and shadow page table. */
	uint32_t ttbr1;
	uint32_t *first;
	uint32_t **shadow;
};

/**
 * Indices of important data within the process context:
 * - RET: preferred return address (which was LR_svc upon exception entry)
 * - SPSR: saved program status register, returned to CPSR on return
 * - SP: process stack pointer
 */
#define PROC_CTX_RET 15
#define PROC_CTX_SPSR 16
#define PROC_CTX_SP 0
#define PROC_CTX_A1 2

/* Create a process */
struct process *create_process(uint32_t binary);
#define BIN_SALUTATIONS 0
#define BIN_HELLO 1
#define BIN_USH 2
int32_t process_image_lookup(char *name);

/* Start a process running, never return. */
void start_process(struct process *p);
void start_process_asm(void *startup);

/* Destroy the current process and reschedule. Does not return. */
void destroy_current_process(void);

/* Schedule (i.e. choose and contextswitch a new process) */
void schedule(void);

/* For processes, this is a system call to return back to the kernel. */
void relinquish(void);

/* The current process */
extern struct process *current;
extern struct list_head process_list;

/* Initialize process subsystem */
void process_init(void);

/*
 * Pre-built binary processes
 */
extern uint32_t process_salutations_start[];
extern uint32_t process_salutations_end[];
extern uint32_t process_hello_start[];
extern uint32_t process_hello_end[];
extern uint32_t process_ush_start[];
extern uint32_t process_ush_end[];

/* "uncomment" this if you want to debug page allocations */
#ifdef DEBUG_PAGE_ALLOCATOR_CALLS
#define alloc_pages(allocator, bytes, align)                                   \
	({                                                                     \
		uint32_t rv = alloc_pages(allocator, bytes, align);            \
		printf("%s:%u: (in %s) alloc_pages(%s, 0x%x, %u) = 0x%x\n",    \
		       __FILE__, __LINE__, __func__, #allocator, bytes, align, \
		       rv);                                                    \
		rv;                                                            \
	})
#endif

void dtb_init(uint32_t phys);

void ksh(void*);

/* ksh commands */
int cmd_mkproc(int argc, char **argv);
int cmd_lsproc(int argc, char **argv);
int cmd_execproc(int argc, char **argv);
int cmd_dtb_ls(int argc, char **argv);
int cmd_dtb_prop(int argc, char **argv);
int cmd_dtb_dump(int argc, char **argv);
int cmd_timer_get_freq(int argc, char **argv);
int cmd_timer_get_count(int argc, char **argv);
int cmd_timer_get_ctl(int argc, char **argv);
int cmd_timer_set_tval(int argc, char **argv);
int virtio_blk_cmd_status(int argc, char **argv);
int virtio_blk_cmd_read(int argc, char **argv);
int virtio_blk_cmd_write(int argc, char **argv);
int virtio_net_cmd_status(int argc, char **argv);
int virtio_net_cmd_dhcpdiscover(int argc, char **argv);

/* GIC Driver */
typedef void(*isr_t)(uint32_t);
void gic_init(void);
void gic_enable_interrupt(uint8_t int_id);
uint32_t gic_interrupt_acknowledge(void);
void gic_end_interrupt(uint32_t int_id);
void gic_register_isr(uint32_t intid_start, uint32_t intid_count, isr_t isr);
isr_t gic_get_isr(uint32_t intid);

/* timer */
void timer_init(void);
void timer_isr(uint32_t intid);

/* special exectuion functions, see entry.s */
void return_from_exception(uint32_t retval, uint32_t use_ret, void *return_to);
void block(uint32_t *ctx);

/* Virtio */
void virtio_init(void);

#define EBUSY 1

#define mb() asm("dsb")
