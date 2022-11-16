DESTDIR    =
PREFIX     =/usr/local
AR         =ar
CC         =gcc
CFLAGS     =-Wall -g
CFLAGS_ALL =$(LDFLAGS) $(CFLAGS) $(CPPFLAGS)
SCRIPTS    =blocks/snet_asink_r blocks/snet_sigsrc_r
PROGRAMS   =snet$(EXE)

##
all: $(PROGRAMS)
clean:
	@echo 'D $(PROGRAMS)'
	@rm -f $(PROGRAMS)
install: $(PROGRAMS) $(SCRIPTS)
	@echo 'I bin/ $(not-dir $(PROGRAMS) $(SCRIPTS))'
	@install -d $(DESTDIR)$(PREFIX)/bin
	@install -m755 $(PROGRAMS) $(SCRIPTS) $(DESTDIR)$(PREFIX)/bin
	@echo 'I share/snet/examples/ '
	@install -d $(DESTDIR)$(PREFIX)/share/snet/examples
	@cp -r examples/* $(DESTDIR)$(PREFIX)/share/snet/examples

##
snet$(EXE): snet.c
	@echo "B $@ $^"
	@$(CC) -o $@ $^ $(CFLAGS_ALL) $(LIBS)

## -- license --
install: install-license
install-license: LICENSE
	@echo 'I share/doc/c-snet/LICENSE'
	@mkdir -p $(DESTDIR)$(PREFIX)/share/doc/c-snet
	@cp LICENSE $(DESTDIR)$(PREFIX)/share/doc/c-snet
## -- license --
