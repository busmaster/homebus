OBJS = bus.o
BIN  = libbus.a
ARCH = $(TARGET_ARCH)
OS = $(TARGET_OS)
OBJDIR = obj
BINDIR = bin

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

%.o : %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -c -Wall -I . -I ../include -I ../include/linux $< -o $(OBJDIR)/$@

all: $(OBJS)
	@mkdir -p $(BINDIR)
	$(AR) rcs $(BINDIR)/$(BIN) $(OBJDIR)/$(OBJS)

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)


