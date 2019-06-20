/*
 * Declarations worth keeping :)
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "list.h"
#include "format.h"
#include "alloc.h"

#define nelem(x) (sizeof(x) / sizeof(x[0]))

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
/* NB: this is actually a uint32_t array because it points to a table of words */
extern uint32_t first_level_table[];

/*
 * Address of the UART is stored as a variable.
 */
extern uint32_t uart_base;

/*
 * Some well-known addresses which we determine at runtime.
 */
extern void *second_level_table;

/*
 * Basic I/O (see uart.s and format.c for details)
 */
void puts(char *string);
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

/*
 * Initialize kernel memory system (see mem.c for details)
 */
void kmem_init(uint32_t phys, bool verbose);

/*
 * Look up the physical address corresponding to a kernel virtual address
 */
uint32_t kmem_lookup_phys(void *virt_ptr);

/*
 * Return pages of kernel memory, already mapped and everything!
 */
void *kmem_get_pages(uint32_t bytes, uint32_t align);

/*
 * Free that memory.
 */
void kmem_free_pages(void *virt_ptr, uint32_t len);

/*
 * Map physical memory into the kernel memory space.
 */
void kmem_map_pages(uint32_t virt, uint32_t phys, uint32_t len, uint32_t attrs);

void kmem_unmap_pages(uint32_t virt, uint32_t len);

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
#define FLD_COARSE   0x01
#define FLD_SECTION  0x02

#define FLD_MASK     0x03

/* second level descriptor types */
#define SLD_UNMAPPED 0x00
#define SLD_LARGE    0x01
#define SLD_SMALL    0x02

#define SLD_MASK     0x03

/* access control for second level */
#define NOT_GLOBAL   (0x1 << 11)
#define PRW_UNA      (SLD__AP0)            /* AP=0b001 */
#define PRW_URO      (SLD__AP1)            /* AP=0b010 */
#define PRW_URW      (SLD__AP1 | SLD__AP0) /* AP=0b011 */
#define PRO_UNA      (SLD__AP2 | SLD__AP0) /* AP=0b101 */
#define PRO_URO      (SLD__AP2 | SLD__AP1) /* AP=0b110 */
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
void data_abort(void);

/**
 * Load coprocessor register into dst.
 */
#define get_cpreg(dst, CRn, op1, CRm, op2) \
 __asm__ __volatile__ ( \
  "mrc p15, " #op1 ", %[rd], " #CRn ", " #CRm ", " #op2 \
  : [rd] "=r" (dst) \
  : \
  : \
  )

/**
 * Set coprocessor register from src.
 */
#define set_cpreg(src, CRn, op1, CRm, op2) \
 __asm__ __volatile__ ( \
  "mcr p15, " #op1 ", %[rs], " #CRn ", " #CRm ", " #op2 \
  : [rs] "+r" (src) \
  : \
  : \
  )

/**
 * Load stack pointer
 */
#define get_sp(dst) \
 __asm__ __volatile__ ( \
  "mov %[rd], sp" \
  : [rd] "=r" (dst) \
  : \
  : \
  )

/*
 * Processes!
 */

/**
 * This "process" is hardly a process. It runs in user mode, but shares its
 * memory space with the kernel. It has a separate stack and separate registers,
 * and it can call a "relinquish()" method which swi's back into svc mode. This
 * simulates a process without actually doing anything.
 */
struct process {
	/**
	 * Context kept for context switching:
	 *
	 * - Preferred return address, Saved program status register
	 * - SP_usr, LR_usr
	 * - v1-v8
	 *
	 * A total of 12 words.
	 */
	uint32_t context[12];

	/** Global process list entry. */
	struct list_head list;

	uint32_t id;

	uint32_t size;
	uint32_t phys;
};

/**
 * Indices of important data within the process context:
 * - RET: preferred return address (which was LR_svc upon exception entry)
 * - SPSR: saved program status register, returned to CPSR on return
 * - SP: process stack pointer
 */
#define PROC_CTX_RET  10
#define PROC_CTX_SPSR 11
#define PROC_CTX_SP    0

/* Create a process */
struct process *create_process(struct process *p, uint32_t binary);
#define BIN_SALUTATIONS 0
#define BIN_HELLO       1

/* Start a process running, never return. */
void start_process(struct process *p);
void start_process_asm(void *startup);

/* Remove a process from the list and free it. Must reschedule after. */
void destroy_process(struct process *p);

/* Schedule (i.e. choose and contextswitch a new process) */
void schedule(void);

/* For processes, this is a system call to return back to the kernel. */
void relinquish(void);

/* The current process */
extern struct process *current;
extern struct list_head process_list;

/*
 * Pre-built binary processes
 */
extern uint32_t process_salutations_start[];
extern uint32_t process_salutations_end[];
extern uint32_t process_hello_start[];
extern uint32_t process_hello_end[];
