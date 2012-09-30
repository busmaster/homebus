OBJS = main.o
BIN  = addchecksum
ARCH = $(TARGET_ARCH)
OS = $(TARGET_OS)
GCC_VERSION = 4.6

ifeq ($(ARCH),i686)
	ifeq ($(OS),linux)
		GCC_PREFIX = i686-linux-gnu
	endif
else ifeq ($(ARCH), arm)
	ifeq ($(OS),linux)
		GCC_PREFIX = arm-linux-gnueabi
	endif
endif

ifeq ($(GCC_PREFIX),)
$(error "TARGET_OS and/or TARGET_ARCH not set properly")
endif

GCC = $(GCC_PREFIX)-gcc-$(GCC_VERSION)

all: $(OBJS)
	$(GCC) $(OBJS) -o bin/$(OS)/$(ARCH)/$(BIN)

%.o: %.c
	$(GCC) -g -c -Wall -I . -I ../../include -I ../../include/linux $<


.PHONY: clean
clean:
	rm -rf bin/$(OS)/$(ARCH)/$(BIN) $(OBJS)


