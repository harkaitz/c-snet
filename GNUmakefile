PROJECT    =c-snet
VERSION    =1.0.0
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
## -- BLOCK:license --
install: install-license
install-license: 
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/$(PROJECT)
	cp LICENSE README.md $(DESTDIR)$(PREFIX)/share/doc/$(PROJECT)
update: update-license
update-license:
	ssnip README.md
## -- BLOCK:license --
## -- BLOCK:man --
## -- BLOCK:man --
