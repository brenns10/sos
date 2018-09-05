#!/bin/bash
arm-none-eabi-as -g uart.s -o uart.o
arm-none-eabi-ld -T test.ld uart.o -o test.elf
#arm-none-eabi-ld uart.o -o test
arm-none-eabi-objcopy -O binary test.elf test.bin
qemu-system-arm -M virt -kernel test.elf -nographic
#qemu-system-arm -M virt -kernel test.bin -nographic -gdb tcp::9000 -S
