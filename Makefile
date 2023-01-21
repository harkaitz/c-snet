DESTDIR    =
PREFIX     =/usr/local
AR         =ar
CC         =cc
CFLAGS     =-Wall -g
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS)
SCRIPTS    =blocks/snet_asink_r blocks/snet_sigsrc_r
PROGRAMS   =snet$(EXE)
## -- targets
all: $(PROGRAMS)
clean:
	rm -f $(PROGRAMS)
install: $(PROGRAMS) $(SCRIPTS)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m755 $(PROGRAMS) $(SCRIPTS) $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/snet/examples
	cp -r examples/* $(DESTDIR)$(PREFIX)/share/snet/examples
## -- programs
snet$(EXE): snet.c
	$(CC) -o $@ $^ $(CFLAGS_ALL) $(LIBS)
## -- license --
install: install-license
install-license: LICENSE
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/c-snet
	cp LICENSE $(DESTDIR)$(PREFIX)/share/doc/c-snet
## -- license --
