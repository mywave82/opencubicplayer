copt = /otexan /fpi87
lopt =

exefile = tga2wav
objects = tga2wav.obj

$(exefile).exe: $(objects)
  *wlink $(lopt) &
    system dos4g &
    name $^*.exe &
    debug watcom all &
    file {$(objects)} &
    option map &
    option eliminate

.cpp.obj:
  *wpp386 $(copt) $<

clean:
  del *.obj
  del *.err
  del *.map
  del *.bak
  del $(exefile).exe
  del *.wav
