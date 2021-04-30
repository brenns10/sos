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


Raspberry Pi SD Card & Firmware
-------------------------------

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


Bootloader
----------

From here, you could copy the `kernel.bin` into the sdcard (naming it
`kernel.img`), but copying it over every time you change the code and recompile
would be really tedious -- trust me, I've done it _a lot._

Instead, I use a serial bootloader to load SOS. The original Pi bootloader is
called raspbootin, but it was not updated with Pi 4 support. So, for the acutal
bootloader implementation, we use [imgrecv][imgrecv]. Clone the repo and use the
command to build it:

    cd raspi
    # may need to tweak ARM32CHAIN depending on compiler
    make ARM32CHAIN=arm-none-eabi- rpi4_32

Copied this to the sdcard named `imgrecv.bin`. Then, I put the following
contents into `config.txt`:

    kernel=imgrecv.bin
    enable_jtag_gpio=1
    enable_gic=1

With the bootloader, rather than using picocom, you need to use "raspbootcom" to
connect to the serial port. This interprets special escape sequences sent by the
bootloader, and sends the kernel to the bootloader. Clone
[raspbootcom][raspbootcom], and use the following:

    cd raspbootcom
    make

Then, run raspbootcom, with the SD card in the Pi, and the Pi connected via
serial.

    ./raspbootcom /dev/ttyUSB0 path/to/sos/kernel.bin

[imgrecv]: https://gitlab.com/brenns10/imgrecv
[raspbootcom]: https://github.com/brenns10/raspbootin

Plug the Pi into power and you should see the same bootup messages, followed by
a sequence of imgrecv/raspbootin messages. Then SOS will boot!

    Starting start4.elf @ 0xfec00200 partition 0

    imgrecv: ready

    ### sending kernel ../../sos/kernel.bin [122572 byte]
    ### finished sending
    imgrecv: launch bin


Debugging Setup
---------------

For debugging, you'll need the following connections with your JTAG cable:

* yellow -> pin 37 (JTAG TDI)
* orange -> pin 22 (JTAG TCK)
* green -> pin 18 (JTAG TDO)
* brown -> pin 13 (JTAG TMS)
* grey -> pin 15 (JTAG TRST)
* purple -> pin 16 (JTAG RTCK)

Install the absolute latest version of OpenOCD you can find. I have the Arch
`openocd-git` AUR package installed -- version 0.11.0, sha gc69b4deae.

To test it all out, use `make jtag`. If all works, you should see something like
this:

    $ make jtag
    Launching OpenOCD. Once this succeeds, go run "make hwgdb" in a separate
    terminal.
    openocd -f debug/rpi4b/c232hm.cfg -f debug/rpi4b/openocd.cfg
    Open On-Chip Debugger 0.11.0-rc1+dev-00010-gc69b4deae-dirty (2021-01-07-21:31)
    Licensed under GNU GPL v2
    For bug reports, read
            http://openocd.org/doc/doxygen/bugs.html
    DEPRECATED! use 'adapter speed' not 'adapter_khz'
    Warn : DEPRECATED! use '-baseaddr' not '-ctibase'
    Warn : DEPRECATED! use '-baseaddr' not '-ctibase'
    Warn : DEPRECATED! use '-baseaddr' not '-ctibase'
    Warn : DEPRECATED! use '-baseaddr' not '-ctibase'
    Info : Listening on port 6666 for tcl connections
    Info : Listening on port 4444 for telnet connections
    Info : clock speed 1000 kHz
    Info : JTAG tap: bcm2711.tap tap/device found: 0x4ba00477 (mfg: 0x23b (ARM Ltd), part: 0xba00, ver: 0x4)
    Warn : JTAG tap: bcm2711.tap       UNEXPECTED: 0x4ba00477 (mfg: 0x23b (ARM Ltd), part: 0xba00, ver: 0x4)
    Error: JTAG tap: bcm2711.tap  expected 1 of 1: 0xf8600077 (mfg: 0x03b (Integrated CMOS (Vertex)), part: 0x8600, ver: 0xf)
    Error: Trying to use configured scan chain anyway...
    Warn : Bypassing JTAG setup events due to errors
    Info : bcm2711.a72.0: hardware has 6 breakpoints, 4 watchpoints
    Info : bcm2711.a72.1: hardware has 6 breakpoints, 4 watchpoints
    Info : bcm2711.a72.2: hardware has 6 breakpoints, 4 watchpoints
    Info : bcm2711.a72.3: hardware has 6 breakpoints, 4 watchpoints
    Info : starting gdb server for bcm2711.a72.0 on 3333
    Info : Listening on port 3333 for gdb connections
    Info : starting gdb server for bcm2711.a72.1 on 3334
    Info : Listening on port 3334 for gdb connections
    Info : starting gdb server for bcm2711.a72.2 on 3335
    Info : Listening on port 3335 for gdb connections
    Info : starting gdb server for bcm2711.a72.3 on 3336
    Info : Listening on port 3336 for gdb connections

You can now use `make hwgdb` to connect and get started debugging.


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
