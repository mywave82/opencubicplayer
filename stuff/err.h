#ifndef __ERR_H
#define __ERR_H

enum
{
  errOk=0,
  errGen=-1,
  errAllocMem=-9,
  errAllocSamp=-10,
  errFileOpen=-17,
  errFileRead=-18,
  errFileWrite=-19,
  errFileMiss=-20,
  errFormStruc=-25,
  errFormSig=-26,
  errFormOldVer=-27,
  errFormNewVer=-28,
  errFormSupp=-29,
  errFormMiss=-30,
  errPlay=-33,
  errSymSym=-41,
  errSymMod=-42,

  errHelpPrinted=-100
};

const char *errGetShortString(int err);
const char *errGetLongString(int err);

#endif
