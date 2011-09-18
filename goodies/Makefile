TOPDIR=../
include $(TOPDIR)Rules.make

.PHONY: all clean

all:
		$(MAKE) -C helpc TOPDIR=../$(TOPDIR)

clean:
		-$(MAKE) -C helpc TOPDIR=../$(TOPDIR) clean

install:
	$(MAKE) -C helpc TOPDIR=../$(TOPDIR) install

uninstall:
	$(MAKE) -C helpc TOPDIR=../$(TOPDIR) uninstall
