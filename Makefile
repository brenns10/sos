ASFLAGS := -g
CFLAGS := -g -ffreestanding -nostdlib -fPIE  -Wall -Wno-address-of-packed-member
CPPFLAGS := -iquote lib \
            -iquote include \
            -iquote kernel
LDFLAGS := -nostdlib -fPIE

TEST_CFLAGS = -fprofile-arcs -ftest-coverage -lgcov -g -DTEST_PREFIX
HOSTCC := gcc

include ./arch/config.mk
include ./config/config.mk

CPPFLAGS += -isystem arch/$(ARCH)/include
AS := $(TOOLCHAIN)as
CC := $(TOOLCHAIN)gcc
LD := $(TOOLCHAIN)ld
OBJCOPY := $(TOOLCHAIN)objcopy
GDB := $(TOOLCHAIN)gdb
PYTEST := python3 -m pytest

# User configuration overrides
-include conf.mk

.PHONY: all
all: kernel.bin compile_unittests

# Object files going into the kernel:
kernel.elf: arch/$(ARCH)/mmu.o
kernel.elf: arch/$(ARCH)/startup.o
kernel.elf: kernel/uart.o
kernel.elf: kernel/smain.o
#kernel.elf: kernel/main.o
kernel.elf: kernel/kmem.o
#kernel.elf: kernel/sysinfo.o
#kernel.elf: kernel/entry.o
#kernel.elf: kernel/c_entry.o
#kernel.elf: kernel/process.o
#kernel.elf: kernel/rawdata.o
#kernel.elf: kernel/dtb.o
#kernel.elf: kernel/ksh.o
#kernel.elf: kernel/timer.o
#kernel.elf: kernel/gic.o
#kernel.elf: kernel/syscall.o
#kernel.elf: kernel/virtio.o
#kernel.elf: kernel/virtio-blk.o
#kernel.elf: kernel/virtio-net.o
#kernel.elf: kernel/eth.o
#kernel.elf: kernel/ip.o
#kernel.elf: kernel/udp.o
#kernel.elf: kernel/netutils.o
#kernel.elf: kernel/dhcp.o
#kernel.elf: kernel/kmalloc.o
#kernel.elf: kernel/socket.o
#kernel.elf: kernel/user.o
#kernel.elf: kernel/wait.o
#kernel.elf: kernel/cxtk.o
#kernel.elf: kernel/debug.o
#kernel.elf: kernel/blk.o
#kernel.elf: kernel/fat.o
#kernel.elf: kernel/rpi-gpio.o
#kernel.elf: kernel/arm-mailbox.o
#kernel.elf: kernel/sync.o
#kernel.elf: kernel/fs.o
#kernel.elf: kernel/ldisc.o
#kernel.elf: kernel/setctx.o
#
#kernel.elf: lib/list.o
kernel.elf: lib/format.o
kernel.elf: lib/alloc.o
kernel.elf: lib/string.o
#kernel.elf: lib/util.o
#kernel.elf: lib/slab.o
#kernel.elf: lib/math.o
#kernel.elf: lib/inet.o
#
#kernel.elf: board/qemu.o
#kernel.elf: board/rpi4b.o
#
#kernel/ksh.o: kernel/ksh_commands.h

# Object files going into each userspace program:
USER_BASIC = user/syscall.o user/startup.o
user/salutations.elf: user/salutations.o lib/format.o $(USER_BASIC)
user/hello.elf: user/hello.o lib/format.o $(USER_BASIC)
user/ush.elf: user/ush.o lib/format.o lib/string.o lib/inet.o $(USER_BASIC)

# Userspace bins going into the kernel:
kernel/rawdata.o: user/salutations.bin user/hello.bin user/ush.bin

# To build a userspace program:
user/%.elf:
	$(LD) -T user.ld $^ -o $@ -M > $(patsubst %.elf,%.map,$@)
user/%.bin: user/%.elf
	$(OBJCOPY) -O binary $< $@

# To build the kernel
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

# Builds kernel.elf, also preprocesses the linker based on config/arch specific
# changes.
%.elf:
	$(CC) -E -x c $(CPPFLAGS) $(patsubst %.elf,arch/$(ARCH)/%.ld.in,$@) | grep -v '^#' >$(patsubst %.elf,arch/$(ARCH)/%.ld,$@)
	$(CC) -E -D PREMMU -x c $(CPPFLAGS) $(patsubst %.elf,arch/$(ARCH)/%.ld.in,$@) | grep -v '^#' >$(patsubst %.elf,arch/$(ARCH)/%.pre_mmu.ld,$@)
	$(LD) -T $(patsubst %.elf,arch/$(ARCH)/%.ld,$@) $^ -o $@ -M > $(patsubst %.elf,%.map,$@)
	$(LD) -T $(patsubst %.elf,arch/$(ARCH)/%.pre_mmu.ld,$@) $^ -o pre_mmu.elf

#
# Unit tests
#
lib/%.to: lib/%.c
	$(HOSTCC) $(CPPFLAGS) $(TEST_CFLAGS) -g -c $< -o $@ -iquote lib/
unittests/%.to: unittests/%.c
	$(HOSTCC) $(CPPFLAGS) $(TEST_CFLAGS) -g -c $< -o $@ -iquote lib/

unittests/list.test: unittests/test_list.to lib/list.to lib/unittest.to
	$(HOSTCC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $^
unittests/alloc.test: unittests/test_alloc.to lib/alloc.to lib/unittest.to
	$(HOSTCC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $^
unittests/slab.test: unittests/test_slab.to lib/slab.to lib/unittest.to lib/list.to
	$(HOSTCC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $^
unittests/format.test: unittests/test_format.to lib/format.to lib/unittest.to
	$(HOSTCC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $^
unittests/inet.test: unittests/test_inet.to lib/inet.to lib/unittest.to
	$(HOSTCC) $(CPPFLAGS) $(TEST_CFLAGS) -o $@ $^

.PHONY: compile_unittests
compile_unittests: unittests/list.test unittests/alloc.test unittests/slab.test unittests/format.test unittests/inet.test

.PHONY: unittest
unittest: compile_unittests
	rm -f cov*.html *.gcda lib/*.gcda unittests/*.gcda
	@unittests/list.test
	@unittests/alloc.test
	@unittests/slab.test
	@unittests/format.test
	@unittests/inet.test
	gcovr -r . --html --html-details -o cov.html lib/ unittests/

.PHONY: test
test: unittest

.PHONY: clean
clean:
	rm -f *.elf *.bin
	rm -f kernel/*.o
	rm -f kernel/configvals.h arch/config.mk config/config.mk
	rm -f board/*.o
	rm -f lib/*.o unittests/*.to lib/*.to
	rm -f user/*.o user/*.elf user/*.bin
	rm -f unittests/*.gcda unittests/*.gcno unittests/*.to unittests/*.test
	rm -f cov.*.html
	rm -f dump.pcap
