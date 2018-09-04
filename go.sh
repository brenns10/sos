#!/bin/bash

alias leg-none-eabi-as='arm-none-eabi-as'
alias leg-none-eabi-ld='arm-none-eabi-ld'
alias leg-none-eabi-objcopy='arm-none-eabi-objcopy'

leg-none-eabi-as uart.s -o uart.o
leg-none-eabi-ld -T test.ld uart.o -o test.elf
leg-none-eabi-objcopy -O binary test.elf test.bin
qemu-system-arm -M virt -kernel test.bin -nographic
