################################################################################
# Automatically-generated file. Do not edit!
################################################################################

EXTRA_LIBS=/usr/local/arm-none-eabi/lib/libosmocore.a
EXTRA_CFLAGS=-I/usr/local/arm-none-eabi/include -I../../ccid

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

OBJS_AS_ARGS +=  \
"hal/src/hal_io.o" \
"stdio_redirect/stdio_io.o" \
"stdio_redirect/gcc/write.o" \
"hpl/core/hpl_core_m4.o" \
"hal/src/hal_cache.o" \
"usb/class/cdc/device/cdcdf_acm.o" \
"usb/class/ccid/device/ccid_df.o" \
"hal/utils/src/utils_syscalls.o" \
"stdio_redirect/gcc/read.o" \
"gcc/system_same54.o" \
"hpl/usb/hpl_usb.o" \
"dma_m2m/dma_memory.o" \
"hal/src/hal_delay.o" \
"hpl/pm/hpl_pm.o" \
"hpl/core/hpl_init.o" \
"hpl/gclk/hpl_gclk.o" \
"hal/utils/src/utils_list.o" \
"hal/utils/src/utils_assert.o" \
"hpl/dmac/hpl_dmac.o" \
"hpl/oscctrl/hpl_oscctrl.o" \
"hal/src/hal_usart_sync.o" \
"hpl/mclk/hpl_mclk.o" \
"hpl/ramecc/hpl_ramecc.o" \
"stdio_start.o" \
"usb/usb_protocol.o" \
"hal/src/hal_init.o" \
"gcc/gcc/startup_same54.o" \
"hal/src/hal_usb_device.o" \
"main.o" \
"manual_test.o" \
"talloc.o" \
"usb_descriptors.o" \
"i2c_bitbang.o" \
"octsim_i2c.o" \
"ncn8025.o" \
"command.o" \
"iso7816_3.o" \
"hpl/osc32kctrl/hpl_osc32kctrl.o" \
"driver_init.o" \
"hal/src/hal_usart_async.o" \
"hal/src/hal_usart_async_rings.o" \
"hpl/sercom/hpl_sercom.o" \
"hal/utils/src/utils_ringbuffer.o" \
"hal/src/hal_gpio.o" \
"hal/utils/src/utils_event.o" \
"hal/src/hal_sleep.o" \
"usb_start.o" \
"hpl/cmcc/hpl_cmcc.o" \
"atmel_start.o" \
"usb/device/usbdc.o" \
"hal/src/hal_atomic.o"

# List the dependency files
DEPS := $(OBJS:%.o=%.d)

DEPS_AS_ARGS +=  \
"dma_m2m/dma_memory.d" \
"stdio_redirect/stdio_io.d" \
"hal/utils/src/utils_event.d" \
"hal/src/hal_io.d" \
"hpl/ramecc/hpl_ramecc.d" \
"hpl/core/hpl_core_m4.d" \
"hal/utils/src/utils_syscalls.d" \
"usb/class/cdc/device/cdcdf_acm.d" \
"stdio_redirect/gcc/write.d" \
"gcc/gcc/startup_same54.d" \
"hpl/usb/hpl_usb.d" \
"hal/utils/src/utils_list.d" \
"hpl/cmcc/hpl_cmcc.d" \
"hpl/dmac/hpl_dmac.d" \
"hal/utils/src/utils_assert.d" \
"hal/src/hal_delay.d" \
"hpl/core/hpl_init.d" \
"hpl/pm/hpl_pm.d" \
"usb/usb_protocol.d" \
"stdio_start.d" \
"hpl/gclk/hpl_gclk.d" \
"hal/src/hal_usb_device.d" \
"usb_start.d" \
"hal/src/hal_init.d" \
"hal/src/hal_usart_sync.d" \
"hpl/mclk/hpl_mclk.d" \
"driver_init.d" \
"stdio_redirect/gcc/read.d" \
"hal/src/hal_usart_async.d" \
"hal/src/hal_usart_async_rings.d" \
"hpl/osc32kctrl/hpl_osc32kctrl.d" \
"main.d" \
"manual_test.d" \
"talloc.d" \
"usb_descriptors.d" \
"i2c_bitbang.d" \
"octsim_i2c.d" \
"ncn8025.d" \
"command.d" \
"iso7816_3.d" \
"hal/src/halcache.d" \
"hal/src/hal_sleep.d" \
"hal/utils/src/utils_ringbuffer.d" \
"hpl/sercom/hpl_sercom.d" \
"hal/src/hal_gpio.d" \
"hal/src/hal_atomic.d" \
"usb/device/usbdc.d" \
"hpl/oscctrl/hpl_oscctrl.d" \
"gcc/system_same54.d" \
"atmel_start.d"

OUTPUT_FILE_NAME :=AtmelStart
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
	$(QUOTE)arm-none-eabi-gcc$(QUOTE) -o $(OUTPUT_FILE_NAME).elf $(OBJS_AS_ARGS) $(EXTRA_LIBS) -Wl,--start-group -lm -Wl,--end-group -mthumb \
-Wl,-Map="$(OUTPUT_FILE_NAME).map" --specs=nano.specs -Wl,--gc-sections -mcpu=cortex-m4 \
 \
-T"../gcc/gcc/same54n19a_dfu.ld" \
-L"../gcc/gcc"
	@echo Finished building target: $@

	"arm-none-eabi-objcopy" -O binary "$(OUTPUT_FILE_NAME).elf" "$(OUTPUT_FILE_NAME).bin"
	"arm-none-eabi-objcopy" -O ihex -R .eeprom -R .fuse -R .lock -R .signature  \
        "$(OUTPUT_FILE_NAME).elf" "$(OUTPUT_FILE_NAME).hex"
	"arm-none-eabi-objcopy" -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma \
        .eeprom=0 --no-change-warnings -O binary "$(OUTPUT_FILE_NAME).elf" \
        "$(OUTPUT_FILE_NAME).eep" || exit 0
	"arm-none-eabi-objdump" -h -S "$(OUTPUT_FILE_NAME).elf" > "$(OUTPUT_FILE_NAME).lss"
	"arm-none-eabi-size" "$(OUTPUT_FILE_NAME).elf"

	

# Compiler targets




%.o: %.c
	@echo Building file: $<
	@echo ARM/GNU C Compiler
	$(QUOTE)arm-none-eabi-gcc$(QUOTE) -x c -mthumb -DDEBUG -Os -ffunction-sections -fdata-sections -mlong-calls -g3 -Wall -c -std=gnu99 \
-D__SAME54N19A__ -mcpu=cortex-m4 -mfloat-abi=softfp -mfpu=fpv4-sp-d16 \
-I"../" -I"../config" -I"../hal/include" -I"../hal/utils/include" -I"../hpl/cmcc" -I"../hpl/core" -I"../hpl/dmac" -I"../hpl/gclk" -I"../hpl/mclk" -I"../hpl/osc32kctrl" -I"../hpl/oscctrl" -I"../hpl/pm" -I"../hpl/port" -I"../hpl/ramecc" -I"../hpl/sercom" -I"../hpl/usb" -I"../hri" -I"../" -I"../config" -I"../usb" -I"../usb/class/cdc" -I"../usb/class/cdc/device" -I"../usb/class/ccid" -I"../usb/class/ccid/device" -I"../usb/device" -I"../" -I"../config" -I"../stdio_redirect" -I"../" -I"../dma_m2m" -I"../" -I"../CMSIS/Core/Include" -I"../include" $(EXTRA_CFLAGS) \
-MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"  -o "$@" "$<"
	@echo Finished building: $<

%.o: %.s
	@echo Building file: $<
	@echo ARM/GNU Assembler
	$(QUOTE)arm-none-eabi-as$(QUOTE) -x c -mthumb -DDEBUG -Os -ffunction-sections -fdata-sections -mlong-calls -g3 -Wall -c -std=gnu99 \
-D__SAME54N19A__ -mcpu=cortex-m4 -mfloat-abi=softfp -mfpu=fpv4-sp-d16 \
-I"../" -I"../config" -I"../hal/include" -I"../hal/utils/include" -I"../hpl/cmcc" -I"../hpl/core" -I"../hpl/dmac" -I"../hpl/gclk" -I"../hpl/mclk" -I"../hpl/osc32kctrl" -I"../hpl/oscctrl" -I"../hpl/pm" -I"../hpl/port" -I"../hpl/ramecc" -I"../hpl/sercom" -I"../hpl/usb" -I"../hri" -I"../" -I"../config" -I"../usb" -I"../usb/class/cdc" -I"../usb/class/cdc/device" -I"../usb/class/ccid" -I"../usb/class/ccid/device" -I"../usb/device" -I"../" -I"../config" -I"../stdio_redirect" -I"../" -I"../dma_m2m" -I"../" -I"../CMSIS/Core/Include" -I"../include"  \
-MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"  -o "$@" "$<"
	@echo Finished building: $<

%.o: %.S
	@echo Building file: $<
	@echo ARM/GNU Preprocessing Assembler
	$(QUOTE)arm-none-eabi-gcc$(QUOTE) -x c -mthumb -DDEBUG -Os -ffunction-sections -fdata-sections -mlong-calls -g3 -Wall -c -std=gnu99 \
-D__SAME54N19A__ -mcpu=cortex-m4 -mfloat-abi=softfp -mfpu=fpv4-sp-d16 \
-I"../" -I"../config" -I"../hal/include" -I"../hal/utils/include" -I"../hpl/cmcc" -I"../hpl/core" -I"../hpl/dmac" -I"../hpl/gclk" -I"../hpl/mclk" -I"../hpl/osc32kctrl" -I"../hpl/oscctrl" -I"../hpl/pm" -I"../hpl/port" -I"../hpl/ramecc" -I"../hpl/sercom" -I"../hpl/usb" -I"../hri" -I"../" -I"../config" -I"../usb" -I"../usb/class/cdc" -I"../usb/class/cdc/device" -I"../usb/class/ccid" -I"../usb/class/ccid/device" -I"../usb/device" -I"../" -I"../config" -I"../stdio_redirect" -I"../" -I"../dma_m2m" -I"../" -I"../CMSIS/Core/Include" -I"../include"  \
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
	rm -f $(OBJS_AS_ARGS)
	rm -f $(OUTPUT_FILE_PATH)
	rm -f $(DEPS_AS_ARGS)
	rm -f $(OUTPUT_FILE_NAME).a $(OUTPUT_FILE_NAME).hex $(OUTPUT_FILE_NAME).bin \
        $(OUTPUT_FILE_NAME).lss $(OUTPUT_FILE_NAME).eep $(OUTPUT_FILE_NAME).map \
        $(OUTPUT_FILE_NAME).srec
