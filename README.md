SOS (Stephen's OS)
==================

This is a toy operating system for ARM processors. It doesn't have many
features, and the code is pretty bad. It has never actually run on a real
processor (just a virtual machine). The list of limitations is too long to write
here. But it's all homemade, and so I love it.

Here are some of the things it does:

* Kernel may use the UART to print messages to the console
* Basic printf support for writing to the console
* MMU is fully configured and managed
* Memory allocation code (which also allows you to free addresses, usually
  that's at least half the battle)
* System call handling, basic exception reporting
* Context switching!
* Processes support
  - separate address spaces
  - user mode
  - system calls
  - cooperative multiprocessing
* Scheduling using a round-robin scheduler
* Configures a regular timer interrupt and handles it

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

These are a bunch of things I've looked at while making this, but it's not
exhaustive. The primary resource, of course, is the ARMv7-A architecture
reference manual.

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
