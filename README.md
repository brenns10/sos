arm
===

Currently toying with writing code to run on a "bare metal" ARM processor.
Hoping to work up to a small operating system. Things that work now:

* Using the UART to print to the console
* Basic string formatting to the UART
* MMU enabled in assembly, and can be re-configured in C (both mapping and
  unmapping memory)
* Memory allocators for physical and virtual address spaces, which are able to
  allocate AND free memory (sounds stupid to mention that but freeing memory is
  actually very hard).
* Interrupt handling (simply reporting faults to UART, etc)
* "Processes" with:
  - separate stacks
  - shared address space
  - user mode
* Context switching between processes, and scheduling using a very basic round
  robin scheduler
* System calls:
  - relinquish() for cooperative multi-tasking
  - display() for printing to the UART

Next steps:

* Compile processes as separate binary files built-in to the kernel image, and
  load them into a "user address space" (while still sharing address space)
* Separate address spaces per-process - this will be very involved

If you want to work with the code, you can install qemu, the arm eabi toolchain,
and then use the commands below:

    # run the code
    make run

    # do debug mode
    make debug
    # (in another terminal)
    make gdb

If you want to run the tests (yes there are tests for some things), run the
following: (no need for qemu or special toolchains)

    make test


Resources
---------

The following links do similar things, but with different machines (i.e. not the
"virt" board from qemu), which means different memory layouts, etc:

https://balau82.wordpress.com/2010/02/28/hello-world-for-bare-metal-arm-using-qemu/

https://community.arm.com/processors/b/blog/posts/hello-world-with-bare-metal-and-qemu

This one does similar things, but with ARM 64 bit.

https://github.com/freedomtan/aarch64-bare-metal-qemu

ARM assembly reference card:

http://ozark.hendrix.edu/~burch/cs/230/arm-ref.pdf

Baking Pi

https://www.cl.cam.ac.uk/projects/raspberrypi/tutorials/os/ok05.html

This is an interesting alternative which uses qemu without a "system", just to
run some ARM code. Interesting stuff:

https://doppioandante.github.io/2015/07/10/Simple-ARM-programming-on-linux.html

This course website is a useful reference as well:

http://cs107e.github.io/
