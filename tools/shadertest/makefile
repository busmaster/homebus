OBJS = $(OBJDIR)/main.o $(OBJDIR)/digout.o $(OBJDIR)/shader.o
BIN  = shadertest
ARCH = $(TARGET_ARCH)
OS = $(TARGET_OS)
OBJDIR = obj
BINDIR = bin
INCLUDES = -I . -I ../../include -I ../../include/linux -I ../../include/devices/common
CFLAGS = -g -c -Wall -DTIME_SIMULATION

ifeq ($(ARCH),i686)
	ifeq ($(OS),linux)
		GCC_PREFIX = i686-linux-gnu-
	endif
endif

GCC = $(GCC_PREFIX)gcc


all: $(OBJS)
	@mkdir -p $(BINDIR)
	$(GCC) $(OBJS) -lrt -o $(BINDIR)/$(BIN)

$(OBJDIR)/main.o: main.c
	@mkdir -p $(OBJDIR)
	$(GCC) $(INCLUDES) $(CFLAGS) -o $@ $<

$(OBJDIR)/digout.o: digout.c
	@mkdir -p $(OBJDIR)
	$(GCC) $(INCLUDES) $(CFLAGS) -o $@ $<

$(OBJDIR)/shader.o: ../../devices/common/shader.c
	@mkdir -p $(OBJDIR)
	$(GCC) $(INCLUDES) $(CFLAGS) -o $@ $<


.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)


