###############################################################################
# Makefile for the project cff3100if_bootloader
###############################################################################

## General Flags
PROJECT = cff3100if_bootloader
MCU = atmega168p
TARGET = cff3100if_bootloader.elf
CC = avr-gcc

CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -DF_CPU=1000000UL -O2 -fsigned-char -funsigned-bitfields -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=cff3100if_bootloader.map
LDFLAGS += -Wl,-section-start=.text=0x3800


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Include Directories
INCLUDES = -I . -I ../../../include/avr -I ../../../include

## Objects that must be built in order to link
OBJECTS = main.o applasm.o flash.o flashasm.o bus.o siotype1poll.o

## Objects explicitly added by the user
LINKONLYOBJECTS =

## Build
all: $(TARGET) cff3100if_bootloader.hex cff3100if_bootloader.eep size

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

siotype1poll.o: siotype1poll.c
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
	-rm -rf $(OBJECTS) cff3100if_bootloader.elf dep/* cff3100if_bootloader.hex cff3100if_bootloader.eep cff3100if_bootloader.map

## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

