Ubuntu Setup
============

Ubuntu has two issues which make the setup slightly more involved (but not that
much more so). First is that the QEMU package is pretty outdated (we require
4.2+). Second is that Ubuntu has a GDB which can work on any architecture,
rather than having a arch-specific one. We can address these issues by creating
a file `conf.mk` and overriding some tool selections.

But first, install required packages like so:

    sudo apt install qemu-system-arm build-essential gcc-arm-none-eabi \
        binutil-arm-none-eabi gdb-multiarch libpixman-1-dev

QEMU Setup
----------

First, install your default QEMU so we can check the version.

    sudo apt install qemu-system-arm
    qemu-system-arm --version

If the version is >=4.2, then you're good! Proceed to the next section.
Otherwise, you'll need to build and install version 4.2 or greater. These steps
will download 4.2, compile it, and install it into your home directory:

    wget https://download.qemu.org/qemu-4.2.0.tar.xz
    tar xf qemu-4.2.0.tar.xz
    cd qemu-4.2.0
    ./configure --target-list=arm-softmmu --prefix=$HOME/qemu-local
    make -j8  # set the -j arg based on your system's CPUs
    make install

Now, you can place the following into your `conf.mk`:

    QEMU := $(HOME)/qemu-local/bin/qemu-system-arm

GDB Setup
---------

Place the following into `conf.mk` to set the GDB version you'll use:

    GDB := gdb-multiarch
