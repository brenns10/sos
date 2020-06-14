QEMU = qemu-system-arm
QEMU_CMD = $(QEMU) -M virt -global virtio-mmio.force-legacy=false -nographic \
       -drive file=mydisk,if=none,format=raw,id=hd -device virtio-blk-device,drive=hd \
       -netdev user,id=u1 -device virtio-net-device,netdev=u1 -object filter-dump,id=f1,netdev=u1,file=dump.pcap \
       -d guest_errors
QEMU_DBG = -gdb tcp::9000 -S
TOOLCHAIN ?= arm-none-eabi-
AS = $(TOOLCHAIN)as
CC = $(TOOLCHAIN)gcc
LD = $(TOOLCHAIN)ld
OBJCOPY = $(TOOLCHAIN)objcopy
GDB = $(TOOLCHAIN)gdb

HOSTCC = gcc

ARCH := armv7-a
ASFLAGS = -g -march=$(ARCH)
CFLAGS = -g -ffreestanding -nostdlib -fPIC -march=$(ARCH) -iquote lib \
         -iquote include -marm -Wall -Wno-address-of-packed-member
LDFLAGS = -nostdlib

TEST_CFLAGS = -fprofile-arcs -ftest-coverage -lgcov -g -DTEST_PREFIX

# Include here to allow overriding settings via a more permanent conf.mk
-include conf.mk

.PHONY: run
run: kernel.bin mydisk
	@echo Running. Exit with Ctrl-A X
	@echo
	$(QEMU_CMD) -kernel kernel.bin

.PHONY: all
all: kernel.bin compile_unittests

.PHONY: debug
debug: kernel.bin mydisk
	@echo Entering debug mode. Go run \"make gdb\" in another terminal.
	@echo You can terminate the qemu process with Ctrl-A X
	@echo
	$(QEMU_CMD) $(QEMU_DBG) -kernel kernel.bin

.PHONY: gdb
gdb:
	$(GDB) -x gdbscript

mydisk:
	dd if=/dev/zero of=mydisk bs=1M count=1

# Object files going into the kernel:
kernel.elf: kernel/uart.o
kernel.elf: kernel/startup.o
kernel.elf: kernel/main.o
kernel.elf: kernel/kmem.o
kernel.elf: kernel/sysinfo.o
kernel.elf: kernel/entry.o
kernel.elf: kernel/c_entry.o
kernel.elf: kernel/process.o
kernel.elf: kernel/rawdata.o
kernel.elf: kernel/dtb.o
kernel.elf: kernel/ksh.o
kernel.elf: kernel/timer.o
kernel.elf: kernel/gic.o
kernel.elf: kernel/syscall.o
kernel.elf: kernel/virtio.o
kernel.elf: kernel/virtio-blk.o
kernel.elf: kernel/virtio-net.o
kernel.elf: kernel/eth.o
kernel.elf: kernel/ip.o
kernel.elf: kernel/udp.o
kernel.elf: kernel/netutils.o
kernel.elf: kernel/dhcp.o
kernel.elf: kernel/kmalloc.o
kernel.elf: kernel/socket.o
kernel.elf: kernel/user.o
kernel.elf: kernel/wait.o
kernel.elf: kernel/cxtk.o
kernel.elf: kernel/debug.o

kernel.elf: lib/list.o
kernel.elf: lib/format.o
kernel.elf: lib/alloc.o
kernel.elf: lib/string.o
kernel.elf: lib/util.o
kernel.elf: lib/slab.o
kernel.elf: lib/math.o
kernel.elf: lib/inet.o

# Object files going into each userspace program:
USER_BASIC = user/syscall.o user/startup.o
user/salutations.elf: user/salutations.o lib/format.o $(USER_BASIC)
user/hello.elf: user/hello.o lib/format.o $(USER_BASIC)
user/ush.elf: user/ush.o lib/format.o lib/string.o lib/inet.o $(USER_BASIC)

# Userspace bins going into the kernel:
kernel/rawdata.o: user/salutations.bin user/hello.bin user/ush.bin

# To build a userspace program:
user/%.elf:
	$(LD) -T user.ld $^ -o $@
user/%.bin: user/%.elf
	$(OBJCOPY) -O binary $< $@

# To build the kernel
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
%.elf:
	$(LD) -T $(patsubst %.elf,%.ld,$@) $^ -o $@ -M > $(patsubst %.elf,%.map,$@)
	$(LD) -T pre_mmu.ld $^ -o pre_mmu.elf

#
# Unit tests
#
lib/%.to: lib/%.c
	$(HOSTCC) $(TEST_CFLAGS) -g -c $< -o $@ -iquote lib/
unittests/%.to: unittests/%.c
	$(HOSTCC) $(TEST_CFLAGS) -g -c $< -o $@ -iquote lib/

unittests/list.test: unittests/test_list.to lib/list.to lib/unittest.to
	$(HOSTCC) $(TEST_CFLAGS) -o $@ $^
unittests/alloc.test: unittests/test_alloc.to lib/alloc.to lib/unittest.to
	$(HOSTCC) $(TEST_CFLAGS) -o $@ $^
unittests/slab.test: unittests/test_slab.to lib/slab.to lib/unittest.to lib/list.to
	$(HOSTCC) $(TEST_CFLAGS) -o $@ $^
unittests/format.test: unittests/test_format.to lib/format.to lib/unittest.to
	$(HOSTCC) $(TEST_CFLAGS) -o $@ $^
unittests/inet.test: unittests/test_inet.to lib/inet.to lib/unittest.to
	$(HOSTCC) $(TEST_CFLAGS) -o $@ $^

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

.PHONY: integrationtest
integrationtest: kernel.bin mydisk
	@QEMU_CMD="$(QEMU_CMD)" pytest integrationtests

.PHONY: testdebug
testdebug:
	@SOS_DEBUG=true QEMU_CMD="$(QEMU_CMD) $(QEMU_DBG)" pytest integrationtests -k $(TEST) -s

.PHONY: test
test: unittest integrationtest

.PHONY: clean
clean:
	rm -f *.elf *.bin
	rm -f kernel/*.o
	rm -f lib/*.o unittests/*.to lib/*.to
	rm -f user/*.o user/*.elf user/*.bin
	rm -f unittests/*.gcda unittests/*.gcno unittests/*.to unittests/*.test
	rm -f cov.*.html
	rm -f dump.pcap
