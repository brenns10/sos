SOS (Stephen's OS)
==================

[![builds.sr.ht status](https://builds.sr.ht/~brenns10/sos.svg)](https://builds.sr.ht/~brenns10/sos?)

This is my personal operating system project. It targets the 32-bit ARMv7-A
architecture. The main target board is qemu, but progress is being made on the
Raspberry Pi 4B target! Despite being very imited, this one-person project has
actually made a great deal of progress and continues to improve.

Instructions
------------

To build and run this, you need two important pieces of software: QEMU 4.2+, and
an ARM cross compiler. These dependencies are straightforward on Arch Linux, but
Ubuntu users should see more detailed setup instructions in
[Ubuntu.md](Ubuntu.md).

Some additional dependencies which get installed by the following command:

- pytest: a python testing framework, used to implement integration tests. You
  can skip this if you're not planning to run integration tests.
- mtools: tools which create and manage MS-DOS FAT12/16/32 volumes. These are
  automatically used by the integration test framework, but also are useful for
  creating your own filesystem image. However, a few dummy FS images are
  included in this repo, so it is not strictly necessary.

```
# Arch Linux
$ sudo pacman -Sy arm-none-eabi-gcc arm-none-eabi-binutils arm-none-eabi-gdb \
      qemu qemu-arch-extra python-pytest mtools
```

With these dependencies satisfied, you can do the following:

    # First, configure SOS to run in a VM:
    make config_qemu

    # Then, either build & run the OS in a VM:
    make run
    # or...

    # Run in debug mode, which allows stepping through code in GDB:
    make debug
    # (in another terminal)
    make gdb
    # or...

    # Run unit tests (which exercise just the C code) plus integration tests
    # (which start SOS in a VM and then test command functionality)
    make test


Raspberry Pi 4B
---------------

This OS is growing support for the Raspberry Pi 4B. So far, SOS can boot into a
simple kernel shell (which is not backed by kernel threads or a scheduler yet).
For more information on setup and running, see [docs/RaspberryPi.md][].

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
* Driver for a virtio block device (see `kernel/virtio-net.c`) and some very
  basic functionality which uses it.
* Driver for a virtio network device (see `kernel/virtio-net.c`) and some very
  basic functionality which uses it.
* FAT filesystem driver (currently only supports FAT12)

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

* Implement a filesystem
* Dynamic memory for processes
* ELF parsing
* Better DTB parsing, use it to determine peripherals
* Decent networking subsystem
* Implement UDP sockets
* Implement TCP?

Editor config
-------------

For editing code, I would recommend running the following periodically to
generate a `compile_commands.json` file:

    pip install --user compiledb  # do this just once
    compiledb -n make all

With this done, you should be able to use a C language server (e.g. clangd,
which I've verified to work) and client (e.g. vim-lsp, emacs, vscode). This will
enable things like jump-to-definition, code completion, and renaming.

Resources
---------

Here are some non-official resources I have found useful during this project:

- [Baking Pi][baking-pi] is a series of tutorials about writing bare metal
  programs for Raspberry Pi. I followed this for a while before starting this
  project, so it influenced me a lot. It's a lot of fun and has lots of great
  information.
- [This Stanford course website][course] has a lot of labs and references. For
  some of my earlier questions, it just kept popping up on Google, and it's
  really good.
- [Bare-metal programming for ARM][ebook] helped a me get interrupts working.
  This book has a fairly narrow focus, but I found it quite well-written and
  helpful for the topics it did cover.

[baking-pi]: https://www.cl.cam.ac.uk/projects/raspberrypi/tutorials/os/ok05.html
[course]: http://cs107e.github.io/
[ebook]: http://umanovskis.se/files/arm-baremetal-ebook.pdf

Here are some of the official standards and specifications I've relied on:

- [ARMv7-A Architecture Reference Manual][arm-arm]: the authoritative reference
  for ARM assembly, and other architecture topics like the Virtual Memory System
  Architecture (VMSA), the Generic Timer, exception and interrupt handling, and
  more.
- [ARM Generic Interrupt Controller Architecture Specification][arm-gic]:
  specifies how the GIC works, which is crucial for being able to handle,
  enable, and route interrupts.
- [PrimeCell PL011 UART Manual][pl011]
- [DeviceTree Specification][dtree]: This specifies the data structures which
  store information about the devices in a system. I've partially implemented
  this and will be revisiting it soon. When I last looked the spec was at
  version 0.2 but now there's a 0.3!
- [Virtio Specification][virtio]: This specifies how virtio devices work. This
  underlies my block and network devices.
- [RFC791][rfc791]: Internet Protocol!
- [RFC768][rfc768]: User Datagram Protocol.
- [RFC2131][rfc2131]: Dynamic Host Configuration Protocol. Also the [Wikipedia
  page][dhcp-wiki] and the [options spec (RFC1533)][rfc1533].
- [Microsoft FAT Specification][fat]

[arm-arm]: https://static.docs.arm.com/ddi0406/c/DDI0406C_C_arm_architecture_reference_manual.pdf
[arm-gic]: https://static.docs.arm.com/ihi0069/d/IHI0069D_gic_architecture_specification.pdf
[pl011]: http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183f/DDI0183.pdf
[dtree]: https://www.devicetree.org/specifications/
[virtio]: http://docs.oasis-open.org/virtio/virtio/v1.0/cs04/virtio-v1.0-cs04.html
[rfc791]: https://tools.ietf.org/html/rfc791
[rfc768]: https://tools.ietf.org/html/rfc768
[rfc2131]: https://tools.ietf.org/html/rfc2131
[rfc1533]: https://tools.ietf.org/html/rfc1533
[fat]: http://read.pudn.com/downloads77/ebook/294884/FAT32%20Spec%20%28SDA%20Contribution%29.pdf
[dhcp-wiki]: https://en.wikipedia.org/wiki/Dynamic_Host_Configuration_Protocol

License
-------

All told, this isn't a particularly functional or useful operating system yet,
but there may be pieces that are interesting or useful on their own. You're free
to use and modify the code as you'd like, under the terms of the MIT License,
provided in `LICENSE.txt`.
