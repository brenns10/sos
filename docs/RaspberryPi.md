Raspberry Pi Support
====================

SOS currently targets the ARMv7-A architecture. Several Raspberry Pi models
support this architecture, see the specification table at [Wikipedia][wiki] for
more information.

[wiki]: https://en.wikipedia.org/wiki/Raspberry_Pi#Specifications

Currently, the only specific model supported is the Raspberry Pi 4B. This has a
quad-core ARM Cortex-A72, which actually implements the ARMv8-A architecture.
This is backward compatible, having a 32-bit mode which emulates the ARMv7.
The reason for this particular model is simply that it is the only ARMv7+ model
I own.


Hardware Requirements
---------------------

For the full development experience, you'll want the following:

1. Raspberry Pi 4B (obviously) (*)
2. MicroSD card (any size will do the trick). Be sure you have a port or adapter
   to read this card on your PC. (*)
3. USB to TTL Serial cable. (*)  I have this one:
   https://www.adafruit.com/product/954
4. JTAG cable for Raspberry Pi. I have this one:
   https://www.digikey.com/product-detail/en/ftdi-future-technology-devices-international-ltd/C232HM-DDHSL-0/768-1106-ND/2714139?utm_source=oemsecrets&utm_medium=aggregator&utm_campaign=buynow

If you don't intend to do debugging, you only need the (*) starred items. But
the JTAG is super useful.


Serial Setup
------------

Your serial cable should be connected as follows:

* black cable to pin 6 (Ground)
* white cable to pin 8 (TXD)
* green cable to pin 10 (RXD)
* LEAVE THE POWER CABLE (RED) UNPLUGGED! (it won't break anything... but it also
  won't work)

Which pin is which?

https://pinout.xyz/#

I only refer to Raspberry Pi pin numbers (not BCM).

From then, your connection should be simple, I use picocom but you can use any
suitable serial terminal program. My picocom invocation:

    picocom /dev/ttyUSB1 -b 115200

The file sometimes changes, but the baud rate is very important!


Raspberry Pi Setup
------------------

The Raspberry Pi boots (primarily) from an SD card. The SD card should be
FAT(32) formatted and contain several important firmware and configuration
files. The firmware files can be found at the [raspberrypi/firmware][rpi-fw]
repository. Simply download the latest zip and copy the contents of the boot
directory to the SD card.

[rpi-fw]: https://github.com/raspberrypi/firmware

The Pi 4B additionally uses an EEPROM to store some (apparently complex) boot
code. This can be found at the [rpi-eeprom][rpi-eeprom] repository. In
particular, you really need to do the following:

1. Use the [Bootloader Configuration][cfg] article to configure `BOOT_UART=1`,
   this is *so* helpful. You should start with a bootloader version of at least
   2020-06-15.
2. Take the configured firmware file from step 1 and use the "recovery.bin"
   method to flash the EEPROM. (This basically means, copy recovery.bin and the
   configured bootloader file to the SD card and boot it).

[rpi-eeprom]: https://github.com/raspberrypi/rpi-eeprom/tree/master/firmware
[cfg]: https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2711_bootloader_config.md

If all this works, then your configured serial terminal should stream boot
messages as the Pi powers on, regardless of whether you even have anything in
the SD card slot, let alone a bootable image. This is super useful for
debugging: you'll know that your serial cable is working, ruling out a hardware
issue!

From there, you need to copy two SOS specific files onto the SD card:

1. A config.txt file, located in this same directory `rpiconfig.txt`. Be sure to
   rename it config.txt within the SD card. There are several very important
   configuration values in that file, it is very much required.
2. The kernel.bin file, produced by building `make config_rpi4b all`. I rename
   this to `sos.bin` within the SD card.

At this point, you should be able to boot the OS (to whatever level of
functionality I have implemented).


Debugging Setup
---------------

For debugging, you'll need the following connections with your JTAG cable:

* yellow -> pin 37 (JTAG TDI)
* orange -> pin 22 (JTAG TCK)
* green -> pin 18 (JTAG TDO)
* blue -> pin 16 (JTAG RTCK)
* grey -> pin 15 (JTAG TRST)
* brown -> pin 13 (JTAG TMS)

Furthermore, you'll need to clone the latest and greatest OpenOCD (use master,
don't try to use a versioned release). Configure and compile it. I had to use
the following configure invocation because my compiler is newer and has more
compiler warnings (which become errors in their build):

    /configure "CFLAGS=-Wno-implicit-fallthrough -Wno-tautological-compare -Wno-format-overflow -Wno-cpp" --enable-cmsis-dap

You'll need to run openocd as root (or add a udev rule, I forget tbh).

You'll next need my configuration file, which I've stored in the `debug/rpi4b`
folder as well, named openocd.cfg. Then, you'll be able to run these commands:

    # you can just run openocd from the build directory, no need to install
    sudo src/openocd -f debug/rpi4b/openocd.cfg

    # in another terminal
    arm-none-eabi-gdb -x debug/rpi4b/hwgdb

Some notes about JTAG debugging in this particular instance:

1. It seems that anything involving a breakpoint is not supported. GDB
   implements the break point by inserting a halt instruction, but it doesn't
   seem to know how to remove the instruction! This is probably a bug with my
   openocd / gdb configuration, but I don't know how to fix it.

   You can work around this by sticking infinite loops into your code. It's
   messy but it works.

2. You must wait until the board finishes printing bootloader uart before
   starting openocd.


Documents & Resources
---------------------

OSDev wiki which has some useful information:
https://wiki.osdev.org/Raspberry_Pi_Bare_Bones#Pi_3.2C_4

This mbox code was pretty useful to get started, but it is in ARM assembly :(
https://github.com/inaciose/noos/blob/master/pi3/blinker02/greenled.s

Determines pin numbers of LED / PWR
https://github.com/raspberrypi/firmware/blob/master/extra/dt-blob.dts#L1175

JTAG article:
https://metebalci.com/blog/bare-metal-raspberry-pi-3b-jtag/
