OBJS = sio.o
BIN  = libsio.a
ARCH = $(TARGET_ARCH)
OS = linux
OBJDIR = obj
BINDIR = bin

INCLUDE_PATH = . ../../include ../../include/linux

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


