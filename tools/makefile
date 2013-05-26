SUBDIRS = addchecksum firmwareupdate modulservice monitor shadertest shuttercontrol
ARCH = $(TARGET_ARCH)
OS = $(TARGET_OS)

ifeq ($(ARCH),i686)
	ifeq ($(OS),linux)
		GCC_PREFIX = i686-linux-gnu-
	endif
else ifeq ($(ARCH), arm)
	ifeq ($(OS),linux)
		GCC_PREFIX = arm-linux-gnueabi-
	endif
endif

GCC = $(GCC_PREFIX)gcc

all:
	for d in $(SUBDIRS); do \
		(cd $$d; make all)  \
	done

.PHONY: clean
clean:
	for d in $(SUBDIRS); do \
		(cd $$d; make clean)  \
	done


