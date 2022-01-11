TOPDIR=..
include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C texi all TOPDIR="../$(TOPDIR)"

clean:
	$(MAKE) -C texi clean TOPDIR="../$(TOPDIR)"

install:
	$(MAKE) -C texi install TOPDIR="../$(TOPDIR)" DESTDIR="$(DESTDIR)"
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	$(CP) ocp.1 $(DESTDIR)$(MANDIR)/man1

uninstall:
	$(MAKE) -C texi uninstall TOPDIR="../$(TOPDIR)" DESTDIR="$(DESTDIR)"
	rm -f $(DESTDIR)$(MANDIR)/man1/ocp.1
