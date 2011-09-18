.silent
.erase

wap.exe: wap.obj
  @echo $@
  wlink option quiet file $< name $@ system dos4g

wap.obj: wap.cpp
  @echo $<
  wpp386 /zq /otexan /zp1 /4r /s /I../../binfile $<

clean: .symbolic
  @del wap.obj wap.exe >nul
