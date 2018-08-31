#!/bin/bash
arm-none-eabi-as uart.s -o uart.o
arm-none-eabi-ld -T test.ld uart.o -o test.elf
arm-none-eabi-objcopy -O binary test.elf test.bin
qemu-system-arm -M virt -kernel test.bin -nographic
