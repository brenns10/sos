Memory
======

Memory management is confusing. This document aims to roughly describe the
approach I'm taking for my OS.

Important Facts
---------------

QEMU seems to be quite insistent on loading all code at the physical address
0x40010000, however the memory management code does not depend on this. We can
simply use PC-relative addressing to determine what our physical address is on
startup.

Our memory management system is built on the "Short descriptor translation table
format" described in the ARMv7 Reference, Section B3.5. We use "small pages" of
size 4KB. While this is almost certainly not an optimal choice, it gives us lots
of flexibility in allocations, permissions, etc.

Our virtual memory layout will one day be:

    0x00000000  # interrupt vector, startup code
      # code
      # static
      # kernel-mode stack
      # first & second level translation tables (for kernel)
      # dynamic memory for kernel
      ...
    0x02000000  # begin user address space

We will use different translation table base addresses for user-mode memory.

Initialization
--------------

`startup.s` contains the first stage of MMU configuration. This stub is very
self-contained, and does the following:

1. Using linker symbols, determine where the virtual address of the kernel is
   meant to be.
2. Using the PC, determine the physical location of the code.
3. Map the code at an identity mapping, as well as at the expected virtual
   address, and enable the MMU. (It also identity maps the UART for debugging).
4. Branch to the expected virtual address, setup a C stack, and invoke main()
   with a single argument: the physical address of the start of code.

Critically, it leaves the following up to C code:

* Cleaning up the initial identity mapping of code and UART
* Mapping the UART to a more convenient location
* Determining permissions for blocks of memory
* Allocating stacks for other processor modes

C Initialization
----------------

TBD
