OBJS = main.o
BIN  = buttongpio
ARCH = $(TARGET_ARCH)
OBJDIR = obj
BINDIR = bin

SYS = $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
OS = linux
else ifeq ($(SYS),mingw32)
OS = win32
endif

SUBDIRS = ../../bus
ifeq ($(OS),win32)
SUBDIRS += ../../sio/win32
else ifeq ($(OS),linux)
SUBDIRS += ../../sio/linux
endif

INCLUDE_PATH = . ../../include
ifeq ($(OS),win32)
INCLUDE_PATH += ../../include/win32
else ifeq ($(OS),linux)
INCLUDE_PATH += ../../include/linux
endif

LIBRARY_PATH = ../../bus/bin
ifeq ($(OS),win32)
LIBRARY_PATH += ../../sio/win32/bin
else ifeq ($(OS),linux)
LIBRARY_PATH += ../../sio/linux/bin
endif

LIBRARY = sio bus
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
	$(GCC) $(OBJDIR)/$(OBJS) $(LIB_PATH) $(LIBS) -o $(BINDIR)/$(BIN) -Wall

%.o: %.c
	@mkdir -p $(OBJDIR)
	$(GCC) -g -c -Wall $(INC_PATH) $< -o $(OBJDIR)/$@

.PHONY: clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)
	for d in $(SUBDIRS); do \
		(cd $$d; $(MAKE) clean) \
	done


