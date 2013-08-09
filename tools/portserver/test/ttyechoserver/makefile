OBJS = main.o
BIN  = ttyechoserver
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

all: $(OBJS)
	@mkdir -p $(BINDIR)
	$(GCC) $(OBJDIR)/$(OBJS) -lrt -o $(BINDIR)/$(BIN)

%.o: %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -g -c -Wall -std=c99 -I . $< -o $(OBJDIR)/$@


.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)


