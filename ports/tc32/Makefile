include ../../py/mkenv.mk

BOARD ?= LT716
BOARD_DIR ?= boards/$(BOARD)

TC32_HOME = /opt/pkg/tc32
TC32_SDK = $(HOME)/src/8232_BLE_SDK/ble_sdk_hawk

CROSS_COMPILE = $(TC32_HOME)/bin/tc32-elf-

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

# MicroPython feature configurations
MICROPY_ROM_TEXT_COMPRESSION ?= 1

FROZEN_MANIFEST = manifest.py

include ../../py/mkenv.mk
include $(BOARD_DIR)/mpconfigboard.mk

MCU_SERIES_LOWER = $(shell echo $(MCU_SERIES) | tr '[:upper:]' '[:lower:]')

include mcu/$(MCU_SERIES_LOWER)/mpconfigmcu.mk

# include py core make definitions
include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk

INC += -I.
INC += -Iinclude
INC += -I$(TC32_SDK)
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -Imcu/$(MCU_SERIES_LOWER)

MAKE_PINS = boards/make-pins.py
BOARD_PINS = $(BOARD_DIR)/pins.csv
PREFIX_FILE = boards/pins_prefix.c
AF_FILE = mcu/$(MCU_SERIES_LOWER)/pin-af-table.csv
GEN_PINS_SRC = $(BUILD)/pins_$(BOARD).c
GEN_PINS_HDR = $(HEADER_BUILD)/pins.h

CFLAGS_TC32 = -fdata-sections -ffunction-sections -DMCU_STARTUP_5316 -fno-jump-tables
CFLAGS += $(INC) -Wall -Werror -std=c99 -nostdlib $(CFLAGS_TC32) $(COPT)
LDFLAGS += -nostdlib -T $(TC32_SDK)/boot.link -Map=$@.map --cref --gc-sections

CFLAGS += -Os -DNDEBUG -g -Wno-error
CFLAGS_TC32 += -DMCU_$(MCU_SERIES)

# Bits of Micropython are in C11. However, Telink's compiler doesn't support
# this, but luckily the bits we need (anonymous unions) are a GNU extension.
CFLAGS_TC32 += -std=gnu99

# Work around bugs in Telink's SDK.
CFLAGS_TC32 += -isystem include

# LFS configuration.
CFLAGS_TC32 += -DLFS1_NO_MALLOC
CFLAGS_TC32 += -DLFS1_NO_DEBUG
CFLAGS_TC32 += -DLFS1_NO_WARN
CFLAGS_TC32 += -DLFS1_NO_ERROR
CFLAGS_TC32 += -DLFS1_NO_ASSERT

# Flags for optional C++ source code
CXXFLAGS += $(filter-out -std=c99,$(CFLAGS))

LIBS = $(TC32_SDK)/proj_lib/liblt_5316.a \
	$(shell $(CC) -print-libgcc-file-name)

SRC_C += \
	main.c \
	uart.c \
	gccollect.c \
	machine_pin.c \
	tc32_flash.c \
	modtc32.c \
	math.c \
	$(TC32_SDK)/drivers/5316/adc.c \
	$(TC32_SDK)/drivers/5316/analog.c \
	$(TC32_SDK)/drivers/5316/bsp.c \
	$(TC32_SDK)/drivers/5316/clock.c \
	$(TC32_SDK)/drivers/5316/gpio.c \
	$(TC32_SDK)/drivers/5316/uart.c \
	$(TC32_SDK)/drivers/5316/flash.c \
	shared/libc/string0.c \
	shared/libc/printf.c \
	shared/readline/readline.c \
	shared/runtime/pyexec.c \
	shared/runtime/stdout_helpers.c \
	shared/runtime/gchelper_native.c \
	lib/littlefs/lfs1.c \
	lib/littlefs/lfs1_util.c \

SRC_S += \
	$(TC32_SDK)/boot/5316/cstartup_5316.S \
	shared/runtime/gchelper_tc32.S \

SRC_QSTR += shared/readline/readline.c shared/runtime/pyexec.c $(GEN_PINS_SRC)
SRC_QSTR += machine_pin.c main.c modtc32.c

OBJ += $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_S:.S=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_CXX:.cpp=.o))
OBJ += $(GEN_PINS_SRC:.c=.o)

all: $(BUILD)/firmware.bin

$(BUILD)/firmware.elf: $(BUILD)/$(TC32_SDK)/boot/5316/cstartup_5316.o $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(Q)$(SIZE) $@

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O binary $^ $@

# Use a pattern rule here so that make will only call make-pins.py once to make
# both pins_$(BOARD).c and pins.h
$(BUILD)/%_$(BOARD).c $(HEADER_BUILD)/%.h: $(BOARD_DIR)/%.csv $(MAKE_PINS) $(AF_FILE) $(PREFIX_FILE) | $(HEADER_BUILD)
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(MAKE_PINS) --board-csv $(BOARD_PINS) --af-csv $(AF_FILE) --prefix $(PREFIX_FILE) \
		--output-source $(GEN_PINS_SRC) --output-header $(GEN_PINS_HDR) \
		--mcu $(MCU_SERIES)

include $(TOP)/py/mkrules.mk
