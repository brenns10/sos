arm
===

Currently toying with writing code to run on a "bare metal" ARM processor.
Hoping to work up to a small operating system. Right now, hello world works.

You'll probably need to install the arm-non-eabi toolchain, and qemu for ARM.
Then you can do:

    # run the code
    make run

    # do debug mode
    make debug
    # (in another terminal)
    make gdb
