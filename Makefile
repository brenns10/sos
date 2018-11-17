.PHONY: clean debug run gdb

QEMU = qemu-system-arm -M virt
TOOLCHAIN = arm-none-eabi-
AS = $(TOOLCHAIN)as
CC = $(TOOLCHAIN)gcc

ASFLAGS = -g
CFLAGS = -g -nostdlib

run: test.bin
	@echo Running. Exit with Ctrl-A X
	@echo
	$(QEMU) -kernel test.bin -nographic

debug: test.bin
	@echo Entering debug mode. Go run \"make gdb\" in another terminal.
	@echo You can terminate the qemu process with Ctrl-A X
	@echo
	$(QEMU) -kernel test.bin -nographic -gdb tcp::9000 -S

gdb:
	$(TOOLCHAIN)gdb -x gdbscript

# declare object files here
test.elf: uart.o

%.bin: %.elf
	$(TOOLCHAIN)objcopy -O binary $< $@

%.elf:
	$(TOOLCHAIN)ld -T test.ld $^ -o $@

clean:
	rm -f *.o *.elf *.bin


