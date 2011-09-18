TOPDIR=../..
include $(TOPDIR)/Rules.make

all: liblzh.a

install:

liblzh.a: lzhlib.o
	$(AR) cr liblzh.a lzhlib.o

lzhlib.o: lzhlib.cpp lzh.h $(TOPDIR)/config.h
	$(CXX) $(CXXFLAGS) -c lzhlib.cpp

clean:
	$(RM) *~ *.o *.a

dist:
