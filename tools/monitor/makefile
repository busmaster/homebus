SUBDIRS = ../../sio/linux ../../bus
OBJS = main.o
BIN  = monitor
ARCH = $(TARGET_ARCH)
OS = $(TARGET_OS)
GCC_VERSION = 4.6
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

GCC = $(GCC_PREFIX)gcc-$(GCC_VERSION)

all: $(OBJS)
	for d in $(SUBDIRS); do \
		(cd $$d; make all)  \
	done
	@mkdir -p $(BINDIR)
	$(GCC) $(OBJDIR)/$(OBJS) -L ../../sio/linux/bin -L ../../bus/bin -lsio -lbus -lrt -o $(BINDIR)/$(BIN)

%.o: %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -g -c -Wall -I . -I ../../include -I ../../include/linux $< -o $(OBJDIR)/$@


.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)
	for d in $(SUBDIRS); do \
		(cd $$d; make clean)  \
	done


