# Architecture-Specific API

In order to support multiple architectures (currently only 32/64 bit ARM, but
more could be implemented), we need a clean division between core kernel code
and the architecture-specific details required to manage page tables, enable and
disable interrupts, and more.

To that end, this document describes the API which architectures must implement:

## Boot

Currently, a single `kernel.ld` is provided, and architectures/configs simply
modify the base addresses for the linker. TODO: let the architecture completely
control the linker script.

The architecture-specific code should be the first to execute. It is responsible
for setting up page tables and enabling the MMU. MMU enablement has been a
difficult topic for me - my current "best" approach is to implement the page
table code in C, call it before enabling MMU, and then return back to the
assembly startup stub to do the actual enablement.

Another important responsibility of the architecture specific code is to
initialize the serial console such that the puts() function will work in the
`main()` function. This may not require much work at all, but after the MMU is
enabled, it's important to map the device at an appropriate memory location.

Finally, the boot process should hand off control to the `main()` function of
the core kernel.

## Memory Layout Requirements

In the boot process, arch code must initialize the page tables. As described
elsewhere, the memory layout should include a few components:

1. An "upper-kernel" split (i.e. kernel addresses should begin with `0xF...`).
2. A physical direct mapping of memory, ideally at the base kernel address. It
   is left up to the architecture, but ideally offset 0 should correspond with
   the first _physical_ memory address, even if its physical address is not 0.
3. A segment of memory within the kernel address space where we can map devices,
   or if for some reason I decide to support >2GiB memory on 32bit, then a place
   where we could map "highmem".
4. Optionally, a region where the code and static data sections are directly
   mapped, near the top of the address space. This would allow to compile the
   kernel once and load it from any load address.
   
Memory addresses from the physical direct mapping are considered "canonical
addresses". Core kernel code cannot assume that code or static data structures
are canonical addresses.

### Direct Mapping Offset

Architecture-specific code must initialize a variable describing the offset of
the physical direct mapping:

```c
uintptr_t arch_direct_map_offset;
```

The core kernel code will add this value to physical addresses to compute the
canonical kernel address, and subtract from virtual addresses to compute the
physical address. The routines `kvtop()` and `kptov()` implement this, and
should only be used after this is initialized.

### Physical Memory Allocator

Architecture-specific code must initialize the physical memory allocator:

```c
void *arch_phys_allocator;
```

This allocator should return addresses from the physical address range. The
architecture code may use this allocator in the course of initialization prior
to `main()`. However, it must ensure that the pointer is to a canonical kernel
address by the time `main()` is called. (For example, the arch code may
initialize it to a physical address and use it to help allocate page tables, but
once the MMU is enabled, the arch code should translate its physical address to
a canonical virtual address.)

The `kmem_get_pages()` functions are simple wrappers around this.

### Dynamic Mapping API

Architecture-specific code must provide the following APIs to create page
mappings:

```c
#include <arch/mmu.h>

void *arch_devmap(uintptr_t phys_addr, uintptr_t size);
```

This maps device memory for a physical address. The size must be in increments
of PAGE_SIZE. The virtual memory address is automatically selected from the
dynamic mapping range, which is limited.

Since this is a device-specific mapping API, the MMU configuration must not
allow caching - it should be configured as "device memory" in whatever way this
is supported by the architecture's MMU and caching facilities.

There are currently no APIs for maintaining "normal" memory mappings in the
kernel address space, although this is almost certainly something which could be
useful. When use cases arise, the API will be designed.

### Userspace Mapping APIs

Architecture-specific code must provide the ability to create, populate, and
"enter" a userspace address space:

```c
#include <arch/mmu.h>

// Create a user address space
void *arch_umem_alloc(void);

// Map pages into the user address space
void *arch_umem_map(void *as, uintptr_t virt, uintptr_t phys, uintptr_t size, enum umem_perm perm);

// Enter the user address space (discarding old one)
//  -> pass as==NULL to clear the current address space
//  -> id: the task ID, which hardware may use for TLB maintenance
void arch_umem_enter(void *as, unsigned int id);

// Query the address space mapping
uintptr_t arch_umem_lookup(void *as, void *user_ptr);

// Destroy the address space mapping
void arch_umem_free(void *as);
```

While memory mapping APIs should invoke the necessary TLB flushes, they may not
properly maintain the instruction or data caches. Additional work (and arch
APIs) will be necessary for cache maintenance.

## Interrupts

TODO

## System Calls

TODO

## Faults

TODO

## Context Switching

TODO

## Synchronization

TODO

## SMP CPU Management

TODO
