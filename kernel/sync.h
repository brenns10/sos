/*
 * Synchronization primitives
 */
#pragma once
#include <stdint.h>

typedef uint32_t spinsem_t;

/*
 * All static spin semaphores should be declared using DECLARE_SPINSEM:
 *
 *     DECLARE_SPINSEM(mysem, 1);
 *
 * Any spin semaphore which cannot be declared statically should be initialized
 * like so:
 *
 *     struct foo {
 *         // ...
 *         spinsem_t mysem;
 *     };
 *     struct foo *myfoo = alloc_foo();
 *     INIT_SPINSEM(&myfee->mysem, 1);
 *
 * Presently this does nothing special, but I'm sure it will be useful someday.
 */
#define DECLARE_SPINSEM(name, init) spinsem_t name = init
#define INIT_SPINSEM(addr, val)     *(addr) = (val)

/*
 * _spin_acquire(inputaddr): attempt to "acquire" the semaphore pointed by
 * inputaddr. This means attempting to decrement the semaphore if it is greater
 * than 0, otherwise blocking (by spinning) until it is greater than 0.
 *
 * This is a raw procedure - to properly spin lock, interrupts must be disabled.
 * Otherwise, an interrupt could deadlock waiting on this lock. In fact, since
 * SOS is single-CPU, a lock could be implemented just by disabling interrupts,
 * but oh well.
 *
 * Implementation notes:
 *
 *   See section A3.4 of the ARM Architecture Reference Manual.
 *
 *   1) Read the semaphore value exclusively. If the read gives us a 0, clear
 *      the exclusive read, and try again. Just keep trying until we get
 *      something other than 0.
 *   2) Subtract 1 from the read value. Store it back exclusively. If the
 *      exclusive store fails, this means that some other process has
 *      concurrently written since we performed our exclusive read. If that
 *      happens, we try again from step 1.
 *   3) Finally, execute a memory barrier to ensure that memory accesses after
 *      this take place after we've gained mutual exclusion.
 */
#define _spin_acquire(inputaddr)                                               \
	__asm__ __volatile__("1:  clrex\n\t"                                   \
	                     "    ldrex r0, [%[addr]]\n\t"                     \
	                     "    cmp   r0, #0\n\t"                            \
	                     "    beq   1b\n\t"                                \
	                     "    sub   r0, r0, #1\n\t"                        \
	                     "    strex r1, r0, [%[addr]]\n\t"                 \
	                     "    cmp   r1, #0\n\t"                            \
	                     "    bne   1b\n\t"                                \
	                     "    dmb\n\t"                                     \
	                     : /* no output */                                 \
	                     : [ addr ] "r"(inputaddr)                         \
	                     : "r0", "r1", "cc", "memory")

static inline void spin_acquire_irqsave(spinsem_t *sem, int *flags)
{
	irqsave(flags);
	_spin_acquire(sem);
}

#define ldrex(dest, inputaddr)                                                 \
	__asm__ __volatile__("    ldrex %[rv], [%[addr]]\n\t"                  \
	                     : [ rv ] "=r"(dest)                               \
	                     : [ addr ] "r"(inputaddr)                         \
	                     : "memory")
#define strex(dst, val, inputaddr)                                             \
	__asm__ __volatile__("    strex %[dest], %[value], [%[addr]]\n\t"      \
	                     : [ dest ] "=r"(dst)                              \
	                     : [ addr ] "r"(inputaddr), [ value ] "r"(val)     \
	                     : "memory")

#define clrex() __asm__ __volatile__("    clrex\n\t" :::)

/*
 * _spin_release(inputaddr): release the semaphore pointed by inputaddr. This
 * means incrementing the semaphore. DO NOT do this if you didn't have a
 * previous spin_acquire(), that would be rude.
 *
 * Implementation notes:
 *
 *   1) First a memory barrier, to ensure that all memory accesses happen before
 *      the semaphore is released.
 *   2) We still need a loop here, so that we can retry in case our exclusive
 *      store got interrupted by somebody else.
 */
#define _spin_release(inputaddr)                                               \
	__asm__ __volatile__("    dmb\n\t"                                     \
	                     "1:  ldrex r0, [%[addr]]\n\t"                     \
	                     "    add   r0, r0, #1\n\t"                        \
	                     "    strex r1, r0, [%[addr]]\n\t"                 \
	                     "    cmp   r1, #0\n\t"                            \
	                     "    bne   1b\n\t"                                \
	                     : /* no output */                                 \
	                     : [ addr ] "r"(inputaddr)                         \
	                     : "r0", "r1", "cc", "memory")

static inline void spin_release_irqrestore(spinsem_t *sem, int *flags)
{
	_spin_release(sem);
	irqrestore(flags);
}
