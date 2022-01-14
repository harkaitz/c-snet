## Configuration
DESTDIR    =
PREFIX     =/usr/local
AR         =ar
CC         =gcc
CFLAGS     =-Wall -g
CPPFLAGS   =
LIBS       =
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS)
SCRIPTS    =
PROGRAMS   =

## Help string.
all:
help:
	@echo "all     : Build everything."
	@echo "clean   : Clean files."
	@echo "install : Install all produced files."

## Programs.
SCRIPTS  += snet-test
PROGRAMS += ./snet
./snet: snet.c
	$(CC) -o $@ $^ $(CFLAGS_ALL) $(LIBS)

## Blocks.
SCRIPTS  +=blocks/snet_asink_r blocks/snet_sigsrc_r

## install and clean.
all: $(PROGRAMS)
install: $(PROGRAMS) $(SCRIPTS)
	install -d                           $(DESTDIR)$(PREFIX)/bin
	install -m755 $(PROGRAMS) $(SCRIPTS) $(DESTDIR)$(PREFIX)/bin
	install -d                           $(DESTDIR)$(PREFIX)/share/snet/examples
	cp -r examples/*                     $(DESTDIR)$(PREFIX)/share/snet/examples
clean:
	rm -f $(PROGRAMS)
