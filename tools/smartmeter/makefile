OBJS = main.o
BIN  = smartmeter
ARCH = $(TARGET_ARCH)
OBJDIR = obj
BINDIR = bin

SYS = $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
OS = linux
else ifeq ($(SYS),mingw32)
OS = win32
endif

ifeq ($(OS),win32)
SUBDIRS = ../../aes ../../sio/win32
else ifeq ($(OS),linux)
SUBDIRS = ../../aes ../../sio/linux
endif

INCLUDE_PATH = . ../../include
ifeq ($(OS),win32)
INCLUDE_PATH += ../../include/aes ../../include/win32
else ifeq ($(OS),linux)
INCLUDE_PATH += ../../include/aes ../../include/linux
endif

ifeq ($(OS),win32)
LIBRARY_PATH = ../../aes/bin ../../sio/win32/bin
else ifeq ($(OS),linux)
LIBRARY_PATH = ../../aes/bin ../../sio/linux/bin
endif

LIBRARY = sio aes
ifeq ($(OS),linux)
LIBRARY += rt
endif

ifeq ($(ARCH),i686)
		GCC_PREFIX = i686-linux-gnu-
else ifeq ($(ARCH), arm)
		GCC_PREFIX = arm-linux-gnueabi-
else ifeq ($(ARCH), armhf)
		GCC_PREFIX = arm-linux-gnueabihf-
endif

GCC = $(GCC_PREFIX)gcc
INC_PATH=$(foreach d, $(INCLUDE_PATH), -I$d)
LIB_PATH=$(foreach d, $(LIBRARY_PATH), -L$d)
LIBS=$(foreach d, $(LIBRARY), -l$d)

.PHONY: all
all: $(OBJS)
	for d in $(SUBDIRS); do \
		(cd $$d; $(MAKE) all)  \
	done
	@mkdir -p $(BINDIR)
	$(GCC) $(OBJDIR)/$(OBJS) $(LIB_PATH) $(LIBS) -o $(BINDIR)/$(BIN)

%.o: %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -g -c -Wall $(INC_PATH) $< -o $(OBJDIR)/$@

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)
	for d in $(SUBDIRS); do \
		(cd $$d; $(MAKE) clean)  \
	done
