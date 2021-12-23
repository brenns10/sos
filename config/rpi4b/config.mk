# Configuration Specific Makefile: rpi4b

TTY = /dev/ttyUSB1
.PHONY: pi-serial
run: kernel.bin
	@echo Launching raspbootcom!
	make -C submodules/raspbootin/raspbootcom
	submodules/raspbootin/raspbootcom/raspbootcom $(TTY) kernel.bin

.PHONY: jtag
jtag:
	@echo Launching OpenOCD. Once this succeeds, go run \"make gdb\" in a separate
	@echo terminal.
	openocd -f debug/rpi4b/c232hm.cfg -f debug/rpi4b/openocd.cfg

.PHONY: hwgdb
gdb:
	@echo NOTE: you must also be running \"make run\" and \"make jtag\" with the
	@echo appropriate connectors attached.
	$(GDB) -x debug/rpi4b/hwgdb
