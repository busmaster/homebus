
OBJS = sio.o
BIN  = libsio.a
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
AR = ar

%.o : %.c
	$(GCC) -c -Wall -I . -I ../../include -I ../../include/linux $<

all: $(OBJS)
	$(AR) rcs $(BIN) $(OBJS)

.PHONY: clean
clean:
	rm -rf $(BIN) $(OBJS)


