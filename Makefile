MKDIR=mkdir -p
RM=rm
CC=cc
ECHO=echo

CFLAGS=-Wall

OBJS=\
$(OBJDIR)/p1load.o \
$(OBJDIR)/port.o \
$(OBJDIR)/ploader.o \
$(OBJDIR)/packet.o

EEPROM_OBJS=\
$(OBJDIR)/eeprom.o \
$(OBJDIR)/port.o \
$(OBJDIR)/ploader.o \
$(OBJDIR)/packet.o

OS?=macosx

VERSION := $(shell git describe --tags --long 2>/dev/null)
ifeq ($(VERSION),)
	VERSION := '0.0.0'
endif

CFLAGS += -D'VERSION="$(VERSION)"'

ifeq ($(OS),linux)
CFLAGS+=-DLINUX
EXT=
OSINT=osint_linux
LIBS=
endif

ifeq ($(OS),raspberrypi)
OS=linux
CFLAGS+=-DLINUX -DRASPBERRY_PI
EXT=
OSINT=osint_linux.o
LIBS=
OSINT+=gpio_sysfs.o
endif

ifeq ($(OS),msys)
CFLAGS+=-DMINGW
EXT=.exe
OSINT=osint_mingw.o enumcom.o
LIBS=-lsetupapi
endif

ifeq ($(OS),macosx)
CFLAGS+=-DMACOSX
EXT=
OSINT=osint_linux.o
LIBS=
endif

ifeq ($(OS),)
$(error OS not set)
endif

SRCDIR=.
OBJDIR=obj/$(OS)
BINDIR=bin/$(OS)

TARGET=$(BINDIR)/p1load$(EXT)

EEPROM_TARGET=$(BINDIR)/eeprom$(EXT)

HDRS=\
ploader.h

OBJS+=$(foreach x, $(OSINT), $(OBJDIR)/$(x))
EEPROM_OBJS+=$(foreach x, $(OSINT), $(OBJDIR)/$(x))

CFLAGS+=-Wall
LDFLAGS=$(CFLAGS)

.PHONY:	default
default:	$(TARGET) $(EEPROM_TARGET)

DIRS=$(OBJDIR) $(BINDIR)

$(TARGET):	$(BINDIR) $(OBJDIR) $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(EEPROM_TARGET):	$(BINDIR) $(OBJDIR) $(EEPROM_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(EEPROM_OBJS) $(LIBS)

$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(HDRS) $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DIRS):
	$(MKDIR) $@

.PHONY:	clean
clean:
	$(RM) -f -r $(OBJDIR)
	$(RM) -f -r $(BINDIR)

.PHONY:	clean-all
clean-all:
	$(RM) -f -r obj
	$(RM) -f -r bin
