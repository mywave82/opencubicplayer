TOPDIR=..
include $(TOPDIR)/Rules.make

all:
	$(MAKE) -C texi all TOPDIR="../$(TOPDIR)"

clean:
	$(MAKE) -C texi clean TOPDIR="../$(TOPDIR)"

install:
	$(MAKE) -C texi install TOPDIR="../$(TOPDIR)" DESTDIR="$(DESTDIR)"

uninstall:
	$(MAKE) -C texi uninstall TOPDIR="../$(TOPDIR)" DESTDIR="$(DESTDIR)"
