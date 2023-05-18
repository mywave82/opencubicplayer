TOPDIR=../../
include $(TOPDIR)/Rules.make
ifeq ($(HAVE_MAKEINFO),yes)
endif

ifeq ($(HAVE_MAKEINFO),yes)
SOURCES=configuration.texi fileselector.texi player.texi starting.texi wap.texi faq.texi ocp.texi support.texi
all: ocp.info.gz

ocp.info.gz: $(SOURCES)
	rm -Rf ocp ocp_html ocp.info ocp.info.gz
	LC_ALL=POSIX makeinfo ocp.texi
	mv ocp ocp.info
	gzip -9 ocp.info
	LC_ALL=POSIX makeinfo --html ocp.texi
	@# compensate for that texinfo version 7.0 creates a directory named "ocp_html", while prior versions uses just "ocp"
	if [ ! -d ocp_html ]; then mv ocp ocp_html; fi

install: all
	mkdir -p "$(DESTDIR)$(INFODIR)"
ifeq ($(INSTALL_INFO),1)
	$(CP) ocp.info.gz "$(DESTDIR)$(INFODIR)"
	install-info --dir-file="$(DESTDIR)$(INFODIR)/dir" "$(DESTDIR)$(INFODIR)/ocp.info.gz" || true
endif
	mkdir -p "$(DESTDIR)$(DOCDIR)/html"
	$(CP) -R ocp_html/* "$(DESTDIR)$(DOCDIR)/html"
uninstall:
	install-info --delete --dir-file="$(DESTDIR)$(INFODIR)/dir" "$(DESTDIR)$(INFODIR)/ocp.info.gz" || true
	rm -f "$(DESTDIR)$(INFODIR)/ocp.info.gz"
	rm -Rf "$(DESTDIR)$(DOCDIR)/html"
else
all:

install:

uninstall:
endif

clean:
	rm -Rf ocp ocp.info ocp.info.gz html ocp_html
