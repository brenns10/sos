# Architecture Specific Configuration: arm (32-bit)
ARCH := arm
TOOLCHAIN := arm-none-eabi-

# Arch code needs to setup some qemu-related details, even if the chosen
# configuration is a physical board.
QEMU := qemu-system-arm -M virt

ASFLAGS += -march=armv8-a
CFLAGS += -march=armv7-a
