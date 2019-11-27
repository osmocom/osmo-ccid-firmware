################################################################################
# Manually edited. Do not overwrite with Automatically-generated file.
################################################################################

SYSTEM_PREFIX?=/usr/local/arm-none-eabi
EXTRA_LIBS=$(SYSTEM_PREFIX)/lib/libosmocore.a
EXTRA_CFLAGS=-I$(SYSTEM_PREFIX)/include -I../../ccid_common
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

INC_DIRS = \
	-I"../" \
	-I"../CMSIS/Core/Include" \
	-I"../ccid_common" \
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
	-I"../hpl/rtc" \
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
	atmel_start.o \
	ccid_common/ccid_proto.o \
	ccid_common/ccid_device.o \
	ccid_common/iso7816_fsm.o \
	ccid_common/iso7816_3.o \
	ccid_common/cuart.o \
	ccid_common/ccid_slot_fsm.o \
	cuart_driver_asf4_usart_async.o \
	command.o \
	dma_m2m/dma_memory.o \
	driver_init.o \
	gcc/gcc/startup_same54.o \
	gcc/system_same54.o \
	hal/src/hal_atomic.o \
	hal/src/hal_cache.o \
	hal/src/hal_calendar.o \
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
	hpl/rtc/hpl_rtc.o \
	hpl/sercom/hpl_sercom.o \
	hpl/usb/hpl_usb.o \
	i2c_bitbang.o \
	libosmo_emb.o \
	main.o \
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
# List the subdirectories for creating object files
SUB_DIRS := $(sort $(foreach obj,$(OBJS),$(dir $(obj))))

OUTPUT_FILE_NAME := sysmoOCTSIM-$(GIT_VERSION)
QUOTE := "
OUTPUT_FILE_PATH +=$(OUTPUT_FILE_NAME).elf
OUTPUT_FILE_PATH_AS_ARGS +=$(OUTPUT_FILE_NAME).elf

vpath %.c ../
vpath %.s ../
vpath %.S ../

# All Target
all: $(SUB_DIRS) elf bin ihex eep lss

elf: $(OUTPUT_FILE_NAME).elf sysmoOCTSIM.elf
bin: $(OUTPUT_FILE_NAME).bin sysmoOCTSIM.bin
ihex: $(OUTPUT_FILE_NAME).ihex
eep: $(OUTPUT_FILE_NAME).eep
lss: $(OUTPUT_FILE_NAME).lss

# Linker target

$(OUTPUT_FILE_PATH): $(OBJS)
	@echo Building target: $@
	@echo Invoking: ARM/GNU Linker
	$(CC) -o $(OUTPUT_FILE_NAME).elf $(OBJS) $(EXTRA_LIBS) -Wl,--start-group -lm -Wl,--end-group -mthumb \
-Wl,-Map="$(OUTPUT_FILE_NAME).map" --specs=nano.specs -Wl,--gc-sections -Wl,--print-memory-usage -mcpu=cortex-m4 \
 \
-T"../gcc/gcc/same54n19a_dfu.ld" \
-L"../gcc/gcc"
	@echo Finished building target: $@

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.ihex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@

%.eep: %.elf
	$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma \
		.eeprom=0 --no-change-warnings -O binary $< $@ || exit 0

%.lss: %.elf
	$(OBJDUMP) -h -S $< > $@
	$(SIZE) "$(OUTPUT_FILE_NAME).elf"

sysmoOCTSIM.elf: $(OUTPUT_FILE_PATH)
	ln -sf $(OUTPUT_FILE_NAME).elf $@

sysmoOCTSIM.bin: $(OUTPUT_FILE_PATH)
	ln -sf $(OUTPUT_FILE_NAME).bin $@

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

mrproper: clean
	rm -f *.o *.d *.a *.elf *.bin *.ihex *.eep *.lss *.map *.srec
