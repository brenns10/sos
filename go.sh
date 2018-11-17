#!/bin/bash

# Stop on first failure
set -e

# Compile
arm-none-eabi-as -g uart.s -o uart.o

# Link
arm-none-eabi-ld -T test.ld uart.o -o test.elf

# Get the binary image
arm-none-eabi-objcopy -O binary test.elf test.bin

# Run as normal
#qemu-system-arm -M virt -kernel test.bin -nographic

# Run with gdb
qemu-system-arm -M virt -kernel test.bin -nographic -gdb tcp::9000 -S

# Run without a "system" - segfaults
#qemu-arm -singlestep -g 9000 test.elf
