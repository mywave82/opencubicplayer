/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'20 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlayer interface routines
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
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -added many many things to provide channel display and stuff
 *    -removed some bugs which caused crashing in some situations
 *  -ss040709   Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing instead of cputime/clock()
 */

#include "config.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviwave.h"
#include "dev/mcp.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "itplay.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"

__attribute__ ((visibility ("internal"))) struct itplayer itplayer = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static struct it_module mod = {{0},0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,{0},{0},0,0,0,0,0};

static struct it_instrument *insts;
static struct it_sample *samps;

static time_t starttime;
static time_t pausetime;
static time_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;
static char utf8_8_dot_3  [12*4+1];  /* UTF-8 ready */
static char utf8_16_dot_3 [20*4+1]; /* UTF-8 ready */
static struct moduleinfostruct mdbdata;

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
		mcpSet(-1, mcpMasterPause, plPause=0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade(void)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=0;
		if (i>=64)
		{
			i=64;
			pausefadedirect=0;
		}
	} else {
		i=64-((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i>=64)
			i=64;
		if (i<=0)
		{
			i=0;
			pausefadedirect=0;
			pausetime=dos_clock();
			mcpSet(-1, mcpMasterPause, plPause=1);
			mcpSetMasterPauseFadeParameters (64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetMasterPauseFadeParameters (i);
}


static int itpProcessKey(uint16_t key)
{
	int row;
	int pat, p;

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
			mcpSet(-1, mcpMasterPause, plPause^=1);
			break;
		case KEY_CTRL_HOME:
			itpInstClear();
			setpos (&itplayer, 0, 0);
			if (plPause)
				starttime=pausetime;
			else
				starttime=dos_clock();
			break;
		case '<':
		case KEY_CTRL_LEFT:
			p=getpos(&itplayer);
			pat=p>>16;
			setpos(&itplayer, pat-1, 0);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			p=getpos(&itplayer);
			pat=p>>16;
			setpos(&itplayer, pat+1, 0);
			break;
		case KEY_CTRL_UP:
			p=getpos(&itplayer);
			pat=p>>16;
			row=(p>>8)&0xFF;
			setpos(&itplayer, pat, row-8);
			break;
		case KEY_CTRL_DOWN:
			p=getpos(&itplayer);
			pat=p>>16;
			row=(p>>8)&0xFF;
			setpos(&itplayer, pat, row+8);
			break;
		default:
			return mcpSetProcessKey (key);
	}
	return 1;
}

static int itpLooped(void)
{
	return !fsLoopMods&&getloop(&itplayer);
}

static void itpIdle(void)
{
	setloop(&itplayer, fsLoopMods);
	if (mcpIdle)
		mcpIdle();
	if (pausefadedirect)
		dopausefade();
}

static void itpDrawGStrings (void)
{
	int pos=getrealpos(&itplayer)>>8;
	int gvol, bpm, tmp, gs;
	int i, nch = 0;

	mcpDrawGStrings ();

	getglobinfo(&itplayer, &tmp, &bpm, &gvol, &gs);

	/* mcpNChan == cpifaceSessionAPI.PhysicalChannelCount */
	for (i=0; i<mcpNChan; i++)
	{
		if (mcpGet(i, mcpCStatus))
		{
			nch++;
		}
	}

	mcpDrawGStringsTracked
	(
		utf8_8_dot_3,
		utf8_16_dot_3,
		0,          /* song X */
		0,          /* song Y */
		pos&0xFF,   /* row X */
		mod.patlens[mod.orders[pos>>8]]-1, /* row Y */
		pos>>8,     /* order X */
		mod.nord-1, /* order Y */
		tmp,        /* speed */
		bpm,        /* tempo */
		gvol,
		(gs==ifxGVSUp)?1:(gs==ifxGVSDown)?-1:0,
		nch,
		mcpNChan,
		mcpset.amp,
		(set.filter==1)?"AOI":(set.filter==2)?"FOI":"off",
		plPause,
		plPause?((pausetime-starttime)/DOS_CLK_TCK):((dos_clock()-starttime)/DOS_CLK_TCK),
		&mdbdata
	);
}

static void itpCloseFile(void)
{
	stop(&itplayer);
	it_free(&mod);
}

/**********************************************************************/

static void itpMarkInsSamp(uint8_t *ins, uint8_t *smp)
{
	int i;

	/* mod.nchan == cpiSessionAPI->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		int j;
		if (plMuteCh[i])
			continue;
		for (j=0; j<mod.nchan; j++)
		{
			int lc, in, sm;
			if (!chanactive(&itplayer, j, &lc))
				continue;
			if (lc!=i)
				continue;
			in=getchanins(&itplayer, j);
			sm=getchansamp(&itplayer, j);
			ins[in-1]=((plSelCh==i)||(ins[in-1]==3))?3:2;
			smp[sm]=((plSelCh==i)||(smp[sm]==3))?3:2;
		}
	}
}

/************************************************************************/

static void logvolbar(int *l, int *r)
{
	(*l)*=2;
	(*r)*=2;
	if ((*l)>32)
		(*l)=32+(((*l)-32)>>1);
	if ((*l)>48)
		(*l)=48+(((*l)-48)>>1);
	if ((*l)>56)
		(*l)=56+(((*l)-56)>>1);
	if ((*l)>64)
		(*l)=64;
	if ((*r)>32)
		(*r)=32+(((*r)-32)>>1);
	if ((*r)>48)
		(*r)=48+(((*r)-48)>>1);
	if ((*r)>56)
		(*r)=56+(((*r)-56)>>1);
	if ((*r)>64)
		(*r)=64;
}

static void drawvolbar(uint16_t *buf, int i, uint8_t st)
{
	int l,r;
	itplayer_getrealvol(&itplayer, i, &l, &r);
	logvolbar(&l, &r);

	l=(l+4)>>3;
	r=(r+4)>>3;
	if (plPause)
		l=r=0;
	if (st)
	{
		writestring(buf, 8-l, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", l);
		writestring(buf, 9, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0ffe};
		writestringattr(buf, 8-l, left+8-l, l);
		writestringattr(buf, 9, right, r);
	}
}

static void drawlongvolbar(uint16_t *buf, int i, uint8_t st)
{
	int l,r;
	itplayer_getrealvol(&itplayer, i, &l, &r);
	logvolbar(&l, &r);
	l=(l+2)>>2;
	r=(r+2)>>2;
	if (plPause)
		l=r=0;
	if (st)
	{
		writestring(buf, 16-l, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", l);
		writestring(buf, 17, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		writestringattr(buf, 16-l, left+16-l, l);
		writestringattr(buf, 17, right, r);
	}
}

static char *fxstr3[]={0,"vl\x18","vl\x19","fv\x18","fv\x19","pt\x18",
                       "pt\x19","pt\x0d","fp\x18","fp\x19","pn\x1a","pn\x1b",
                       "tre", "trr","vib","arp","cut","ret","ofs","eps",
                       "del", "cv\x18", "cv\x19", "fc\x18", "fc\x19","p-c",
                       "p-o", "p-f", "ve0", "ve1", "pe0", "pe1", "fe0",
                       "fe1", "pbr"
                      };

static char *fxstr6[]={0, "volsl\x18","volsl\x19","fvols\x18","fvols\x19",
                       "porta\x18","porta\x19","porta\x0d","fport\x18",
                       "fport\x19","pansl\x1a","pansl\x1b","tremol","tremor",
                       "vibrat","arpegg"," \x0e""cut ","retrig","offset",
                       "envpos","delay\x0d", "chvol\x18", "chvol\x19",
                       "fchvl\x18", "fchvl\x19", "past-C", "past-O",
                       "past-F", "venv:0", "venv:1", "penv:0", "penv:1",
                       "fenv:0", "fenv:1", "panbrl"
                      };

static char *fxstr12[]={0, "volumeslide\x18","volumeslide\x19",
                        "finevolslid\x18","finevolslid\x19","portamento \x18",
                        "portamento \x19","porta to \x0d  ","fine porta \x18",
                        "fine porta \x19","pan slide \x1a ","pan slide \x1b ",
                        "tremolo     ","tremor      ","vibrato     ",
                        "arpeggio    ","note cut    ","note retrig ",
                        "sampleoffset","set env pos ","note delay  ",
                        "chanvolslid\x18","chanvolslid\x19",
                        "finechvolsl\x18","finechvolsl\x19", "past cut",
                        "past off","past fade","vol env off","vol env on",
                        "pan env off", "pan env on", "pitchenv off",
                        "pitchenv on", "panbrello"
                       };


static void drawchannel(uint16_t *buf, int len, int i)
{
	uint8_t st=plMuteCh[i];

	uint8_t tcol=st?0x08:0x0F;
	uint8_t tcold=st?0x08:0x07;
	uint8_t tcolr=st?0x08:0x0B;

	int av;

	struct it_chaninfo ci;

	char *fxstr;

	switch (len)
	{
		case 36:
			writestring(buf, 0, tcold, " \xfa\xfa -- --- -- --- \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 36);
			break;
		case 62:
			writestring(buf, 0, tcold, " \xfa\xfa                      ---\xfa --\xfa -\xfa ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 62);
			break;
		case 128:
			writestring(buf,  0, tcold, " \xfa\xfa                             \xb3                   \xb3    \xb3   \xb3  \xb3            \xb3  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 128);
			break;
		case 76:
			writestring(buf,  0, tcold, " \xfa\xfa                             \xb3    \xb3   \xb3  \xb3            \xb3 \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 76);
			break;
		case 44:
			writestring(buf, 0, tcold, " \xfa\xfa -- ---\xfa --\xfa -\xfa ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 44);
			break;
	}

	av=getchanalloc(&itplayer, i);
	if (av)
		writenum(buf, 1, tcold, av, 16, 2, 0);

	if (!lchanactive(&itplayer, i))
		return;

	getchaninfo(&itplayer, i, &ci);

	switch (len)
	{
		case 36:
			writenum(buf,  4, tcol, ci.ins, 16, 2, 0);
			writestring(buf,  7, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writenum(buf, 11, tcol, ci.vol, 16, 2, 0);
			fxstr=fxstr3[ci.fx];
			if (fxstr)
				writestring(buf, 14, tcol, fxstr, 3);
			drawvolbar(buf+18, i, st);
			break;
		case 62:
			if (ci.ins)
			{
				if (*insts[ci.ins-1].name)
					writestring(buf,  4, tcol, insts[ci.ins-1].name, 19);
				else
				{
					writestring(buf,  4, 0x08, "(  )", 4);
					writenum(buf,  5, 0x08, ci.ins, 16, 2, 0);
				}
			}
			writestring(buf, 25, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 28, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 30, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 32, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			writestring(buf, 34, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			writestring(buf, 35, tcol, &" \x1A\x1B"[ci.panslide], 1);
			fxstr=fxstr6[ci.fx];
			if (fxstr)
				writestring(buf, 37, tcol, fxstr, 6);
			drawvolbar(buf+44, i, st);
			break;
		case 76:
			if (ci.ins)
			{
				if (*insts[ci.ins-1].name)
					writestring(buf,  4, tcol, insts[ci.ins-1].name, 28);
				else
				{
					writestring(buf,  4, 0x08, "(  )", 4);
					writenum(buf,  5, 0x08, ci.ins, 16, 2, 0);
				}
			}
			writestring(buf, 33, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 36, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 38, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 40, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			writestring(buf, 42, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			writestring(buf, 43, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=fxstr12[ci.fx];
			if (fxstr)
				writestring(buf, 45, tcol, fxstr, 12);

			drawvolbar(buf+59, i, st);
			break;
		case 128:
			if (ci.ins)
			{
				if (*insts[ci.ins-1].name)
					writestring(buf,  4, tcol, insts[ci.ins-1].name, 28);
				else
				{
					writestring(buf,  4, 0x08, "(  )", 4);
					writenum(buf,  5, 0x08, ci.ins, 16, 2, 0);
				}
			}
			if (ci.smp!=0xFFFF)
			{
				if (*samps[ci.smp].name)
					writestring(buf, 34, tcol, samps[ci.smp].name, 17);
				else
				{
					writestring(buf, 34, 0x08, "(    )", 6);
					writenum(buf, 35, 0x08, ci.smp, 16, 4, 0);
				}
			}
			writestring(buf, 53, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 56, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 58, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 60, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			writestring(buf, 62, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			writestring(buf, 63, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=fxstr12[ci.fx];
			if (fxstr)
				writestring(buf, 65, tcol, fxstr, 12);
			drawlongvolbar(buf+80, i, st);
			break;
		case 44:
			writenum(buf,  4, tcol, ci.ins, 16, 2, 0);
			writestring(buf,  7, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 10, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 12, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 14, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			writestring(buf, 16, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			writestring(buf, 17, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=fxstr6[ci.fx];
			if (fxstr)
				writestring(buf, 19, tcol, fxstr, 6);
			drawvolbar(buf+26, i, st);
			break;
	}
}

/************************************************************************/

static int itpGetDots(struct notedotsdata *d, int max)
{
	int i,j;
	int pos=0;
	/* mod.nchan == cpiSessionAPI->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		if (pos>=max)
			break;
		j=0;
		while (pos<max)
		{
			int smp, voll, volr, note, sus;
			j=getdotsdata(&itplayer, i, j, &smp, &note, &voll, &volr, &sus);
			if (j==-1)
				break;
			d[pos].voll=voll;
			d[pos].volr=volr;
			d[pos].chan=i;
			d[pos].note=note;
			d[pos].col=(smp&15)+(sus?32:16);
			pos++;
		}
	}
  return pos;
}

static void itpMute(int i, int m)
{
	mutechan(&itplayer, i, m);
}

static int itpGetLChanSample(unsigned int ch, int16_t *buf, unsigned int len, uint32_t rate, int opt)
{
	return getchansample(&itplayer, ch, buf, len, rate, opt);
}

static int itpOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *loader, struct cpifaceSessionAPI_t *cpiSessionAPI) /* no loader needed/used by this plugin */
{
	const char *filename;
	int retval;
	int nch;

	if (!file)
		return errFileOpen;

	mdbdata = *info;

	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s (%uk)...\n", filename, (unsigned int)(file->filesize(file)>>10));
	utf8_XdotY_name ( 8, 3, utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, utf8_16_dot_3, filename);

	if (!(retval=it_load(&mod, file)))
		if (!loadsamples(&mod))
			retval=-1;

	if (retval)
	{
		it_free(&mod);
		return -1;
	}

	it_optimizepatlens(&mod);

	nch=cfGetProfileInt2(cfSoundSec, "sound", "itchan", 64, 10);
	if (!play(&itplayer, &mod, nch, file, cpiSessionAPI))
		retval=errPlay;

	if (retval)
	{
		it_free(&mod);
		return retval;
	}

	insts=mod.instruments;
	samps=mod.samples;
	plIsEnd=itpLooped;
	plIdle=itpIdle;
	plProcessKey=itpProcessKey;
	plDrawGStrings=itpDrawGStrings;
	plSetMute=itpMute;
	plGetLChanSample=itpGetLChanSample;
	cpiSessionAPI->LogicalChannelCount = mod.nchan;
	cpiSessionAPI->PhysicalChannelCount = mcpNChan;
	plUseDots(itpGetDots);
	plUseChannels(drawchannel);
	itpInstSetup(mod.instruments, mod.ninst, mod.samples, mod.nsamp, mod.sampleinfos, /*mod.nsampi,*/ 0, itpMarkInsSamp);
	itTrkSetup(&mod);
	if (mod.message)
		plUseMessage(mod.message);

	plGetPChanSample=mcpGetChanSample;

	starttime=dos_clock();
	plPause=0;
	mcpSet(-1, mcpMasterPause, 0);
	pausefadedirect=0;

	return errOk;
}

struct cpifaceplayerstruct itpPlayer = {"[ImpulseTracker plugin]", itpOpenFile, itpCloseFile};
struct linkinfostruct dllextinfo = {.name = "playit", .desc = "OpenCP IT Player (c) 1997-'22 Tammo Hinrichs, Niklas Beisert, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
