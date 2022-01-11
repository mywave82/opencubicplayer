TOPDIR=..
include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C texi all TOPDIR="../$(TOPDIR)"

clean:
	$(MAKE) -C texi clean TOPDIR="../$(TOPDIR)"

install:
	$(MAKE) -C texi install TOPDIR="../$(TOPDIR)" DESTDIR="$(DESTDIR)"
	$(MKDIR) $(DESTDIR)$(MANDIR)
	$(CP) ocp.1 $(DESTDIR)$(MANDIR)

uninstall:
	$(MAKE) -C texi uninstall TOPDIR="../$(TOPDIR)" DESTDIR="$(DESTDIR)"
	rm -f $(DESTDIR)$(MANDIR)/ocp.1
