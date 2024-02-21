PROJECT    =c-snet
VERSION    =1.0.0
DESTDIR    =
PREFIX     =/usr/local
CC         =gcc -pedantic-errors -std=c99 -Wall
SCRIPTS    =blocks/snet_asink_r blocks/snet_sigsrc_r
PROGRAMS   =snet$(EXE)
##
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS)
all: $(PROGRAMS)
clean:
	rm -f $(PROGRAMS)
install: $(PROGRAMS) $(SCRIPTS)
	@install -d $(DESTDIR)$(PREFIX)/bin
	@install -d $(DESTDIR)$(PREFIX)/share/snet/examples
	install -m755 $(PROGRAMS) $(SCRIPTS) $(DESTDIR)$(PREFIX)/bin
	cp -r examples/* $(DESTDIR)$(PREFIX)/share/snet/examples
snet$(EXE): snet.c
	$(CC) -o $@ $^ $(CFLAGS_ALL) $(LIBS)
## -- BLOCK:license --
install: install-license
install-license: 
	@mkdir -p $(DESTDIR)$(PREFIX)/share/doc/$(PROJECT)
	cp LICENSE  $(DESTDIR)$(PREFIX)/share/doc/$(PROJECT)
## -- BLOCK:license --
