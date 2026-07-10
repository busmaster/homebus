###############################################################################
# Makefile for the project do8_bootloader
###############################################################################

## General Flags
PROJECT = do8_bootloader
MCU = atmega32u4
TARGET = do8_bootloader.elf
CC = avr-gcc

CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -DF_CPU=3686400UL -O2 -fsigned-char -funsigned-bitfields -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=do8_bootloader.map
LDFLAGS += -Wl,-section-start=.text=0x7000


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Include Directories
INCLUDES = -I . -I ../../../include/avr -I ../../../include -I ../../../include/devices/do8

## Objects that must be built in order to link
OBJECTS = main.o applasm.o flash.o flashasm.o bus.o sio.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) do8_bootloader.hex do8_bootloader.eep size

## Compile
main.o: main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

applasm.o: applasm.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

flash.o: flash.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

flashasm.o: flashasm.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

bus.o: bus.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

sio.o: sio.c
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
	@avr-size --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) do8_bootloader.elf dep/* do8_bootloader.hex do8_bootloader.eep do8_bootloader.map

## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

