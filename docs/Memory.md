Memory
======

Memory management is confusing. This document aims to roughly describe the
approach I'm taking for my OS.

Important Facts
---------------

QEMU seems to be quite insistent on loading all code at the physical address
0x40010000. The early startup code is roughly position independent, so this
shouldn't matter for it. But the rest of the symbols expect to be present at a
certain virtual address once we're booted, and those addresses change depending
on our load address.

Here's the memory layout we currently use:

    0x0000 0000 - 0x3FFF FFFF  Unused userspace memory
    0x4000 0000 - 0x7FFF FFFF  Userspace link/load address
    ---- user / kernel split.  TTBR0 ^     TTBR1 v
    0x8000 0000 - 0xFF7F FFFF  Kernel direct-map memory
    0xFF80 0000 - 0xFFFF FFFF  Kernel "vmalloc" memory

We use a 2/2 user/kernel split because that's the best that ARM gives us with
the separate TTBR registers, and that seemed like an easy way to get started.

Initialization
--------------

`kernel/kmem.c` owns everything related to page table management. `kmem_init2()`
initializes the page tables before the MMU is enabled. It creates the kernel
direct map region, and then maps a temporary identity page in to allow the
kernel to relocate. After the MMU is enabled, it removes this identity page, and
initializes allocators:

- `kernalloc`: allocates from the kernel direct map region, starting after the
  kernel image ends. This is pretty much all physical memory.
- `kern_virt_allocator`: manages the "vmalloc" region at the top of the address
  space for dynamic mappings.

These memory allocators actually are implemented in `lib/alloc.c`, they are K&R
style allocators which simply track ranges of free/allocatod page addresses.

Alternatives
------------

Prior to this simpler "direct-map" memory model, I had a fun model I used (which
I've subsequently heard referred to as "PMM/VMM" for Physical Memory Manager /
Virtual MM). The idea is that you have one allocator managing physical memory
addresses, and another managing virtual addresses. When page allocations are
requested, you allocate from each allocator and map the two together. This was a
fun approach, but it had some difficulties:

- Unmapping memory is tricky to get right. It requires proper cache and
  TLB maintenance. The complexity was not visible on QEMU, but on real
  hardware I have encountered countless issues. While eliminating the
  virtual memory operations from general page management won't eliminate
  the maintenance requirements (or bugs), it will dramatically reduce
  their scope and make it easier to debug.
- The virtual address space was entirely unrelated to the physical
  address space, so converting between addresses was complex and
  required manually consulting the page tables. Converting physical
  addresses to virtual ones was impossible. kmem v2 allows simple vtop
  and ptov operations for normal kernel memory without consulting page
  tables.
- Kernel page tables required using 2-level page tables and mapping
  everything using small pages. Now, normal kernel pages are mapped
  using large (1MB) section descriptors in the first-level table. We can
  still use small pages for some kernel mappings and for userspace
  mappings.

The direct-map memory model is heavily inspired by Linux. Long term, I inspect
to copy-cat a few other memory-management items from there (copy in spirit, but
use my own implementation). Namely, page descriptors (which make slab allocators
nice) and the buddy allocator (facilitated by page descriptors).
