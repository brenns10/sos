SOS (Stephen's OS)
==================

This is my personal operating system project. It targets the 32-bit ARMv7-A
architecture, with the only currently supported machine being QEMU (a VM). It is
easy to compile, run, modify, and debug. Despite being very limited, this
one-person projcet has actually made a great deal of progress and continues to
improve.

Instructions
------------

To build and run this, you need two important pieces of software:

- [QEMU][qemu]: the virtual machine software used to emulate an ARM system
- ARM embedded toolchain: a compiler which targets ARM systems with no other
  operating system, e.g. "arm-none-eabi-gcc". For Ubuntu there's a [PPA][ppa].

[qemu]: https://www.qemu.org/
[ppa]: https://launchpad.net/~team-gcc-arm-embedded/+archive/ubuntu/ppa

With these two programs you can do:

    # Build & run the OS in a VM:
    make run
    # or...

    # Run in debug mode, which allows stepping through code in GDB:
    make debug
    # (in another terminal)
    make gdb

Demo
----

After starting up the VM, you should see something like this:

```
SOS: Startup
Stephen's OS (user shell, pid=2)
ush>
```

This is a shell running as a user-space process. It has very few features,
except to launch rudimentary processes. The best demonstration of what the OS
can do is to look at the output of the "demo" command. The "demo" command starts
up 10 instances of the same program, which looks like this:

```c
int main()
{
        uint32_t i, j;
        int pid = getpid();

        for (i = 0; i < 8; i++) {
                printf("[pid=%u] Hello world, via system call, #%u\n", pid, i);

                /* do some busy waiting so we see more context switching */
                for (j = 0; j < 300000; j++) {
                        asm("nop");
                }
        }

        return 0;
}
```

In other words, each process starts up, prints a message (including it's
identifier), and busy-waits a while. This is the (partial) output of the demo:

```
[pid=11] Hello world, via system call, #0
[pid=11] Hello world, via system call, #1
[pid=10] Hello world, via system call, #0
[pid=10] Hello world, via system call, #1
[pid=10] Hello world, via system call, #2
[pid=9] Hello world, via system call, #0
[pid=9] Hello world, via system call, #1
[pid=9] Hello world, via system call, #2
[pid=8] Hello world, via system call, #0
[pid=8] Hello world, via system call, #1
[pid=8] Hello world, via system call, #2
[pid=8] Hello world, via system call, #3
```

As each process executes, occasionally the operating system's timer interrupt
will trigger a context switch to a different process. We see this by the fact
that messages from multiple processes are interleaved. This demonstrates, at a
high level, that this OS can do pre-emptive multi-tasking.

Some Features
-------------

Here is a longer list of things my OS can do:

* Text I/O over PL011 UART. (see `kernel/uart.c`)
  - Input is interrupt driven, to avoid polling & busy waiting
* A small, limited `printf` implementation. (see `lib/format.c`)
* ARM MMU configured to provide separate address space for each process. (see
  both `kernel/startup.s` and `kernel/kmem.c`)
* A simple first-fit memory allocator for allocating pages of memory (see
  `lib/alloc.c`)
* A few system calls: display(), getchar(), getpid(), exit(). (see
  `kernel/syscall.c`)
* Driver for ARM generic timer, with a tick configured at 100Hz
  (`kernel/timer.c`)
* Driver for ARM generic interrupt controller, and interrupt handling supported.
  (see `kernel/gic.c`)
* Processes (if that wasn't already clear), with the following attributes:
  - Run in ARM user mode
  - Memory layout:
    - 0x0-0x40000000 - kernel space - no access
    - 0x40000000+ - userspace
    - Each process has a separate user address space
  - Interact with the world via system calls
  - Pre-emptive multi tasking (thanks to timer interrupt)
  - Scheduled via a round-robin scheduler

Some Limitations
----------------

Here are some odd limitations:

* Only one process can read from the UART at a time. If two processes try to
  read and are put in the waiting state, the first one will be put to sleep and
  never re-scheduled.
* Most memory aborts are handled by some form of kernel panic, so a userspace
  segfault will bring down the whole system.
* There's no filesystem, so every possible program is compiled into the kernel,
  and copied into userspace.
* Processes cannot expand their address space.

Here are a bunch of things to add support for, or improve:

* Filesystem
* Dynamic memory for processes
* ELF parsing
* Better DTB parsing, use it to determine peripherals
* Networking?

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

I found this e-book immensely helpful trying to get interrupts working:

http://umanovskis.se/files/arm-baremetal-ebook.pdf
