###############################################################################
# Makefile for the project switchv1_bootloader
###############################################################################

## General Flags
PROJECT = switchv1_bootloader
MCU = atmega8
TARGET = switchv1_bootloader.elf
CC = avr-gcc

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -DF_CPU=1000000UL -Os -fsigned-char
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=switchv1_bootloader.map
LDFLAGS += -Wl,-section-start=.version=0x1ff0
LDFLAGS += -Wl,-section-start=.text=0x1800


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Include Directories
INCLUDES = -I . -I ../../include/avr -I ../../include

## Objects that must be built in order to link
OBJECTS = main.o applasm.o flash.o flashasm.o bus.o siotype0poll.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) switchv1_bootloader.hex switchv1_bootloader.eep size

## Compile
applasm.o: applasm.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

flashasm.o: flashasm.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

main.o: main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

flash.o: flash.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

bus.o: bus.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

siotype0poll.o: siotype0poll.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) switchv1_bootloader.elf dep/* switchv1_bootloader.hex switchv1_bootloader.eep switchv1_bootloader.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

