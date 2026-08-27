##### Project #####

PROJECT        ?= app
# Top path of the template
TOP             = .
# The path for generated files
BUILD_DIR       = _build

##### Options #####
LDSCRIPT        = vendor/Ld/Link.ld

# use printf and UART1
LIB_FLAGS       = DEBUG=1

# version == file with number looks like 1015
VERSION_FILE    = version
CONTENTS		= $$(cat $(VERSION_FILE))

ifndef SW_VERSION
# if sw_version is not written by param - then get it from file
SW_VERSION 	= $$(echo $(CONTENTS) | cut -d'.' -f1 | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/')
endif

DT_UNIX 	?= $(shell date +%s)

# GCC 15: riscv32-wch-elf-
# GCC 12: riscv-none-elf-
# GCC 11 and below: riscv-none-embed-
GCC_PREFIX      ?= riscv32-wch-elf-

# C source folders
CDIRS := app \
vendor/RVMSIS \
vendor/StdPeriphDriver

# C source files (if there are any single ones)
CFILES  := \

# ASM source folders
ADIRS   :=
# ASM single files
AFILES  = vendor/Startup/startup_CH592.S

LIBS    = vendor/StdPeriphDriver

LIBNAMES = ISP592 \
		printf


# Include paths
INCLUDES := \
app \
vendor \
vendor/RVMSIS \
vendor/StdPeriphDriver/inc

##### Optional Libraries ############

include ./rules.mk
