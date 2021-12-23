# Configuration Specific Makefile: qemu

QEMU_ARGS := -nographic -m size=1G \
	-global virtio-mmio.force-legacy=false \
	-drive file=mydisk,if=none,format=raw,id=hd -device virtio-blk-device,drive=hd \
	-netdev user,id=u1 -device virtio-net-device,netdev=u1 -object filter-dump,id=f1,netdev=u1,file=dump.pcap \
	-d guest_errors
# Consider adding to -d when debugging:
#    trace:virtio_blk* (or really virtio*)

QEMU_DBG = -gdb tcp::9000 -S

.PHONY: run
run: kernel.bin mydisk
	@echo Running. Exit with Ctrl-A X
	@echo
	$(QEMU) $(QEMU_ARGS) -kernel kernel.bin

.PHONY: debug
debug: kernel.bin mydisk
	@echo Entering debug mode. Go run \"make gdb\" in another terminal.
	@echo You can terminate the qemu process with Ctrl-A X
	@echo
	$(QEMU) $(QEMU_ARGS) $(QEMU_DBG) -kernel kernel.bin

.PHONY: gdb
gdb:
	$(GDB) -x gdbscript

mydisk:
	dd if=/dev/zero of=mydisk bs=1M count=1

.PHONY: integrationtest
integrationtest: kernel.bin mydisk
	@QEMU_CMD="$(QEMU_CMD)" $(PYTEST) integrationtests

.PHONY: integrationtestpdb
integrationtestpdb: kernel.bin mydisk
	@QEMU_CMD="$(QEMU_CMD)" $(PYTEST) integrationtests --pdb

.PHONY: testdebug
testdebug:
	@SOS_DEBUG=true QEMU_CMD="$(QEMU_CMD) $(QEMU_DBG)" $(PYTEST) integrationtests -k $(TEST) -s

test: integrationtest
