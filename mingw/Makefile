TOPDIR=../
include $(TOPDIR)Rules.make
include Makefile-static

ifeq ($(WINDOWS),1)
all: ocp.res
else
all:
endif

uninstall:

install:

clean:
	rm -f ocp.res

ocp.ico: ../desktop/opencubicplayer.svg
	convert $< -define icon:auto-resize=256,128,64,48,32,16 $@
	touch ocp.ico

ocp.res: ocp.rc ocp.ico
	$(WINDRES) $< -O coff -o $@
