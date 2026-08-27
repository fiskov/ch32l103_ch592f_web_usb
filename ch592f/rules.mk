# 'make V=1' will show all compiler calls.
V		?= 0
ifeq ($(V),0)
Q		:= @
NULL	:= 2>/dev/null
endif

PREFIX		?= $(GCC_PREFIX)
ifdef GCC_TOOLCHAIN
	PREFIX ?= $(GCC_TOOLCHAIN)/$(GCC_PREFIX)
endif
CC			= $(PREFIX)gcc
AS			= $(PREFIX)as
LD			= $(PREFIX)ld
OBJCOPY		= $(PREFIX)objcopy
OBJDUMP		= $(PREFIX)objdump
BDIR		= $(TOP)/$(BUILD_DIR)

# For each direcotry, add it to csources
CSOURCES := $(foreach dir, $(CDIRS), $(shell find $(TOP)/$(dir) -maxdepth 1 -name '*.c'))
# Add single c source files to csources
CSOURCES += $(addprefix $(TOP)/, $(CFILES))
# Then assembly source folders and files
ASOURCES := $(foreach dir, $(ADIRS), $(shell find $(TOP)/$(dir) -maxdepth 1 -iname '*.s'))
ASOURCES += $(addprefix $(TOP)/, $(AFILES))

LIBSOURCES := $(addprefix -L $(TOP)/, $(LIBS))

# Fill object files with c and asm files (keep source directory structure), format: $(var:a=b)
OBJS = $(CSOURCES:$(TOP)/%.c=$(BDIR)/%.o)
OBJS += $(ASOURCES:$(TOP)/%.s=$(BDIR)/%.o)
# d files for detecting h file changes
DEPS=$(CSOURCES:$(TOP)/%.c=$(BDIR)/%.d)

# Arch and target specified
# - GCC 12, CH32V20x (ch32v208 = D8W, no FPU, xw extension)
ARCH_FLAGS	:= -march=rv32imacxw -mabi=ilp32 \
			-mcmodel=medany -msmall-data-limit=8 -mno-save-restore \
			-fmessage-length=0 \
			-fsigned-char

# Debug options, -gdwarf-2 for debug, -g0 for release 
# https://gcc.gnu.org/onlinedocs/gcc-12.2.0/gcc/Debugging-Options.html
#  -g: system’s native format, -g0:off, -g/g1,-g2,-g3 -> more verbosely
#  -ggdb: for gdb, -ggdb0:off, -ggdb/ggdb1,-ggdb2,-ggdb3 -> more verbosely
#  -gdwarf: in DWARF format, -gdwarf-2,-gdwarf-3,-gdwarf-4,-gdwarf-5
DEBUG_FLAGS ?= -g0 #-gdwarf-3

# c flags
OPT			?= -Os -Wextra # -Og
CSTD		?= -std=gnu99
TGT_CFLAGS 	+= $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) $(CSTD) $(addprefix -D, $(LIB_FLAGS)) \
				-DSW_VERSION_=$(SW_VERSION) -DDT_UNIX=$(DT_UNIX) \
				-Wall -ffunction-sections -fdata-sections -fno-common \
				--param=highcode-gen-section-name=1 \
				-Werror -fmax-errors=3

# asm flags
TGT_ASFLAGS += $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) -Wa,--war

# ld flags
TGT_LDFLAGS += $(ARCH_FLAGS) $(OPT) -Wall -nostartfiles \
				-Xlinker --gc-sections -Wl,--print-memory-usage -Wl,-Map,$(BDIR)/$(PROJECT).map \
				--specs=nano.specs --specs=nosys.specs


# include paths
TGT_INCFLAGS := $(addprefix -I $(TOP)/, $(INCLUDES))

LIB_NAMES := $(addprefix -l, $(LIBNAMES))

.PHONY: all clean flash echo

all: $(BDIR)/$(PROJECT).elf $(BDIR)/$(PROJECT).bin $(BDIR)/$(PROJECT).hex 
#$(BDIR)/$(PROJECT).lst

# for debug
echo:
	$(info 1. AFILES  : $(AFILES))
	$(info 2. ASOURCES: $(ASOURCES))
	$(info 3. CSOURCES: $(CSOURCES))
	$(info 4. OBJS    : $(OBJS))
	$(info 5. INCLUDES: $(TGT_INCFLAGS))
	$(info 6. TGT_CFLAGS: $(TGT_CFLAGS))

# include d files without non-exist warning
-include $(DEPS)

# Compile c to obj
$(BDIR)/%.o: $(TOP)/%.c
	@printf "  CC\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_CFLAGS) $(TGT_INCFLAGS) -MT $@ -o $@ -c $< -MMD -MP -MF $(BDIR)/$*.d

# Compile asm to obj
$(BDIR)/%.o: $(TOP)/%.s
	@printf "  AS\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_ASFLAGS) -o $@ -c $<

# Link object files to elf
$(BDIR)/$(PROJECT).elf: $(OBJS) $(TOP)/$(LDSCRIPT)
	@printf "  LD\t$@\n"
	$(Q)$(CC) $(TGT_LDFLAGS) $(TGT_INCFLAGS) $(LIBSOURCES) -T$(TOP)/$(LDSCRIPT) $(OBJS) -o $@ $(LIB_NAMES)
	@printf "  SW_VERSION == ${SW_VERSION}\n"

# Convert elf to bin
%.bin: %.elf
	@printf "  OBJCP BIN\t$@\n"
	$(Q)$(OBJCOPY) -O binary  $< $@

# Convert elf to hex
%.hex: %.elf
	@printf "  OBJCP HEX\t$@\n"
	$(Q)$(OBJCOPY) -O ihex  $< $@

%.lst: %.elf
	@printf "  OBJDP LST\t$@\n"
	$(Q)$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $@

clean:
	rm -rf $(BDIR)/*

flash:
	@printf "  FLASH\t$(BDIR)/$(PROJECT).hex\n"
	wchisp flash $(BDIR)/$(PROJECT).hex

flash-openocd:
	@printf "  FLASH\t$(BDIR)/$(PROJECT).hex\n"
	openocd -f "interface/wch-riscv.cfg" -c "program $(BDIR)/$(PROJECT).hex reset exit;"
#	$(OPENOCD_ROOT)/openocd -f "interface/wch-riscv.cfg" -c "init; halt; flash protect 0 0 last off; program $(BDIR)/$(PROJECT).hex verify reset exit;"

#flash:
#	wlink flash $(BDIR)/$(PROJECT).hex
