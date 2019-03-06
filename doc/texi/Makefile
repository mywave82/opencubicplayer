TOPDIR=../../
include $(TOPDIR)/Rules.make
ifeq ($(HAVE_MAKEINFO),yes)
endif

ifeq ($(HAVE_MAKEINFO),yes)
SOURCES=configuration.texi fileselector.texi mdz.texi player.texi starting.texi wap.texi faq.texi install.texi ocp.texi plugin.texi support.texi
all: ocp.info.gz

ocp.info.gz: $(SOURCES)
	rm -Rf ocp ocp.html ocp.info ocp.info.gz
	LC_ALL=POSIX makeinfo ocp.texi
	mv ocp ocp.info
	gzip -9 ocp.info
	LC_ALL=POSIX makeinfo --html ocp.texi
	mv ocp ocp.html

install: all
	mkdir -p "$(DESTDIR)$(INFODIR)"
	$(CP) ocp.info.gz "$(DESTDIR)$(INFODIR)"
	install-info --dir-file="$(DESTDIR)$(INFODIR)/dir" "$(DESTDIR)$(INFODIR)/ocp.info.gz" || true
	mkdir -p "$(DESTDIR)$(DOCDIR)/html"
	$(CP) -R ocp.html/* "$(DESTDIR)$(DOCDIR)/html"
uninstall:
	install-info --delete --dir-file="$(DESTDIR)$(INFODIR)/dir" "$(DESTDIR)$(INFODIR)/ocp.info.gz"
	rm -f "$(DESTDIR)$(INFODIR)/ocp.info.gz"
	rm -Rf "$(DESTDIR)$(DOCDIR)/html"
else
all:

install:

uninstall:
endif

clean:
	rm -Rf ocp ocp.info ocp.info.gz html ocp.html
