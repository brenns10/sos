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
 * Physical addresses worth remembering. Only valid once mem_init() is done.
 */
extern uint32_t phys_code_start;
extern uint32_t phys_code_end;
extern uint32_t phys_data_start;
extern uint32_t phys_data_end;
extern uint32_t phys_stack_start;
extern uint32_t phys_stack_end;
extern uint32_t phys_first_level_table;
extern uint32_t phys_second_level_table;
extern uint32_t phys_dynamic;

/*
 * Some well-known addresses which we determine at runtime.
 */
extern void *second_level_table;
extern void *phys_allocator;
extern void *kern_virt_allocator;

/*
 * Basic I/O (see uart.s and format.c for details)
 */
void puts(char *string);
uint32_t snprintf(char *buf, uint32_t size, const char *format, ...);
uint32_t printf(const char *format, ...);

/*
 * Initialize memory system (see mem.c for details)
 */
void mem_init(uint32_t phys, bool verbose);

/*
 * Look up the physical address corresponding to a virtual address
 */
uint32_t lookup_phys(void *virt_ptr);

/*
 * Wraps the virtual and physical allocators
 */
void *get_mapped_pages(uint32_t bytes, uint32_t align);

/*
 * Free what was returned by get_mapped_pages()
 */
void free_mapped_pages(void *virt_ptr, uint32_t len);

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

/* macro quoting utilities */
#define kern_quote(blah) #blah
#define kern_expand_and_quote(blah) kern_quote(blah)

/*
 * System calls!
 */
#define SYS_RELINQUISH 0
#define SYS_DISPLAY    1
#define SYS_EXIT       2
#define MAX_SYS 2

/**
 * Generic system call, no args.
 */
#define sys0(sys_n) \
	__asm__ __volatile__ ( \
		"svc #" kern_expand_and_quote(sys_n) \
		: /* output operands */ \
		: /* input operands */ \
		: /* clobbers */ "a1", "a2", "a3", "a4" \
		)

/**
 * Generic system call, one arg.
 */
#define sys1(sys_n, arg1) \
	__asm__ __volatile__ ( \
		"mov a1, %[a1]\n" \
		"svc #" kern_expand_and_quote(sys_n) \
		: /* output operands */  \
		: /* input operands */ [a1] "r" (arg1) \
		: /* clobbers */ "a1", "a2", "a3", "a4" \
		)

/*
 * Processes!
 */

/* The "userspace code" we run in a process */
typedef void (*process_start_t)(void);

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

/* Create a process with a stack. */
struct process *create_process(process_start_t startup);

/* Start a process running, never return. */
void start_process(struct process *p);
void start_process_asm(process_start_t startup, void *sp);

/* Remove a process from the list and free it. Must reschedule after. */
void destroy_process(struct process *p);

/* Schedule (i.e. choose and contextswitch a new process) */
void schedule(void);

/* For processes, this is a system call to return back to the kernel. */
void relinquish(void);

/* The current process */
extern struct process *current;
extern struct list_head process_list;
