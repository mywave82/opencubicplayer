TOPDIR=../..
include $(TOPDIR)/Rules.make

LIB = digidrum.o Ymload.o Ym2149Ex.o YmMusic.o YmUserInterface.o

all: libym.a

install:

libym.a: $(LIB)
	$(AR) cr libym.a $(LIB)

digidrum.o: digidrum.cpp YmTypes.h $(TOPDIR)/config.h
	$(CXX) $(CXXFLAGS) -c digidrum.cpp

Ym2149Ex.o: Ym2149Ex.cpp Ym2149Ex.h YmTypes.h $(TOPDIR)/config.h
	$(CXX) $(CXXFLAGS) -c Ym2149Ex.cpp

Ymload.o: Ymload.cpp YmMusic.h ../lzh/lzh.h $(TOPDIR)/config.h
	$(CXX) $(CXXFLAGS) -c Ymload.cpp -I..

YmMusic.o: YmMusic.cpp YmMusic.h $(TOPDIR)/config.h
	$(CXX) $(CXXFLAGS) -c YmMusic.cpp

YmUserInterface.o: YmUserInterface.cpp StSoundLibrary.h YmMusic.h $(TOPDIR)/config.h
	$(CXX) $(CXXFLAGS) -c YmUserInterface.cpp

clean:
	$(RM) *~ *.o *.a

dist:
