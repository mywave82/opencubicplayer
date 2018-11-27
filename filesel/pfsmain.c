/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Main routine, calls fileselector and interface code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs
 *    -complete restructuring of fsMain_ etc. to enable non-playable
 *     file types which are handled differently
 *    -therefore, added "handler" line in CP.INI filetype entries
 *    -added routines to read out PLS and M3U play lists
 *    -as always, added dllinfo record ;)
 *  -fd981014   Felix Domke <tmbinc@gmx.net>
 *    -small bugfix (tf was closed, even if it was NULL)
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -minor tweak.. Do not touch any screen functions after conRestore and
 *     before conSave
 */

#define NO_PFILESEL_IMPORT
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "stuff/err.h"
#include "mdb.h"
#include "pfilesel.h"
#include "boot/plinkman.h"
#include "boot/pmain.h"
#include "stuff/poutput.h"
#include "boot/psetting.h"

extern struct mdbreaddirregstruct adbReadDirReg, dosReadDirReg, fsReadDirReg, plsReadDirReg, m3uReadDirReg;
extern struct mdbreadinforegstruct fsReadInfoReg;

static struct interfacestruct *plintr = 0;
static struct interfacestruct *nextintr = 0;
static FILE *thisf=NULL;
static FILE *nextf=NULL;
static struct moduleinfostruct nextinfo;
static struct moduleinfostruct plModuleInfo;
static char thispath[PATH_MAX+1];
static char nextpath[PATH_MAX+1];

typedef enum
{
	DoNotForceCallFS = 0,
	DoForceCallFS = 1
} enumForceCallFS;

typedef enum
{
	DoNotAutoCallFS = 0,
	DoAutoCallFS = 1
} enumAutoCallFS;

typedef enum
{
	DoNotForceNext=0,
	DoForceNext=1,
	DoForcePrev=2
} enumForceNext;

static enumAutoCallFS callfs;
static char firstfile;

static interfaceReturnEnum stop;

/* stop: 0 cont
 *       1 next song
 *       6 prev song
 *       2 quit
 *       3 iface : next song or fs
 *       4 iface : call fs
 *       5 iface : dosshell
 */



/* return values:
 * 0  - no files available (and user hit esc)
 * 1  - we have a new song availble
 * -1 - error occured
 */

int callselector (char *path, struct moduleinfostruct *info, FILE **fi,
                  enumAutoCallFS callfs, enumForceCallFS forcecall, enumForceNext forcenext,
                  struct interfacestruct **iface)
{
	int ret;
	int result;
	char tpath[PATH_MAX+1];
	struct interfacestruct *intr;
	struct filehandlerstruct *hdlr;
	struct moduleinfostruct tmodinfo;
	char secname[20];
	FILE *tf=NULL;

	*iface=0;
	*fi=0;

	do
	{
		ret=result=0;
		if ((callfs && !fsFilesLeft())||forcecall)
			ret=result=fsFileSelect();

		if (!fsFilesLeft())
			return 0;

		while (ret || forcenext)
		{
			conRestore();

			if (!fsFilesLeft())
			{
				conSave();
				break;
			}
			if (forcenext==2)
			{
				if (!fsGetPrevFile(tpath, &tmodinfo, &tf))
				{
					if (tf)
					{
						fclose(tf);
						tf=NULL;
					}
					conSave();
					continue;
				}
			} else {
				if (!fsGetNextFile(tpath, &tmodinfo, &tf))
				{
					if (tf)
					{
						fclose(tf);
						tf=NULL;
					}
					conSave();
					continue;
				}
			}

			sprintf(secname, "filetype %d", tmodinfo.modtype&0xFF);
			intr=plFindInterface(cfGetProfileString(secname, "interface", ""));
			hdlr=(struct filehandlerstruct *)_lnkGetSymbol(cfGetProfileString(secname, "handler", ""));

			if (hdlr)
			{
				hdlr->Process(tpath, &tmodinfo, &tf);
			}

			conSave();
			{
				unsigned int i;
				for (i=0;i<plScrHeight;i++)
					displayvoid(i, 0, plScrWidth);
			}

			if (intr)
			{
				ret=0;
				*iface=intr;
				memcpy(&*info, &tmodinfo, sizeof(*info));
				*fi=tf;
				strcpy(path,tpath);
				return result?-1:1;
			} else {
				if(tf)
				{
					fclose(tf);
					tf=NULL;
				}
				fsForceRemove(tpath);
			}
		}
		if (ret)
			conSave();
	} while (ret);

	return 0;
}

static int _fsMain(int argc, char *argv[])
{
	conSave();

	callfs=DoNotAutoCallFS;
	stop=interfaceReturnContinue;
	firstfile=1;

#ifdef DOS32
  /* TODO.. this can be done on xterm consoles, and proc titles*/
	setwintitle("OpenCP");
#endif

	fsRescanDir();

	while (1)
	{
		struct preprocregstruct *prep;

/*
		while (ekbhit())
		{
			uint16_t key=egetch();
			if ((key==0)||(key==3)||(key==27))
				stop=interfaceReturnQuit;
		}*/
		if (stop==interfaceReturnQuit)
			break;

		if (!plintr)
		{
			int fsr;
			conSave();
			fsr=callselector(nextpath,&nextinfo,&nextf,(callfs||firstfile), (stop==interfaceReturnCallFs)?DoForceCallFS:DoNotForceCallFS, DoForceNext, &nextintr);
			if (!fsr)
			{
				break;
			} else if (fsr==-1)
			{
				callfs=DoAutoCallFS;
			}
			conRestore();
		}
		stop=interfaceReturnContinue;

		if (callfs)
			firstfile=0;

		if (nextintr)
		{
			conRestore();

			if (plintr)
			{
				plintr->Close();
				plintr=0;
				/* _heapshrink(); */
			}

			if (thisf)
			{
				fclose(thisf);
				thisf=NULL;
			}

			strcpy(thispath,nextpath);
			thisf=nextf;
			nextf=NULL;
			plModuleInfo=nextinfo;
			plintr=nextintr;
			nextintr=0;

			stop=interfaceReturnContinue;

			for (prep=plPreprocess; prep; prep=prep->next)
				prep->Preprocess(thispath, &plModuleInfo, &thisf);

			if (!plintr->Init(thispath, &plModuleInfo, &thisf))
			{
				stop = interfaceReturnCallFs; /* file failed, exit to filebrowser, if we don't do this, we can end up with a freeze if we only have this invalid file in the playlist, optional we could remove the file... */
				plintr=0;
			} else {
				/*
				char wt[256];
				char realname[13];
				memset(wt,0,256);
				fsConv12FileName(realname,plModuleInfo.name);
				strncpy(wt,realname,12);
				strcat(wt," - ");
				strncat(wt,plModuleInfo.modname,32);
			#ifdef DOS32
				setwintitle(wt);
			#endif
				*/
			}
			conSave();
		}

		if (plintr)
		{

/*
			while (ekbhit())
				egetch();*/

			while (!stop) /* != interfaceReturnContinue */
			{
				stop=plintr->Run();
				switch (stop)
				{
					case interfaceReturnContinue: /* 0 */
					case interfaceReturnQuit:
						break;
					case interfaceReturnNextAuto: /* next playlist file (auto) */
						if (callselector(nextpath,&nextinfo,&nextf, DoAutoCallFS, DoNotForceCallFS, DoNotForceNext, &nextintr)==0)
						{
							if (fsFilesLeft())
							{
								callselector(nextpath, &nextinfo,&nextf, DoNotAutoCallFS, DoNotForceCallFS, DoForceNext, &nextintr);
								stop = interfaceReturnNextAuto;
							} else
								stop = interfaceReturnQuit;
						} else
							stop = interfaceReturnNextAuto;
						break;
					case interfaceReturnPrevManuel: /* prev playlist file (man) */
						if (fsFilesLeft())
							stop=callselector(nextpath,&nextinfo,&nextf, DoNotAutoCallFS, DoNotForceCallFS, DoForcePrev, &nextintr)?interfaceReturnNextAuto:interfaceReturnContinue;
						else
							stop=callselector(nextpath,&nextinfo,&nextf, DoAutoCallFS, DoNotForceCallFS, DoNotForceNext, &nextintr)?interfaceReturnNextAuto:interfaceReturnContinue;
						break;
					case interfaceReturnNextManuel: /* next playlist file (man) */
						if (fsFilesLeft())
							stop=callselector(nextpath,&nextinfo,&nextf, DoNotAutoCallFS, DoNotForceCallFS, DoForceNext, &nextintr)?interfaceReturnNextAuto:interfaceReturnContinue;
						else
							stop=callselector(nextpath,&nextinfo,&nextf, DoAutoCallFS, DoNotForceCallFS, DoNotForceNext, &nextintr)?interfaceReturnNextAuto:interfaceReturnContinue;
						break;
					case interfaceReturnCallFs: /* call fs */
						stop=callselector(nextpath,&nextinfo,&nextf, DoAutoCallFS, DoForceCallFS, DoNotForceNext, &nextintr)?interfaceReturnNextAuto:interfaceReturnContinue;
						break;
					case interfaceReturnDosShell: /* dos shell */
						plSetTextMode(fsScrType);
						if (conRestore())
							break;
						stop=interfaceReturnContinue;
						plDosShell();
						conSave();
/*
						while (ekbhit())
							egetch();*/
						break;
				}
			}
			firstfile=0;
		}
	}

	plSetTextMode(fsScrType);
	conRestore();
	if (plintr)
		plintr->Close();
	if (thisf)
	{
		fclose(thisf);
		thisf=NULL;
	}
	return errOk;
}

static int fspreint(void)
{
	mdbRegisterReadDir(&adbReadDirReg);
	mdbRegisterReadDir(&dosReadDirReg);
	mdbRegisterReadDir(&fsReadDirReg);
	mdbRegisterReadDir(&plsReadDirReg);
	mdbRegisterReadDir(&m3uReadDirReg);
	mdbRegisterReadInfo(&fsReadInfoReg);

	fprintf(stderr, "initializing fileselector...\n");
	if (!fsPreInit())
	{
		fprintf(stderr, "fileselector pre-init failed!\n");
		return errGen;
	}

	return errOk;
}

static int fsint(void)
{
	if (!fsInit())
	{
		fprintf(stderr, "fileselector init failed!\n");
		return errGen;
	}

	return errOk;
}

static int fslateint(void)
{
	if (!fsLateInit())
	{
		fprintf(stderr, "fileselector post-init failed!\n");
		return errGen;
	}

	return errOk;
}

static void fsclose()
{
	mdbUnregisterReadDir(&adbReadDirReg);
	mdbUnregisterReadDir(&dosReadDirReg);
	mdbUnregisterReadDir(&fsReadDirReg);
	mdbUnregisterReadDir(&plsReadDirReg);
	mdbUnregisterReadDir(&m3uReadDirReg);
	mdbUnregisterReadInfo(&fsReadInfoReg);

	fsClose();
}






#if 0
void addtoplaylist(const char *source,const char *homepath, char *dest)
{
  char finalpath[_MAX_PATH];
  char longpath[_MAX_PATH];

  if (strchr(source,'\\'))
    strcpy(longpath,source);
  else
  {
    strcpy(longpath,homepath);
    strcat(longpath,source);
  }
  strcpy(finalpath,longpath);

#ifdef DOS32
  callrmstruct r;
  char *mem;
  void __far16 *rmptr;
  __segment pmsel;
  mem=(char*)dosmalloc(1024, rmptr, pmsel);
  if (!mem)
    return;
  strcpy(mem, longpath);
  strcpy(mem+512, longpath);
  clearcallrm(r);
  r.w.ax=0x7160;
  r.b.cl=1;
  r.b.ch=0x80;
  r.w.si=(unsigned short)rmptr;
  r.w.di=((unsigned short)rmptr)+512;
  r.s.ds=((unsigned long)rmptr)>>16;
  r.s.es=((unsigned long)rmptr)>>16;
  r.s.flags|=1;
  intrrm(0x21,r);

  if (!(r.s.flags&1))
    strcpy(finalpath, mem+512);
  else if (strchr(finalpath,' '))
    *finalpath=0;

  dosfree(pmsel);
#endif
  if (!memcmp(finalpath,"\\\\",2))
    *finalpath=0;

  if (*finalpath)
  {
    strcat(dest,finalpath);
    strcat(dest," ");
  }
}

static char bdest[32768];

int addpls(const char *path, moduleinfostruct &info, binfile *&fil)
{
  printf("adding %s to playlist ...\n", path);

  char dir[_MAX_DIR];
  char pspec[_MAX_PATH];
  _splitpath(path, pspec, dir, 0, 0);
  strcat(pspec,dir);

  *bdest=0;

  char *buffer = new char[fil->length()+128];
  char *bufend = buffer+fil->length();
  *bufend=0;
  fil->seek(0);
  fil->read(buffer,fil->length());

  char *bufptr=buffer;
  char *lineptr[1000];
  int linenum=0;

  while (bufptr<bufend)
  {
    while(bufptr<bufend && *bufptr<33)
      bufptr++;
    if (bufptr==bufend)
      break;
    lineptr[linenum]=bufptr;
    while (bufptr<bufend && *bufptr>=32)
      bufptr++;
    if (bufptr==bufend)
      break;
    *bufptr++=0;
    linenum++;
  }

  int snum=1, fnd;
  char lbuf[10];
  strcpy(lbuf,"File");
  if (linenum && !strcmp(lineptr[0],"[playlist]"))
  {
    do
    {
      ltoa(snum,lbuf+4,10);
      for (fnd=1; fnd<linenum; fnd++)
        if (!memcmp(lineptr[fnd],lbuf,strlen(lbuf)) && lineptr[fnd][strlen(lbuf)]=='=')
          break;
      if (fnd<linenum)
      {
        addtoplaylist(lineptr[fnd]+strlen(lbuf)+1,pspec,bdest);
        snum++;
      }
    } while (fnd<linenum);
  }
  delete buffer;

  strcpy(info.modname,"PLS play list (");
  ltoa(snum-1,info.modname+21,10);
  strcat(info.modname," entries)");

  fsAddFiles(bdest);

  return 1;
}




int addm3u(const char *path, moduleinfostruct &info, binfile *&fil)
{
  printf("adding %s to playlist ...\n", path);

  char dir[_MAX_DIR];
  char pspec[_MAX_PATH];
  _splitpath(path, pspec, dir, 0, 0);
  strcat(pspec,dir);

  *bdest=0;

  char *buffer = new char[fil->length()+128];
  char *bufend = buffer+fil->length();
  *bufend=0;
  fil->seek(0);
  fil->read(buffer,fil->length());

  char *bufptr=buffer;
  char *lineptr[1000];
  int linenum=0;

  while (bufptr<bufend)
  {
    while(bufptr<bufend && *bufptr<33)
      bufptr++;
    if (bufptr==bufend)
      break;
    lineptr[linenum]=bufptr;
    while (bufptr<bufend && *bufptr>=32)
      bufptr++;
    if (bufptr==bufend)
      break;
    *bufptr++=0;
    if (strchr(lineptr[linenum],';'))
      *strchr(lineptr[linenum],';')=0;
    if (lineptr[linenum][0])
      linenum++;
  }

  int fnd;
  for (fnd=0; fnd<linenum; fnd++)
    addtoplaylist(lineptr[fnd],pspec,bdest);

  delete buffer;

  strcpy(info.modname,"M3U play list (");
  ltoa(linenum,info.modname+21,10);
  strcat(info.modname," entries)");

  fsAddFiles(bdest);

  return 1;
}
#endif

/*
struct filehandlerstruct fsAddPLS={addpls};
struct filehandlerstruct fsAddM3U={addm3u};
*/
static struct mainstruct fsmain = { _fsMain };

static void __attribute__((constructor))init(void)
{
	if (ocpmain)
		fprintf(stderr, "pfsmain.c: ocpmain != NULL\n");
	else
		ocpmain = &fsmain;
}

static void __attribute__((destructor))done(void)
{
	if (ocpmain!=&fsmain)
		ocpmain = 0;
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "pfilesel", .desc = "OpenCP Fileselector (c) 1994-10 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0, .PreInit = fspreint, .Init = fsint, .LateInit = fslateint, .LateClose = fsclose};
