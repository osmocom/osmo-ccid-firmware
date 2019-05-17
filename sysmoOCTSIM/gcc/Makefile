################################################################################
# Manually edited. Do not overwrite with Automatically-generated file.
################################################################################

EXTRA_LIBS=/usr/local/arm-none-eabi/lib/libosmocore.a
EXTRA_CFLAGS=-I/usr/local/arm-none-eabi/include -I../../ccid
CROSS_COMPILE= arm-none-eabi-

CFLAGS_CPU=-D__SAME54N19A__ -mcpu=cortex-m4 -mfloat-abi=softfp -mfpu=fpv4-sp-d16
CFLAGS=-x c -mthumb -DDEBUG -Os -ffunction-sections -fdata-sections -mlong-calls \
       -g3 -Wall -c -std=gnu99 $(CFLAGS_CPU)

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
SIZE = $(CROSS_COMPILE)size
STRIP = $(CROSS_COMPILE)strip
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE = $(CROSS_COMPILE)size
GDB = $(CROSS_COMPILE)gdb
NM = $(CROSS_COMPILE)nm

TOP=../..
GIT_VERSION=$(shell $(TOP)/git-version-gen $(TOP)/.tarvers)
CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"

ifdef SystemRoot
	SHELL = cmd.exe
	MK_DIR = mkdir
else
	ifeq ($(shell uname), Linux)
		MK_DIR = mkdir -p
	endif

	ifeq ($(shell uname | cut -d _ -f 1), CYGWIN)
		MK_DIR = mkdir -p
	endif

	ifeq ($(shell uname | cut -d _ -f 1), MINGW32)
		MK_DIR = mkdir -p
	endif

	ifeq ($(shell uname | cut -d _ -f 1), MINGW64)
		MK_DIR = mkdir -p
	endif
endif

# List the subdirectories for creating object files
SUB_DIRS +=  \
 \
hal/src \
hpl/ramecc \
hpl/oscctrl \
stdio_redirect/gcc \
gcc \
hpl/osc32kctrl \
usb \
hpl/dmac \
usb/class/cdc/device \
usb/class/ccid/device \
stdio_redirect \
hal/utils/src \
hpl/usb \
dma_m2m \
hpl/pm \
hpl/cmcc \
hpl/gclk \
gcc/gcc \
hpl/mclk \
usb/device \
hpl/sercom \
hpl/core

INC_DIRS = \
	-I"../" \
	-I"../CMSIS/Core/Include" \
	-I"../config" \
	-I"../dma_m2m" \
	-I"../hal/include" \
	-I"../hal/utils/include" \
	-I"../hpl/cmcc" \
	-I"../hpl/core" \
	-I"../hpl/dmac" \
	-I"../hpl/gclk" \
	-I"../hpl/mclk" \
	-I"../hpl/osc32kctrl" \
	-I"../hpl/oscctrl" \
	-I"../hpl/pm" \
	-I"../hpl/port" \
	-I"../hpl/ramecc" \
	-I"../hpl/sercom" \
	-I"../hpl/usb" \
	-I"../hri" \
	-I"../include" \
	-I"../usb" \
	-I"../usb/class/cdc" \
	-I"../usb/class/cdc/device" \
	-I"../usb/class/ccid" \
	-I"../usb/class/ccid/device" \
	-I"../usb/device" \
	-I"../stdio_redirect" \
	$(NULL)

# List the object files
OBJS +=  \
hal/src/hal_io.o \
stdio_redirect/stdio_io.o \
stdio_redirect/gcc/write.o \
hpl/core/hpl_core_m4.o \
hal/src/hal_cache.o \
usb/class/cdc/device/cdcdf_acm.o \
usb/class/ccid/device/ccid_df.o \
hal/utils/src/utils_syscalls.o \
stdio_redirect/gcc/read.o \
gcc/system_same54.o \
hpl/usb/hpl_usb.o \
dma_m2m/dma_memory.o \
hal/src/hal_delay.o \
hpl/pm/hpl_pm.o \
hpl/core/hpl_init.o \
hpl/gclk/hpl_gclk.o \
hal/utils/src/utils_list.o \
hal/utils/src/utils_assert.o \
hpl/dmac/hpl_dmac.o \
hpl/oscctrl/hpl_oscctrl.o \
hal/src/hal_usart_sync.o \
hpl/mclk/hpl_mclk.o \
hpl/ramecc/hpl_ramecc.o \
stdio_start.o \
usb/usb_protocol.o \
hal/src/hal_init.o \
gcc/gcc/startup_same54.o \
hal/src/hal_usb_device.o \
main.o \
libosmo_emb.o \
manual_test.o \
talloc.o \
usb_descriptors.o \
i2c_bitbang.o \
octsim_i2c.o \
ncn8025.o \
command.o \
iso7816_3.o \
hpl/osc32kctrl/hpl_osc32kctrl.o \
driver_init.o \
hal/src/hal_usart_async.o \
hal/src/hal_usart_async_rings.o \
hpl/sercom/hpl_sercom.o \
hal/utils/src/utils_ringbuffer.o \
hal/src/hal_gpio.o \
hal/utils/src/utils_event.o \
hal/src/hal_sleep.o \
usb_start.o \
hpl/cmcc/hpl_cmcc.o \
atmel_start.o \
usb/device/usbdc.o \
hal/src/hal_atomic.o

# List the dependency files
DEPS := $(OBJS:%.o=%.d)

OUTPUT_FILE_NAME := sysmoOCTSIM
QUOTE := "
OUTPUT_FILE_PATH +=$(OUTPUT_FILE_NAME).elf
OUTPUT_FILE_PATH_AS_ARGS +=$(OUTPUT_FILE_NAME).elf

vpath %.c ../
vpath %.s ../
vpath %.S ../

# All Target
all: $(SUB_DIRS) $(OUTPUT_FILE_PATH)

# Linker target

$(OUTPUT_FILE_PATH): $(OBJS)
	@echo Building target: $@
	@echo Invoking: ARM/GNU Linker
	$(CC) -o $(OUTPUT_FILE_NAME).elf $(OBJS) $(EXTRA_LIBS) -Wl,--start-group -lm -Wl,--end-group -mthumb \
-Wl,-Map="$(OUTPUT_FILE_NAME).map" --specs=nano.specs -Wl,--gc-sections -mcpu=cortex-m4 \
 \
-T"../gcc/gcc/same54n19a_dfu.ld" \
-L"../gcc/gcc"
	@echo Finished building target: $@

	$(OBJCOPY) -O binary "$(OUTPUT_FILE_NAME).elf" "$(OUTPUT_FILE_NAME).bin"
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature  \
		"$(OUTPUT_FILE_NAME).elf" "$(OUTPUT_FILE_NAME).hex"
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma \
		.eeprom=0 --no-change-warnings -O binary "$(OUTPUT_FILE_NAME).elf" \
		"$(OUTPUT_FILE_NAME).eep" || exit 0
	$(OBJDUMP) -h -S "$(OUTPUT_FILE_NAME).elf" > "$(OUTPUT_FILE_NAME).lss"
	$(SIZE) "$(OUTPUT_FILE_NAME).elf"


# Compiler targets


%.o: %.c
	@echo Building file: $<
	@echo ARM/GNU C Compiler
	$(CC) $(CFLAGS) $(INC_DIRS) $(EXTRA_CFLAGS) \
-MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"  -o "$@" "$<"
	@echo Finished building: $<

%.o: %.s
	@echo Building file: $<
	@echo ARM/GNU Assembler
	$(CC) $(CFLAGS) $(INC_DIRS) \
-MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"  -o "$@" "$<"
	@echo Finished building: $<

%.o: %.S
	@echo Building file: $<
	@echo ARM/GNU Preprocessing Assembler
	$(CC) $(CFLAGS) $(INC_DIRS) \
-MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"  -o "$@" "$<"
	@echo Finished building: $<

# Detect changes in the dependent files and recompile the respective object files.
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

$(SUB_DIRS):
	$(MK_DIR) "$@"

clean:
	rm -f $(OBJS)
	rm -f $(OUTPUT_FILE_PATH)
	rm -f $(DEPS)
	rm -f $(OUTPUT_FILE_NAME).a $(OUTPUT_FILE_NAME).hex $(OUTPUT_FILE_NAME).bin \
        $(OUTPUT_FILE_NAME).lss $(OUTPUT_FILE_NAME).eep $(OUTPUT_FILE_NAME).map \
        $(OUTPUT_FILE_NAME).srec
