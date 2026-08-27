# ==============================================================================
# Makefile for CH32L103 WinUSB / WebUSB LED demo
#
# Toolchain: WCH's RISC-V GCC (bundled with MounRiver Studio) or any
# rv32imac-ilp32 riscv-gnu-toolchain build. Override TOOLCHAIN_PREFIX if your
# compiler binaries are named differently, e.g.:
#   make TOOLCHAIN_PREFIX=riscv64-unknown-elf-
#
# Typical MounRiver Studio toolchain location (adjust as needed):
#   ~/MounRiver/MounRiver_Studio/toolchain/RISC-V\ Embedded\ GCC12/bin/
# Add it to PATH, or pass TOOLCHAIN_BINDIR=/path/to/bin on the make command
# line.
# ==============================================================================

PROJECT        := winusb_webusb_led

# Default matches the toolchain bundled with MounRiver Studio 2
# (MRS2/.../WCH/Toolchain/RISC-V Embedded GCCxx/bin/riscv32-wch-elf-*).
TOOLCHAIN_PREFIX ?= riscv32-wch-elf-
TOOLCHAIN_BINDIR ?=

CC      := $(TOOLCHAIN_BINDIR)$(TOOLCHAIN_PREFIX)gcc
AS      := $(TOOLCHAIN_BINDIR)$(TOOLCHAIN_PREFIX)gcc
LD      := $(TOOLCHAIN_BINDIR)$(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_BINDIR)$(TOOLCHAIN_PREFIX)objcopy
OBJDUMP := $(TOOLCHAIN_BINDIR)$(TOOLCHAIN_PREFIX)objdump
SIZE    := $(TOOLCHAIN_BINDIR)$(TOOLCHAIN_PREFIX)size

BUILD_DIR := _build

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------
C_SOURCES := \
    $(wildcard User/*.c) \
    $(wildcard Peripheral/src/*.c) \
    Core/core_riscv.c \
    Debug/debug.c

ASM_SOURCES := Startup/startup_ch32l103.S

INCLUDES := \
    -IUser \
    -IDebug \
    -ICore \
    -IPeripheral/inc

# ------------------------------------------------------------------------------
# CPU / architecture flags (QingKe V4C core used by CH32L103: rv32imac)
# ------------------------------------------------------------------------------
ARCH_FLAGS := -march=rv32imac_zicsr_zifencei -mabi=ilp32 -msmall-data-limit=8 -msave-restore

CFLAGS := $(ARCH_FLAGS) \
    -DCH32L103 -Os -Wall -fmessage-length=0 -fsigned-char \
    -ffunction-sections -fdata-sections -fno-common \
    -g $(INCLUDES)

ASFLAGS := $(ARCH_FLAGS) -x assembler-with-cpp $(INCLUDES)

LDFLAGS := $(ARCH_FLAGS) \
    -nostartfiles -Wl,--gc-sections \
    -specs=nano.specs -specs=nosys.specs \
    -TLd/Link.ld \
    -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map

# ------------------------------------------------------------------------------
# Objects
# ------------------------------------------------------------------------------
OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.S=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))

# ------------------------------------------------------------------------------
# Targets
# ------------------------------------------------------------------------------
.PHONY: all clean flash size

all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).hex $(BUILD_DIR)/$(PROJECT).bin size

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROJECT).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

size: $(BUILD_DIR)/$(PROJECT).elf
	$(SIZE) $<

# Flash with WCH-LinkE using wlink (https://github.com/ch32-rs/wlink) if present.
flash: $(BUILD_DIR)/$(PROJECT).bin
	wlink flash $(BUILD_DIR)/$(PROJECT).bin

clean:
	rm -rf $(BUILD_DIR)
