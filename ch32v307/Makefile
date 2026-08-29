##### Project #####

PROJECT        ?= app
TOP             = .
BUILD_DIR       = _build

##### Options #####
LDSCRIPT        = vendor/Ld/Link.ld

LIB_FLAGS       = DEBUG=1

VERSION_FILE    = version
CONTENTS		= $$(cat $(VERSION_FILE))

ifndef SW_VERSION
SW_VERSION 	= $$(echo $(CONTENTS) | cut -d'.' -f1 | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/')
endif

GCC_PREFIX      ?= riscv32-wch-elf-

# C source folders
CDIRS := app \
vendor/Debug \
vendor/Core \
vendor/Peripheral/src

# ASM single files (CH32V307 = D8C variant)
AFILES  = vendor/Startup/startup_ch32v30x_D8C.S

LIBS    = vendor/Peripheral

# Include paths
INCLUDES := \
app \
vendor \
vendor/Debug \
vendor/Core \
vendor/Peripheral/inc vendor/Debug

##### Optional Libraries ############

include ./rules.mk
