###############################################################################
# Makefile for the project klaus241
###############################################################################

## General Flags
PROJECT = klaus241
MCU = atmega128
TARGET = klaus241.elf
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
LDFLAGS +=  -Wl,-Map=klaus241.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Include Directories
INCLUDES = -I . -I ../../include -I ../../include/avr -I ../../include/devices/do31 -I ../../include/devices/common

## Objects that must be built in order to link
OBJECTS = application.o bus.o main.o button.o digout.o shader.o led.o siotype2.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) kaus241.hex klaus241.eep size

## Compile
application.o: application.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

bus.o: ../../bus/bus.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

main.o: ../../devices/do31/main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

button.o: ../../devices/common/button.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

digout.o: ../../devices/do31/digout.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

shader.o: ../../devices/common/shader.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

led.o: ../../devices/do31/led.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

siotype2.o: ../../sio/avr/siotype2.c
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
	-rm -rf $(OBJECTS) klaus241.elf dep/* klaus241.hex klaus241.eep klaus241.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

