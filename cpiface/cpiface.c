/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CPIface main interface code
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
 *  -kb980517   Tammo Hinrichs <kb@nwn.de>
 *    -fixed one small bug in Ctrl-Q/Ctrl-S key handler
 *    -various minor changes
 *  -doj980928  Dirk Jagdmann <doj@cubic.org>
 *    -added cpipic.h to the #include list
 *  -kb981118   Tammo Hinrichs <opencp@gmx.net>
 *    -restructured key handler to let actual modes override otherwise
 *     important keys
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT', along with some
 *     other portability-related changes
 *  -doj990328  Dirk Jagdmann <doj@cubic.org>
 *    -fixed bug in delete plOpenCPPict
 *    -changed note strings
 *    -made title string Y2K compliant
 *  -doj20020410 Dirk Jagdmann <doj@cubic.org>
 *    -added screenshot
 *  -doj20020901 Dirk Jagdmann <doj@cubic.org>
 *    -added plLoopPatterns to enable/disable pattern looping
 *  -ss20040709 Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing, and not cputime/clock()
 *  -ss20040918 Stian Skjelstad <stian@nixia.no>
 *    -We printed the resolution wrong when it was 100 and 1000 characters wide
 */

#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "types.h"
#include "cpiface.h"
#include "cpipic.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "boot/plinkman.h"
#include "stuff/poutput.h"
#include "boot/psetting.h"
#include "stuff/framelock.h"
#include "stuff/timer.h"
#ifdef PLR_DEBUG
#include "dev/player.h"
#endif
#include <unistd.h>
#include <time.h>

#define MAXLCHAN 64

extern struct mdbreadinforegstruct cpiReadInfoReg;

void (*plGetRealMasterVolume)(int *l, int *r);

extern struct cpimoderegstruct cpiModeText;

static struct cpifaceplayerstruct *curplayer;
void (*plSetMute)(int i, int m);
void (*plDrawGStrings)(uint16_t (*)[CONSOLE_MAX_X]);
int (*plProcessKey)(uint16_t key);
int (*plIsEnd)(void);
void (*plIdle)(void);

char plMuteCh[MAXLCHAN];

void (*plGetMasterSample)(int16_t *, unsigned int len, uint32_t rate, int mode);
int (*plGetLChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);
int (*plGetPChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);

unsigned short plNLChan;
unsigned short plNPChan;
unsigned char plSelCh;
unsigned char plChanChanged;
static signed char soloch=-1;

char plPause;

char plCompoMode;
char plPanType;

static struct cpimoderegstruct *cpiModes;
static struct cpimoderegstruct *cpiDefModes;

uint16_t plTitleBuf[5][CONSOLE_MAX_X];
static uint16_t plTitleBufOld[4][CONSOLE_MAX_X];

time_t plEscTick;

static struct cpimoderegstruct *curmode;
static char curmodehandle[9];

static struct interfacestruct plOpenCP;

void cpiSetGraphMode(int big)
{
	plSetGraphMode(big);
	memset(plTitleBufOld, 0xFF, sizeof(plTitleBufOld));
	plChanChanged=1;
}

void cpiSetTextMode(int size)
{
	plSetTextMode(size);
	plChanChanged=1;
}

void cpiDrawGStrings()
{
	char *verstr="  opencp v" VERSION;
	char *author="(c) 1994-2016 Stian Skjelstad ";
	char tstr[CONSOLE_MAX_X+1];

#ifdef DEBUG
	sprintf(tstr, "%02i%% %08X %s", tmGetCpuUsage(),/* debugint, debugstr*/ 0, "");
#else
	strcpy(tstr, verstr);
	while (strlen(tstr)+strlen(author)<plScrWidth)
		strcat(tstr, " ");
	strcat(tstr, author);
#endif

	writestring(plTitleBuf[0], 0, plEscTick?0xC0:0x30, tstr, plScrWidth);
	if (plDrawGStrings)
		plDrawGStrings(plTitleBuf+1);
	else {
		writestring(plTitleBuf[1], 0, 0x07, "", 80);
		writestring(plTitleBuf[2], 0, 0x07, "", 80);
		writestring(plTitleBuf[3], 0, 0x07, "", 80);
	}

#ifdef PLR_DEBUG
	{
		char *temp;
		if (plrDebug)
			temp=plrDebug();
		else
			temp="";
		writestring(plTitleBuf[1], 132, 0x07, temp, 100);
	}
#endif

	if (plScrMode<100)
	{
		int chann;
		int chan0;
		int i;
		int offset;
		int limit=plScrWidth-(80-32);
		char lstr[CONSOLE_MAX_X+1];


		strcpy(lstr, " \xc4 \xc4\xc4 \xc4\xc4\xc4 \xc4\xc4\xc4\xc4\xc4\xc4\xc4  x  ");
		while (strlen(lstr)+10<plScrWidth)
			strcat(lstr, "\xc4");
		strcat(lstr, " \xc4\xc4\xc4 \xc4\xc4 \xc4 ");

		writestring(plTitleBuf[4], 0, 0x08, lstr, plScrWidth);

		if (plScrWidth>=1000)
				writenum(plTitleBuf[4], 15, 0x08, plScrWidth, 10, 4, 0);
			else if (plScrWidth>=100)
				writenum(plTitleBuf[4], 16, 0x08, plScrWidth, 10, 3, 0);
			else
				writenum(plTitleBuf[4], 17, 0x08, plScrWidth, 10, 2, 0);
		if (plScrHeight>=100)
			writenum(plTitleBuf[4], 20, 0x08, plScrHeight, 10, 3, 0);
		else
			writenum(plTitleBuf[4], 20, 0x08, plScrHeight, 10, 2, 0);

		if (limit<2)
			limit=2;

		chann=plNLChan;
		if (chann>limit)
			chann=limit;
		chan0=plSelCh-(chann/2);
		if ((chan0+chann)>=plNLChan)
			chan0=plNLChan-chann;
		if (chan0<0)
			chan0=0;

		offset=plScrWidth/2-chann/2;

		for (i=0; i<chann; i++)
		{
			unsigned short x;
			x='0'+(i+chan0+1)%10;
			if (plMuteCh[i+chan0]&&((i+chan0)!=plSelCh))
				x='\xc4'|0x0800;
			else
				if (plMuteCh[i+chan0])
					x|=0x8000;
				else
					if ((i+chan0)!=plSelCh)
						x|=0x0800;
					else
						x|=0x0700;
			plTitleBuf[4][offset+i+((i+chan0)>=plSelCh)]=x;
			if ((i+chan0)==plSelCh)
				plTitleBuf[4][offset+i]=(x&~0xFF)|('0'+(i+chan0+1)/10);
		}
		if (chann)
		{
			plTitleBuf[4][offset-1]=chan0?0x081B:0x0804;
			plTitleBuf[4][offset+1+chann]=((chan0+chann)!=plNLChan)?0x081A:0x0804;
		}

		displaystrattr(0, 0, plTitleBuf[0], plScrWidth);
		displaystrattr(1, 0, plTitleBuf[1], plScrWidth);
		displaystrattr(2, 0, plTitleBuf[2], plScrWidth);
		displaystrattr(3, 0, plTitleBuf[3], plScrWidth);
		displaystrattr(4, 0, plTitleBuf[4], plScrWidth);
	} else {
		gupdatestr(0, 0, plTitleBuf[0], plScrWidth, plTitleBufOld[0]);
		gupdatestr(1, 0, plTitleBuf[1], plScrWidth, plTitleBufOld[1]);
		gupdatestr(2, 0, plTitleBuf[2], plScrWidth, plTitleBufOld[2]);
		gupdatestr(3, 0, plTitleBuf[3], plScrWidth, plTitleBufOld[3]);

		if (plChanChanged)
		{
			int chann=plNLChan;
			int chan0;
			int i;
			int limit=plScrWidth-(80-32);
			/* int offset; */

			if (limit<2)
				limit=2;

			if (chann>limit)
				chann=limit;
			chan0=plSelCh-(chann/2);
			if ((chan0+chann)>=plNLChan)
				chan0=plNLChan-chann;
			if (chan0<0)
				chan0=0;

			/* offset=plScrWidth/2-chann/2; */

			for (i=0; i<chann; i++)
			{ /* needs tuning... TODO */
				gdrawchar8(384+i*8, 64, '0'+(i+chan0+1)/10, plMuteCh[i+chan0]?8:7, 0);
				gdrawchar8(384+i*8, 72, '0'+(i+chan0+1)%10, plMuteCh[i+chan0]?8:7, 0);
				gdrawchar8(384+i*8, 80, ((i+chan0)==plSelCh)?0x18:((i==0)&&chan0)?0x1B:((i==(chann-1))&&((chan0+chann)!=plNLChan))?0x1A:' ', 15, 0);
			}
		}
	}
}



void cpiResetScreen(void)
{
	if (curmode)
		curmode->SetMode();
}

static void cpiChangeMode(struct cpimoderegstruct *m)
{
	if (curmode)
		if (curmode->Event)
			curmode->Event(cpievClose);
	if (!m)
		m=&cpiModeText;
	curmode=m;
	if (m->Event) /* do not relay on parseing from left in if's  - Stian*/
		if (!m->Event(cpievOpen))
		curmode=&cpiModeText;
	curmode->SetMode();
}

void cpiGetMode(char *hand)
{
	strcpy(hand, curmode->handle);
}

void cpiSetMode(const char *hand)
{
	struct cpimoderegstruct *mod;
	for (mod=cpiModes; mod; mod=mod->next)
		if (!strcasecmp(mod->handle, hand))
			break;
	cpiChangeMode(mod);
}

void cpiRegisterMode(struct cpimoderegstruct *m)
{
	if (m->Event)
		if (!m->Event(cpievInit))
			return;
	m->next=cpiModes;
	cpiModes=m;
}

void cpiUnregisterMode(struct cpimoderegstruct *m)
{
	if (cpiModes==m)
	{
		cpiModes=m->next;
		return;
	} else {
		struct cpimoderegstruct *p = cpiModes;
		while (p)
		{
			if (p->next==m)
			{
				p->next=m->next;
				return;
			}
			p=p->next;
		}
	}
}

void cpiRegisterDefMode(struct cpimoderegstruct *m)
{
	m->nextdef=cpiDefModes;
	cpiDefModes=m;
}

static void cpiVerifyDefModes(void)
{
	struct cpimoderegstruct *p;

	while (cpiDefModes)
	{
		if (cpiDefModes->Event&&!cpiDefModes->Event(cpievInitAll))
			cpiDefModes=cpiDefModes->nextdef;
		else
			break;
	}
	p = cpiDefModes;
	while (p)
	{
		if (p->nextdef)
		{
			if (p->nextdef->Event&&!p->nextdef->Event(cpievInitAll))
				p->nextdef=p->nextdef->nextdef;
			else
				p=p->nextdef;
		} else
			break;
	}
}

static void cpiInitAllModes(void)
{
	struct cpimoderegstruct *p;

	for (p=cpiModes;p;p=p->next)
		if (p->Event)
			p->Event(cpievInit);
}

void cpiUnregisterDefMode(struct cpimoderegstruct *m)
{
	if (cpiDefModes==m)
	{
		cpiDefModes=m->next;
		return;
	} else {
		struct cpimoderegstruct *p = cpiDefModes;
		while (p)
		{
			if (p->nextdef==m)
			{
				p->nextdef=m->nextdef;
				return;
			}
			p=p->nextdef;
		}
	}
}

static int plmpInit(void)
{
	plCompoMode=cfGetProfileBool2(cfScreenSec, "screen", "compomode", 0, 0);
	strncpy(curmodehandle, cfGetProfileString2(cfScreenSec, "screen", "startupmode", "text"), 8);
	curmodehandle[8]=0;

	mdbRegisterReadInfo(&cpiReadInfoReg);

	cpiRegisterDefMode(&cpiModeText);

	cpiVerifyDefModes();

	cpiInitAllModes();

	cpiKeyHelpReset = cpiResetScreen;

	plRegisterInterface (&plOpenCP);

	return errOk;
}

static void plmpClose(void)
{
	plUnregisterInterface (&plOpenCP);
	mdbUnregisterReadInfo(&cpiReadInfoReg);

	while (cpiDefModes)
	{
		if (cpiDefModes->Event)
			cpiDefModes->Event(cpievDoneAll);
		cpiDefModes=cpiDefModes->nextdef;
	}

	if(plOpenCPPict)
	{
		free(plOpenCPPict);
		plOpenCPPict=NULL;
	}
}

static int linkhandle;

static int plmpOpenFile(const char *path, struct moduleinfostruct *info, FILE **fi)
{
	char secname[20];
	const char *link;
	const char *name;
	struct cpimoderegstruct *mod;
	void *fp;
	int retval;

	cpiModes=0;

	plEscTick=0;
	plPause=0;

	plNLChan=0;
	plNPChan=0;
	plSetMute=0;
	plIsEnd=0;
	plIdle=0;
	plGetMasterSample=0;
	plGetRealMasterVolume=0;
	plGetLChanSample=0;
	plGetPChanSample=0;

	strcpy(secname, "filetype ");

	sprintf(secname+strlen(secname), "%d", info->modtype&0xff);
/*
	ultoa(info.modtype&0xFF, secname+strlen(secname), 10);
*/

	link=cfGetProfileString(secname, "pllink", "");
	name=cfGetProfileString(secname, "player", "");

	linkhandle=lnkLink(link);
	if (linkhandle<0)
	{
		fprintf(stderr, "Error finding symbol (pllink in ocp.ini) %s\n", link);
		return 0;
	}

	fp=lnkGetSymbol(linkhandle, name);
	if (!fp)
	{
		lnkFree(linkhandle);
		fprintf(stderr, "Error finding symbol (player in ocp.ini) %s\n", name);
		fprintf(stderr, "link error\n");
		sleep(1);
		return 0;
	}

	curplayer=(struct cpifaceplayerstruct*)fp;

	retval=curplayer->OpenFile(path, info, *fi);

	if (retval)
	{
		lnkFree(linkhandle);
		fprintf(stderr, "error: %s\n", errGetShortString(retval));
		sleep(1);
		return 0;
	}

	for (mod=cpiDefModes; mod; mod=mod->nextdef)
		cpiRegisterMode(mod);
	for (mod=cpiModes; mod; mod=mod->next)
		if (!strcasecmp(mod->handle, curmodehandle))
			break;
	curmode=mod;

	soloch=-1;
	memset(plMuteCh, 0, sizeof(plMuteCh));
	plSelCh=0;

	return 1;
}

static void plmpCloseFile()
{
	cpiGetMode(curmodehandle);
	curplayer->CloseFile();
	while (cpiModes)
	{
		if (cpiModes->Event)
			cpiModes->Event(cpievDone);
		cpiModes=cpiModes->next;
	}
	lnkFree(linkhandle);
}

static void plmpOpenScreen()
{
	if (!curmode)
		curmode=&cpiModeText;
	if (curmode->Event&&!curmode->Event(cpievOpen))
		curmode=&cpiModeText;
	curmode->SetMode();
}


static void plmpCloseScreen()
{
	if (curmode->Event)
		curmode->Event(cpievClose);
/*
  cpimoderegstruct *mod;
  for (mod=cpiModes; mod; mod=mod->next)
    if (mod->Event)
      mod->Event(cpievClose);
*/
}

static int cpiChanProcessKey(uint16_t key)
{
	int i;
	switch (key)
	{
		/*case 0x4b00: //left*/
		case KEY_ALT_K:
			cpiKeyHelp(KEY_LEFT, "Select previous channel");
			cpiKeyHelp(KEY_UP, "Select next channel (and wrap)");
			cpiKeyHelp(KEY_RIGHT, "Select next channel");
			cpiKeyHelp(KEY_DOWN, "Select previous channel (and wrap)");
			cpiKeyHelp('1', "Select and toggle channel 1 on/off");
			cpiKeyHelp('2', "Select and toggle channel 2 on/off");
			cpiKeyHelp('3', "Select and toggle channel 3 on/off");
			cpiKeyHelp('4', "Select and toggle channel 4 on/off");
			cpiKeyHelp('5', "Select and toggle channel 5 on/off");
			cpiKeyHelp('6', "Select and toggle channel 6 on/off");
			cpiKeyHelp('7', "Select and toggle channel 7 on/off");
			cpiKeyHelp('8', "Select and toggle channel 8 on/off");
			cpiKeyHelp('9', "Select and toggle channel 9 on/off");
			cpiKeyHelp('0', "Select and toggle channel 10 on/off");
			cpiKeyHelp('Q', "Toggle selected channel 10 on/off");
			cpiKeyHelp('q', "Toggle selected channel 10 on/off");
			cpiKeyHelp('s', "Toggle solo on selected channel on/off");
			cpiKeyHelp('S', "Toggle solo on selected channel on/off");
			cpiKeyHelp(KEY_CTRL_S, "Enable all channels");
			cpiKeyHelp(KEY_CTRL_Q, "Enable all channels");
			return 0;
		case KEY_LEFT:
			if (plSelCh)
			{
				plSelCh--;
				plChanChanged=1;
			}
			break;
		/*case 0x4800: //up*/
		case KEY_UP:
			plSelCh=(plSelCh-1+plNLChan)%plNLChan;
			plChanChanged=1;
			break;
		/*case 0x4d00: //right*/
		case KEY_RIGHT:
			if ((plSelCh+1)<plNLChan)
			{
				plSelCh++;
				plChanChanged=1;
			}
			break;
		/*case 0x5000: //down*/
		case KEY_DOWN:
			plSelCh=(plSelCh+1)%plNLChan;
			plChanChanged=1;
			break;


		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': case '0':
/*TODO-keys
	  case 0x7800: case 0x7900: case 0x7A00: case 0x7B00: case 0x7C00:
	  case 0x7D00: case 0x7E00: case 0x7F00: case 0x8000: case 0x8100:*/
			if (key=='0')
				key=9;
			else
/*
				if (key<='9')*/
					key-='1';
/*
				else
					key=(key>>8)-0x78+10;*/
			if (key>=plNLChan)
				break;
			plSelCh=key;

		case 'q': case 'Q':
			plMuteCh[plSelCh]=!plMuteCh[plSelCh];
			plSetMute(plSelCh, plMuteCh[plSelCh]);
			plChanChanged=1;
			break;

		case 's': case 'S':
			if (plSelCh==soloch)
			{
				for (i=0; i<plNLChan; i++)
				{
					plMuteCh[i]=0;
					plSetMute(i, plMuteCh[i]);
				}
				soloch=-1;
			} else {
				for (i=0; i<plNLChan; i++)
				{
					plMuteCh[i]=i!=plSelCh;
					plSetMute(i, plMuteCh[i]);
				}
				soloch=plSelCh;
			}
			plChanChanged=1;
			break;

		case KEY_CTRL_Q: case KEY_CTRL_S: /* TODO-keys*/
			for (i=0; i<plNLChan; i++)
			{
				plMuteCh[i]=0;
				plSetMute(i, plMuteCh[i]);
			}
			soloch=-1;
			plChanChanged=1;
			break;
		default:
			return 0;
	}
	return 1;
}

/*
int plmpProcessKey(uint16_t key)
{
	struct cpimoderegstruct *mod;

	if (curmode->AProcessKey(key))
		return 1;
	for (mod=cpiModes; mod; mod=mod->next)
		if (mod->IProcessKey(key))
			return 1;
	if (plNLChan)
		if (cpiChanProcessKey(key))
			return 1;
	if (plProcessKey)
		if (plProcessKey(key))
			return 1;
	return 0;
}
*/

static interfaceReturnEnum plmpDrawScreen(void)
{
	int needdraw=1;
	struct cpimoderegstruct *mod;

	plChanChanged=0;

	if (plIsEnd)
		if (plIsEnd())
			return interfaceReturnNextAuto;

	if (plIdle)
		plIdle();

	for (mod=cpiModes; mod; mod=mod->next)
		mod->Event(cpievKeepalive);

	if (plEscTick&&(dos_clock()>(time_t)(plEscTick+2*DOS_CLK_TCK)))
		plEscTick=0;

	while (ekbhit())
	{
		uint16_t key=egetch();
/*
		if ((key&0xFF)==0xE0)
			key&=0xFF00;
		if (key&0xFF)
			key&=0x00FF;
*/
		needdraw=0;

		if (plEscTick)
		{
			plEscTick=0;
			if (key==KEY_ESC)
				return interfaceReturnQuit;
		}

#ifdef DEBUG
		DEBUGINT(key);
#endif

		if (curmode->AProcessKey(key))
		{
			curmode->Draw();
			continue;
		}

		switch (key)
		{
			struct cpimoderegstruct *mod;
			case KEY_ESC:
				plEscTick=dos_clock();
				break;
			case _KEY_ENTER:
				return interfaceReturnNextManuel;
			case 'f': case 'F':
			case KEY_INSERT:
			/* case 0x5200: //insert */
				return interfaceReturnCallFs;
			case 'd': case 'D': case KEY_CTRL_D:
			/* case 0x6f00: // alt-f8 TODO-keys*/
				return interfaceReturnDosShell;
			case KEY_CTRL_J:
				return interfaceReturnPrevManuel;
			case KEY_CTRL_K:
				return interfaceReturnNextManuel;
			case KEY_CTRL_L:
				fsLoopMods=!fsLoopMods;
				break;
			case KEY_ALT_C:
				fsSetup();
				plSetTextMode(fsScrType);
				fsScrType=plScrType;
				curmode->SetMode();
				break;
			#if 0
			TODO plLoopPatterns
			case KEY_ALT_L:
				plLoopPatterns=!plLoopPatterns;
			#endif
			#ifdef DEBUG
				if(plLoopPatterns)
					DEBUGSTR("pattern loop enabled");
				else
					DEBUGSTR("pattern loop disabled");
			#endif
			#ifdef DOS32 /* TODO*/
			case 0xF8:
				Screenshot();
				break;
			#endif
			case KEY_ALT_K:
				cpiKeyHelp(KEY_ESC, "Exit");
				cpiKeyHelp(_KEY_ENTER, "Next song");
				cpiKeyHelp(KEY_INSERT, "Open file selected");
				cpiKeyHelp('f', "Open file selector");
				cpiKeyHelp('F', "Open file selector");
				cpiKeyHelp('d', "Open a shell");
				cpiKeyHelp('D', "Open a shell");
				cpiKeyHelp(KEY_CTRL_D, "Open a shell");
				cpiKeyHelp(KEY_CTRL_J, "Prev song (forced)");
				cpiKeyHelp(KEY_CTRL_K, "Next song (forced)");
				cpiKeyHelp(KEY_CTRL_L, "Toggle song looping (ALT-C setting)");
				cpiKeyHelp(KEY_ALT_C, "Open setup dialog");
				/* cpiKeyHelp(KEY_ALT_L, "Toggle plLoopPatterns"); */
				/*return 0;*/
			default:
				for (mod=cpiModes; mod; mod=mod->next)
					if (mod->IProcessKey(key))
						goto fertigmitkeys;
				if (plNLChan)
					if (cpiChanProcessKey(key))
						goto fertigmitkeys;
				if (plProcessKey)
					plProcessKey(key);
				cpiKeyHelpDisplay();
			fertigmitkeys: ;
		}
		curmode->Draw();
	}

	if (needdraw)
		curmode->Draw();
	framelock();

	return interfaceReturnContinue;
}

static interfaceReturnEnum plmpCallBack(void)
{
	interfaceReturnEnum stop;

	plmpOpenScreen();
	stop=interfaceReturnContinue;
	while (!stop)
		stop=plmpDrawScreen();
	plmpCloseScreen();
	return stop;
}

char plNoteStr[132][4]=
{
	"c-1","c#1","d-1","d#1","e-1","f-1","f#1","g-1","g#1","a-1","a#1","b-1",
	"C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0",
	"C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1",
	"C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2",
	"C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3",
	"C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4",
	"C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5",
	"C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6",
	"C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7",
	"C-8","C#8","D-8","D#8","E-8","F-8","F#8","G-8","G#8","A-8","A#8","B-8",
	"C-9","C#9","D-9","D#9","E-9","F-9","F#9","G-9","G#9","A-9","A#9","B-9"
};

static struct interfacestruct plOpenCP = {plmpOpenFile, plmpCallBack, plmpCloseFile, "plOpenCP", NULL};
#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "cpiface", .desc = "OpenCP Interface (c) 1994-09 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0, .LateInit = plmpInit, .PreClose = plmpClose};
/* OpenCP Module Player */
