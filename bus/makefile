OBJS = bus.o
BIN  = libbus.a
ARCH = $(TARGET_ARCH)
OS = $(TARGET_OS)
OBJDIR = obj
BINDIR = bin

INCLUDE_PATH = . ../include
ifeq ($(OS),win32)
INCLUDE_PATH += ../include/win32
else ifeq ($(OS),linux)
INCLUDE_PATH += ../include/linux
endif

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
AR = ar

INC_PATH=$(foreach d, $(INCLUDE_PATH), -I$d)

%.o : %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -c -Wall $(INC_PATH) $< -o $(OBJDIR)/$@

all: $(OBJS)
	@mkdir -p $(BINDIR)
	$(AR) rcs $(BINDIR)/$(BIN) $(OBJDIR)/$(OBJS)

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)


