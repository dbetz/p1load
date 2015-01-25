MKDIR=mkdir -p
RM=rm
CC=cc
ECHO=echo

OS?=macosx

ifeq ($(OS),linux)
CFLAGS+=-DLINUX
EXT=
OSINT=osint_linux
LIBS=
endif

ifeq ($(OS),msys)
CFLAGS += -DMINGW
EXT=.exe
OSINT=osint_mingw enumcom
LIBS=-lsetupapi
endif

ifeq ($(OS),macosx)
CFLAGS+=-DMACOSX
EXT=
OSINT=osint_linux
LIBS=
endif

ifeq ($(OS),)
$(error OS not set)
endif

SRCDIR=.
OBJDIR=obj/$(OS)
BINDIR=bin/$(OS)

TARGET=$(BINDIR)/p1load$(EXT)

HDRS=\
ploader.h

OBJS=\
$(OBJDIR)/p1load.o \
$(OBJDIR)/ploader.o \
$(foreach x, $(OSINT), $(OBJDIR)/$(x).o)

CFLAGS+=-Wall
LDFLAGS=$(CFLAGS)

.PHONY:	default
default:	$(TARGET)

DIRS=$(OBJDIR) $(BINDIR)

$(TARGET):	$(BINDIR) $(OBJDIR) $(OBJS)
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@$(ECHO) link $@

$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(HDRS) $(OBJDIR)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(ECHO) $(CC) $@

$(OBJDIR)/%.o:	$(OBJDIR)/%.c $(HDRS) $(OBJDIR)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(ECHO) $(CC) $@

.PHONY:	bin2c
bin2c:		$(BINDIR)/bin2c$(EXT)

$(BINDIR)/bin2c$(EXT):	$(SRCDIR)/tools/bin2c.c $(BINDIR)
	@$(CC) $(CFLAGS) $(LDFLAGS) $(SRCDIR)/tools/bin2c.c -o $@
	@$(ECHO) cc $@

$(DIRS):
	$(MKDIR) $@

.PHONY:	clean
clean:
	@$(RM) -f -r $(OBJDIR)
	@$(RM) -f -r $(PROPOBJDIR)
	@$(RM) -f -r $(BINDIR)

.PHONY:
clean-all:	clean
	@$(RM) -f -r obj
	@$(RM) -f -r bin
