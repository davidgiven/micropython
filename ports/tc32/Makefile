include ../../py/mkenv.mk

TC32_HOME = /opt/pkg/tc32
TC32_SDK = $(HOME)/src/8232_BLE_SDK/ble_sdk_hawk

CROSS_COMPILE = $(TC32_HOME)/bin/tc32-elf-

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

# MicroPython feature configurations
MICROPY_ROM_TEXT_COMPRESSION ?= 1

# include py core make definitions
include $(TOP)/py/py.mk

INC += -I.
INC += -Iinclude
INC += -I$(TC32_SDK)
INC += -I$(TOP)
INC += -I$(BUILD)

CFLAGS_TC32 = -fdata-sections -ffunction-sections -DMCU_STARTUP_5316
CFLAGS += $(INC) -Wall -Werror -std=c99 -nostdlib $(CFLAGS_TC32) $(COPT)
LDFLAGS += -nostdlib -T $(TC32_SDK)/boot.link -Map=$@.map --cref --gc-sections

CFLAGS += -Os -DNDEBUG -g

# Flags for optional C++ source code
CXXFLAGS += $(filter-out -std=c99,$(CFLAGS))

LIBS = $(TC32_SDK)/proj_lib/liblt_5316.a \
	$(shell $(CC) -print-libgcc-file-name)

SRC_C = \
	main.c \
	$(TC32_SDK)/drivers/5316/adc.c \
	$(TC32_SDK)/drivers/5316/analog.c \
	$(TC32_SDK)/drivers/5316/bsp.c \
	$(TC32_SDK)/drivers/5316/clock.c \
	$(TC32_SDK)/drivers/5316/gpio.c \
	$(TC32_SDK)/drivers/5316/uart.c \
	shared/libc/string0.c \
	shared/libc/printf.c \
	shared/readline/readline.c \
	shared/runtime/pyexec.c \
	shared/runtime/stdout_helpers.c \
	$(BUILD)/_frozen_mpy.c \

SRC_S = \
	$(TC32_SDK)/boot/5316/cstartup_5316.S

SRC_QSTR += shared/readline/readline.c shared/runtime/pyexec.c

OBJ += $(PY_CORE_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_S:.S=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_CXX:.cpp=.o))

all: $(BUILD)/firmware.bin

$(BUILD)/_frozen_mpy.c: $(TOP)/tests/frozen/frozentest.mpy $(BUILD)/genhdr/qstrdefs.generated.h
	$(ECHO) "MISC freezing bytecode"
	$(Q)$(TOP)/tools/mpy-tool.py -f -q $(BUILD)/genhdr/qstrdefs.preprocessed.h -mlongint-impl=none $< > $@

$(BUILD)/firmware.elf: $(BUILD)/$(TC32_SDK)/boot/5316/cstartup_5316.o $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(Q)$(SIZE) $@

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O binary $^ $@

include $(TOP)/py/mkrules.mk
