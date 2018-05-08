OBJS = bus.o busvar.o
BIN  = libbus.a
ARCH = $(TARGET_ARCH)
OBJDIR = obj
BINDIR = bin

ifndef BUSVAR_MEMSIZE
BUSVAR_MEMSIZE = 256
endif
ifndef BUSVAR_NUMVAR
BUSVAR_NUMVAR = 32
endif
CFLAGS=-g -c -Wall -DBUSVAR -DBUSVAR_MEMSIZE=$(BUSVAR_MEMSIZE) -DBUSVAR_NUMVAR=$(BUSVAR_NUMVAR)

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

INC_PATH = $(foreach d,$(INCLUDE_PATH),-I$d)
OBJ_LIST = $(foreach o,$(OBJS),$(OBJDIR)/$o)

%.o : %.c
	@mkdir -p $(OBJDIR)
	$(GCC) $(CFLAGS) $(INC_PATH) $< -o $(OBJDIR)/$@

.PHONY: all
all: $(OBJS)
	@mkdir -p $(BINDIR)
	$(AR) rcs $(BINDIR)/$(BIN) $(OBJ_LIST)

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)


