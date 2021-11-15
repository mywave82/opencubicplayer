#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "timidityplay.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "stuff/timer.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

#warning timiditySetVol does not work yet, hence a bunch of keys are disabled

/* options */
static time_t starttime;
static time_t pausetime;
static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];
static char *modname;
static char *composer;

static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;

static void startpausefade(void)
{
	if (plPause)
		starttime=starttime+dos_clock()-pausetime;

	if (pausefadedirect)
	{
		if (pausefadedirect<0)
			plPause=1;
		pausefadestart=2*dos_clock()-DOS_CLK_TCK-pausefadestart;
	} else
		pausefadestart=dos_clock();

	if (plPause)
	{
		plChanChanged=1;
		timidityPause(plPause=0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade(void)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=0;
		if (i>=64)
		{
			i=64;
			pausefadedirect=0;
		}
	} else {
		i=64-(dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i>=64)
			i=64;
		if (i<=0)
		{
			i=0;
			pausefadedirect=0;
			pausetime=dos_clock();
			timidityPause(plPause=1);
			plChanChanged=1;
			mcpSetFadePars(64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetFadePars(i);
}

static int timidityLooped(void)
{
	if (pausefadedirect)
		dopausefade();
	timiditySetLoop(fsLoopMods);
	timidityIdle();
	if (plrIdle)
		plrIdle();
	return !fsLoopMods&&timidityIsLooped();
}

static void timidityDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
	struct mglobinfo gi;
	long tim;

	mcpDrawGStrings(buf);

	timidityGetGlobInfo(&gi);

	if (plPause)
		tim=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim=(dos_clock()-starttime)/DOS_CLK_TCK;

	mcpDrawGStrings(buf);
	if (plScrWidth<128)
	{
#if 0
		memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
#endif
		memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

#if 0
		writestring(buf[0], 0, 0x09, " vol: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 15);
		writestring(buf[0], 15, 0x09, " srnd: \xfa  pan: l\xfa\xfa\xfam\xfa\xfa\xfar  bal: l\xfa\xfa\xfam\xfa\xfa\xfar ", 41);
		writestring(buf[0], 56, 0x09, " spd: ---%   ptch: ---  ", 24);
		writestring(buf[0], 6, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+4)>>3);
		writestring(buf[0], 22, 0x0F, srnd?"x":"o", 1);
		if (((pan+70)>>4)==4)
			writestring(buf[0], 34, 0x0F, "m", 1);
		else {
			writestring(buf[0], 30+((pan+70)>>4), 0x0F, "r", 1);
			writestring(buf[0], 38-((pan+70)>>4), 0x0F, "l", 1);
		}

		writestring(buf[0], 46+((bal+70)>>4), 0x0F, "I", 1);
		_writenum(buf[0], 62, 0x0F, speed*100/256, 10, 3);
		if (pitch < 0)
		{
			writestring(buf[0], 74, 0x0F, "-", 1);
			_writenum(buf[0], 75, 0x0F, -pitch, 10, 3);
		} else {
			_writenum(buf[0], 75, 0x0F, pitch, 10, 3);
		}
#endif

		writestring(buf[1], 57, 0x09, "                       ", 23);

		writestring(buf[1],  0, 0x09, " pos: ......../........           ", 57);
		writenum(buf[1], 6, 0x0F, gi.curtick, 16, 8, 0);
		writenum(buf[1], 15, 0x0F, gi.ticknum-1, 16, 8, 0);

		writestring(buf[2],  0, 0x09, "   midi \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................               time: ..:.. ", 80);
		writestring(buf[2],  8, 0x0F, currentmodname, 8);
		writestring(buf[2], 16, 0x0F, currentmodext, 4);
#warning modname is not UTF-8....
		writestring(buf[2], 22, 0x0F, modname, 31);
		if (plPause)
			writestring(buf[2], 58, 0x0C, "paused", 6);
		writenum(buf[2], 74, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 76, 0x0F, ":", 1);
		writenum(buf[2], 77, 0x0F, tim%60, 10, 2, 0);
	} else {
#if 0
		memset(buf[0]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
#endif
		memset(buf[1]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

#if 0
		writestring(buf[0], 0, 0x09, "    volume: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 30);
		writestring(buf[0], 30, 0x09, " surround: \xfa   panning: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar   balance: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar  ", 72);
		writestring(buf[0], 102, 0x09,  " speed: ---%   pitch: ---     ", 30);
		writestring(buf[0], 12, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+2)>>2);
		writestring(buf[0], 41, 0x0F, srnd?"x":"o", 1);
		if (((pan+68)>>3)==8)
			writestring(buf[0], 62, 0x0F, "m", 1);
		else {
			writestring(buf[0], 54+((pan+68)>>3), 0x0F, "r", 1);
			writestring(buf[0], 70-((pan+68)>>3), 0x0F, "l", 1);
		}

		writestring(buf[0], 83+((bal+68)>>3), 0x0F, "I", 1);
		_writenum(buf[0], 110, 0x0F, speed*100/256, 10, 3);
		if (pitch < 0)
		{
			writestring(buf[0], 123, 0x0F, "-", 1);
			_writenum(buf[0], 124, 0x0F, -pitch, 10, 3);
		} else {
			_writenum(buf[0], 124, 0x0F, pitch, 10, 3);
		}
#endif

		writestring(buf[1],  0, 0x09, "   position: ......../........             ", 80);
		writenum(buf[1], 13, 0x0F, gi.curtick, 16, 8, 0);
		writenum(buf[1], 22, 0x0F, gi.ticknum-1, 16, 8, 0);
		writestring(buf[1], 92, 0x09, "                                        ", 40);

		writestring(buf[2],  0, 0x09, "    module \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................  composer: ...............................                  time: ..:..    ", 132);
		writestring(buf[2], 11, 0x0F, currentmodname, 8);
		writestring(buf[2], 19, 0x0F, currentmodext, 4);
#warning modname and composer is not UTF-8....
		writestring(buf[2], 25, 0x0F, modname, 31);
		writestring(buf[2], 68, 0x0F, composer, 31);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
	}
}

static int timidityProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp('<', "Jump back (big)");
			cpiKeyHelp(KEY_CTRL_LEFT, "Jump back (big)");
			cpiKeyHelp('>', "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Jump forward (big)");
			cpiKeyHelp(KEY_CTRL_UP, "Jump back (small)");
			cpiKeyHelp(KEY_CTRL_DOWN, "Jump forward (small)");
			cpiKeyHelp(KEY_CTRL_HOME, "Jump to start of track");
			mcpSetProcessKey (key);
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			pausefadedirect=0;
			if (plPause)
				starttime=starttime+dos_clock()-pausetime;
			else
				pausetime=dos_clock();
			plPause=!plPause;
			timidityPause(plPause);
			break;
		case KEY_CTRL_UP:
			timiditySetRelPos(-1); /* seconds */
			break;
		case KEY_CTRL_DOWN:
			timiditySetRelPos(1);
			break;
		case '<':
		case KEY_CTRL_LEFT:
			timiditySetRelPos(-10);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			timiditySetRelPos(10);
			break;
		case KEY_CTRL_HOME:
			timidityRestart ();
			break;
		default:
			return mcpSetProcessKey (key);
	}
	return 1;

}

static int timidityOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader) /* no loader needed/used by this plugin */
{
	const char *filename;
	int err;

	if (!file)
		return errGen;

#warning replace currentmodname and currentmodext
	//strncpy(currentmodname, info->name, _MAX_FNAME);
	//strncpy(currentmodext, info->name + _MAX_FNAME, _MAX_EXT);

	modname=info->title;
	composer=info->composer;

	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s...\n", filename);

	plIsEnd=timidityLooped;
	plProcessKey=timidityProcessKey;
	plDrawGStrings=timidityDrawGStrings;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;
	plUseDots(timidityGetDots);

	plNLChan=16;
	timidityChanSetup(/*&mid*/);

	{
		const char *path;
		size_t buffersize = 64*1024;
		uint8_t *buffer = (uint8_t *)malloc (buffersize);
		size_t bufferfill = 0;

		int res;

		dirdbGetName_internalstr (file->dirdb_ref, &path);

		while (!file->eof (file))
		{
			if (buffersize == bufferfill)
			{
				if (buffersize >= 64*1024*1024)
				{
					fprintf (stderr, "timidityOpenFile: %s is bigger than 64 Mb - further loading blocked\n", path);
					free (buffer);
					return -1;
				}
				buffersize += 64*1024;
				buffer = (uint8_t *)realloc (buffer, buffersize);
			}
			res = file->read (file, buffer + bufferfill, buffersize - bufferfill);
			if (res<=0)
				break;
			bufferfill += res;
		}

		err = timidityOpenPlayer(path, buffer, bufferfill, file); /* buffer will be owned by the player */
		if (err)
		{
			free (buffer);
			return err;
		}
	}

	starttime=dos_clock();
	plPause=0;
	pausefadedirect=0;


	return errOk;
}

static void timidityCloseFile(void)
{
	timidityClosePlayer();
}

struct cpifaceplayerstruct timidityPlayer = {timidityOpenFile, timidityCloseFile};
struct linkinfostruct dllextinfo = {.name = "playtimidity", .desc = "OpenCP TiMidity++ Player (c) 2016 TiMidity++ team & Stian Skjelstad", .ver = DLLVERSION, .size = 0};


