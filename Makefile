.PHONY: clean debug run gdb

QEMU = qemu-system-arm -M virt
TOOLCHAIN = arm-none-eabi-
AS = $(TOOLCHAIN)as
CC = $(TOOLCHAIN)gcc

ASFLAGS = -g -march=armv6
CFLAGS = -g -ffreestanding -nostdlib -fPIC -march=armv6
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
kernel.elf: format.o
kernel.elf: sysinfo.o
kernel.elf: pages.o
kernel.elf: entry.o

%.bin: %.elf
	$(TOOLCHAIN)objcopy -O binary $< $@

%.elf:
	$(TOOLCHAIN)ld -T $(patsubst %.elf,%.ld,$@) $^ -o $@
	$(TOOLCHAIN)ld -T pre_mmu.ld $^ -o pre_mmu.elf

clean:
	rm -f *.o *.elf *.bin


