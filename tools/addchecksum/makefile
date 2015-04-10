OBJS = main.o
BIN  = addchecksum
ARCH = $(TARGET_ARCH)
OBJDIR = obj
BINDIR = bin

SYS = $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
OS = linux
else ifeq ($(SYS),mingw32)
OS = win32
endif

ifeq ($(ARCH),i686)
	GCC_PREFIX = i686-linux-gnu-
else ifeq ($(ARCH), arm)
	GCC_PREFIX = arm-linux-gnueabi-
else ifeq ($(ARCH), armhf)
	GCC_PREFIX = arm-linux-gnueabihf-
endif

GCC = $(GCC_PREFIX)gcc

all:	$(OBJS)
	@mkdir -p $(BINDIR)
	$(GCC) $(OBJDIR)/$(OBJS) -o $(BINDIR)/$(BIN)

%.o: %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -g -c -Wall -I . -I ../../include $< -o $(OBJDIR)/$@

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)


