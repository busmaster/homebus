###############################################################################
# Makefile for the project kurt240
###############################################################################

## General Flags
PROJECT = kurt240
MCU = atmega128
TARGET = kurt240.elf
CC = avr-gcc

CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -DF_CPU=1000000UL -O2 -fsigned-char -funsigned-bitfields -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d -DBUSVAR -DBUSVAR_MEMSIZE=256 -DBUSVAR_NUMVAR=32

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=kurt240.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Include Directories
INCLUDES = -I . -I ../../include -I ../../include/avr -I ../../include/devices/do31 -I ../../include/devices/common

## Objects that must be built in order to link
OBJECTS = application.o bus.o busvar.o main.o button.o digout.o shader.o led.o siosingle.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) kurt240.hex kurt240.eep size

## Compile
application.o: application.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

bus.o: ../../bus/bus.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

busvar.o: ../../bus/busvar.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

main.o: ../../devices/do31/main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

digout.o: ../../devices/do31/digout.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

button.o: ../../devices/common/button.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

shader.o: ../../devices/common/shader.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

led.o: ../../devices/do31/led.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

siosingle.o: ../../sio/avr/siosingle.c
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
	-rm -rf $(OBJECTS) kurt240.elf dep/* kurt240.hex kurt240.eep kurt240.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

