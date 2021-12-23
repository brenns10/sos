# Architecture Specific Configuration: aarch64
ARCH := aarch64
TOOLCHAIN := aarch64-linux-gnu-

# Arch code needs to setup some qemu-related details, even if the chosen
# configuration is a physical board.
QEMU := qemu-system-aarch64 -M virt -cpu max

ASFLAGS += -march=armv8-a
CFLAGS += -march=armv8-a -mgeneral-regs-only -DARCH=aarch64
# TODO: remove -mgeneral-regs-only when I safely enable SIMD for kernel
