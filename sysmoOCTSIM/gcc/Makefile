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
	ccid \
	dma_m2m \
	gcc \
	gcc/gcc \
	hal/src \
	hal/utils/src \
	hpl/cmcc \
	hpl/core \
	hpl/dmac \
	hpl/gclk \
	hpl/mclk \
	hpl/osc32kctrl \
	hpl/oscctrl \
	hpl/pm \
	hpl/ramecc \
	hpl/sercom \
	hpl/usb \
	stdio_redirect \
	stdio_redirect/gcc \
	usb \
	usb/class/ccid/device \
	usb/class/cdc/device \
	usb/device \
	$(NULL)

INC_DIRS = \
	-I"../" \
	-I"../CMSIS/Core/Include" \
	-I"../ccid" \
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
	-I"../stdio_redirect" \
	-I"../usb" \
	-I"../usb/class/ccid" \
	-I"../usb/class/ccid/device" \
	-I"../usb/class/cdc" \
	-I"../usb/class/cdc/device" \
	-I"../usb/device" \
	$(NULL)

# List the object files
OBJS +=  \
	ccid/ccid_proto.o \
	ccid/ccid_device.o \
	atmel_start.o \
	command.o \
	dma_m2m/dma_memory.o \
	driver_init.o \
	gcc/gcc/startup_same54.o \
	gcc/system_same54.o \
	hal/src/hal_atomic.o \
	hal/src/hal_cache.o \
	hal/src/hal_delay.o \
	hal/src/hal_gpio.o \
	hal/src/hal_init.o \
	hal/src/hal_io.o \
	hal/src/hal_sleep.o \
	hal/src/hal_usart_async.o \
	hal/src/hal_usart_async_rings.o \
	hal/src/hal_usart_sync.o \
	hal/src/hal_usb_device.o \
	hal/utils/src/utils_assert.o \
	hal/utils/src/utils_event.o \
	hal/utils/src/utils_list.o \
	hal/utils/src/utils_ringbuffer.o \
	hal/utils/src/utils_syscalls.o \
	hpl/cmcc/hpl_cmcc.o \
	hpl/core/hpl_core_m4.o \
	hpl/core/hpl_init.o \
	hpl/dmac/hpl_dmac.o \
	hpl/gclk/hpl_gclk.o \
	hpl/mclk/hpl_mclk.o \
	hpl/osc32kctrl/hpl_osc32kctrl.o \
	hpl/oscctrl/hpl_oscctrl.o \
	hpl/pm/hpl_pm.o \
	hpl/ramecc/hpl_ramecc.o \
	hpl/sercom/hpl_sercom.o \
	hpl/usb/hpl_usb.o \
	i2c_bitbang.o \
	iso7816_3.o \
	libosmo_emb.o \
	main.o \
	manual_test.o \
	ncn8025.o \
	octsim_i2c.o \
	stdio_redirect/gcc/read.o \
	stdio_redirect/gcc/write.o \
	stdio_redirect/stdio_io.o \
	stdio_start.o \
	talloc.o \
	usb/class/ccid/device/ccid_df.o \
	usb/class/cdc/device/cdcdf_acm.o \
	usb/device/usbdc.o \
	usb/usb_protocol.o \
	usb_descriptors.o \
	usb_start.o \
	$(NULL)

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
