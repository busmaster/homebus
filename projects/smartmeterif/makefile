#
#             LUFA Library
#     Copyright (C) Dean Camera, 2014.
#
#  dean [at] fourwalledcubicle [dot] com
#           www.lufa-lib.org
#
# --------------------------------------
#         LUFA Project Makefile.
# --------------------------------------

# Run "make help" for target help.

MCU          = at90usb647
ARCH         = AVR8
F_CPU        = 8000000
F_USB        = $(F_CPU)
OPTIMIZATION = 2
TARGET       = smartmeterif
SRC          = $(TARGET).c ConfigDescriptor.c ../../sio/avr/siosingle.c ../../bus/bus.c ../../devices/smartmeterif/led.c ../../aes/aes.c $(LUFA_SRC_USB) 
#$(LUFA_SRC_USBCLASS)
LUFA_PATH    = ../../lufa/lufa-LUFA-140928/LUFA
CC_FLAGS     = -DUSE_LUFA_CONFIG_HEADER -IConfig/ -I. -I../../include -I../../include/avr -I../../include/devices/smartmeterif -I../../include/devices/common  -I../../include/aes
LD_FLAGS     =

# Default target
all:

ifneq ($(MAKECMDGOALS),unzip_lufa)
# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_doxygen.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
include $(LUFA_PATH)/Build/lufa_hid.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
include $(LUFA_PATH)/Build/lufa_atprogram.mk
endif

.PHONY: unzip_lufa
unzip_lufa:
	unzip ../../lufa/lufa-LUFA-140928.zip -d ../../lufa/
