OBJS = bus.o
BIN  = libbus.a
ARCH = $(TARGET_ARCH)
OBJDIR = obj
BINDIR = bin

SYS = $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
OS = linux
else ifeq ($(SYS),mingw32)
OS = win32
endif

INCLUDE_PATH = . ../include
ifeq ($(OS),win32)
INCLUDE_PATH += ../include/win32
else ifeq ($(OS),linux)
INCLUDE_PATH += ../include/linux
endif

ifeq ($(ARCH),i686)
	GCC_PREFIX = i686-linux-gnu-
else ifeq ($(ARCH), arm)
	GCC_PREFIX = arm-linux-gnueabi-
else ifeq ($(ARCH), armhf)
	GCC_PREFIX = arm-linux-gnueabihf-
endif

GCC = $(GCC_PREFIX)gcc
AR = ar

INC_PATH=$(foreach d, $(INCLUDE_PATH), -I$d)

%.o : %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -c -Wall $(INC_PATH) $< -o $(OBJDIR)/$@

.PHONY: all
all: $(OBJS)
	@mkdir -p $(BINDIR)
	$(AR) rcs $(BINDIR)/$(BIN) $(OBJDIR)/$(OBJS)

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)


