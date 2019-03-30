.PHONY: clean debug run gdb test

QEMU = qemu-system-arm -M virt
TOOLCHAIN = arm-none-eabi-
AS = $(TOOLCHAIN)as
CC = $(TOOLCHAIN)gcc

ASFLAGS = -g -march=armv6
CFLAGS = -g -ffreestanding -nostdlib -fPIC -march=armv6 -Ilib
LDFLAGS = -nostdlib

run: kernel.bin
	@echo Running. Exit with Ctrl-A X
	@echo
	$(QEMU) -kernel kernel.bin -nographic

debug: kernel.bin
	@echo Entering debug mode. Go run \"make gdb\" in another terminal.
	@echo You can terminate the qemu process with Ctrl-A X
	@echo
	$(QEMU) -kernel kernel.bin -nographic -gdb tcp::9000 -S

gdb:
	$(TOOLCHAIN)gdb -x gdbscript

# declare object files here
kernel.elf: uart.o
kernel.elf: startup.o
kernel.elf: main.o
kernel.elf: mem.o
kernel.elf: sysinfo.o
kernel.elf: entry.o
kernel.elf: c_entry.o
kernel.elf: process.o

kernel.elf: lib/list.o
kernel.elf: lib/format.o
kernel.elf: lib/alloc.o

lib/%.to: lib/%.c
	gcc -g -c $< -o $@ -Ilib/
tests/%.to: tests/%.c
	gcc -g -c $< -o $@ -Ilib/

tests/test_list: tests/test_list.to lib/list.to lib/unittest.to
	gcc -o $@ $^

test: tests/test_list
	@tests/test_list

%.bin: %.elf
	$(TOOLCHAIN)objcopy -O binary $< $@

%.elf:
	$(TOOLCHAIN)ld -T $(patsubst %.elf,%.ld,$@) $^ -o $@
	$(TOOLCHAIN)ld -T pre_mmu.ld $^ -o pre_mmu.elf

clean:
	rm -f *.o *.elf *.bin lib/*.o test/*.to lib/*.to
