/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay interface routines
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 *    -removed mcp "restricted" flag (theres no point in rendering XM files
 *     to disk in mono if FT is able to do this in stereo anyway ;)
 *    -finally, added all the screen output we all waited for since november
 *     1996 :)
 *  -ss040326   Stian Skjelstad <stian@nixia.no>
 *    -don't length optimize pats if load failed (and memory is freed)
 *  -ss040709   Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing, and not cputime/clock()
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/deviwave.h"
#include "dev/mcp.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "xmplay.h"

#define _MAX_FNAME 8
#define _MAX_EXT 4

static struct xmodule mod;

static const char *modname;
static const char *composer;

static char currentmodname[_MAX_FNAME+1];
static char currentmodext[_MAX_EXT+1];
static time_t starttime;
static time_t pausetime;

static struct xmpinstrument *insts;
static struct xmpsample *samps;

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
			mcpSet(-1, mcpMasterPause, plPause=1);
			plChanChanged=1;
			mcpSetFadePars(64);
			return;
		}
	}
	pausefaderelspeed=i;
	mcpSetFadePars(i);
}


static int xmpProcessKey(uint16_t key)
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
			mcpSetProcessKey(key);
			if (mcpProcessKey)
				mcpProcessKey(key);
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
			plChanChanged=1;
			break;
#if 0
		case 0x7700: //ctrl-home TODO keys
			xmpInstClear();
			xmpSetPos(0, 0);
			if (plPause)
				starttime=pausetime;
			else
				starttime=dos_clock();
			break;
#endif
		case '<':
		case KEY_CTRL_LEFT:
		/* case 0x7300: //ctrl-left */
			p=xmpGetPos();
			pat=p>>8;
			xmpSetPos(pat-1, 0);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
		/* case 0x7400: //ctrl-right */
			p=xmpGetPos();
			pat=p>>8;
			xmpSetPos(pat+1, 0);
			break;
		case KEY_CTRL_UP:
		/* case 0x8D00: //ctrl-up */
			p=xmpGetPos();
			pat=p>>8;
			row=p&0xFF;
			xmpSetPos(pat, row-8);
			break;
		case KEY_CTRL_DOWN:
		/* case 0x9100: //ctrl-down */
			p=xmpGetPos();
			pat=p>>8;
			row=p&0xFF;
			xmpSetPos(pat, row+8);
			break;
		default:
			if (mcpSetProcessKey(key))
				return 1;
			if (mcpProcessKey)
			{
				int ret=mcpProcessKey(key);
				if (ret==2)
					cpiResetScreen();
				if (ret)
					return 1;
			}
	}
	return 1;
}

static int xmpLooped(void)
{
	return !fsLoopMods&&xmpLoop();
}

static void xmpIdle(void)
{
	xmpSetLoop(fsLoopMods);
	if (mcpIdle)
		mcpIdle();
	if (pausefadedirect)
		dopausefade();
}

static void xmpDrawGStrings(unsigned short (*buf)[CONSOLE_MAX_X])
{
	int pos=xmpGetRealPos();
	int gvol,bpm,tmp;
	unsigned long tim;
	struct xmpglobinfo gi;

	mcpDrawGStrings(buf);

	xmpGetGlobInfo(&tmp, &bpm, &gvol);
	xmpGetGlobInfo2(&gi);
	if (plPause)
		tim=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim=(dos_clock()-starttime)/DOS_CLK_TCK;

	if (plScrWidth<128)
	{
		memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

		writestring(buf[1],  0, 0x09, " row: ../..  ord: .../...  tempo: ..  bpm: ...  gvol: ..\xfa ", 58);
		writenum(buf[1],  6, 0x0F, (pos>>8)&0xFF, 16, 2, 0);
		writenum(buf[1],  9, 0x0F, mod.patlens[mod.orders[(pos>>16)&0xFF]]-1, 16, 2, 0);
		writenum(buf[1], 18, 0x0F, (pos>>16)&0xFF, 16, 3, 0);
		writenum(buf[1], 22, 0x0F, mod.nord-1, 16, 3, 0);
		writenum(buf[1], 34, 0x0F, tmp, 16, 2, 1);
		writenum(buf[1], 43, 0x0F, bpm, 10, 3, 1);
		writenum(buf[1], 54, 0x0F, gvol, 16, 2, 0);
		writestring(buf[1], 56, 0x0F, (gi.globvolslide==xfxGVSUp)?"\x18":(gi.globvolslide==xfxGVSDown)?"\x19":" ", 1);
		writestring(buf[2],  0, 0x09, " module \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................               time: ..:.. ", 80);
		writestring(buf[2],  8, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 16, 0x0F, currentmodext, _MAX_EXT);
#warning modname is is now UTF-8
		writestring(buf[2], 22, 0x0F, modname, 31);
		if (plPause)
			writestring(buf[2], 58, 0x0C, "paused", 6);
		writenum(buf[2], 74, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 76, 0x0F, ":", 1);
		writenum(buf[2], 77, 0x0F, tim%60, 10, 2, 0);
	} else {
		memset(buf[0]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[1]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

		writestring(buf[1],  0, 0x09, "    row: ../..  order: .../...   tempo: ..  speed/bpm: ...   global volume: ..\xfa  ", 81);
		writenum(buf[1],  9, 0x0F, (pos>>8)&0xFF, 16, 2, 0);
		writenum(buf[1], 12, 0x0F, mod.patlens[mod.orders[(pos>>16)&0xFF]]-1, 16, 2, 0);
		writenum(buf[1], 23, 0x0F, (pos>>16)&0xFF, 16, 3, 0);
		writenum(buf[1], 27, 0x0F, mod.nord-1, 16, 3, 0);
		writenum(buf[1], 40, 0x0F, tmp, 16, 2, 1);
		writenum(buf[1], 55, 0x0F, bpm, 10, 3, 1);
		writenum(buf[1], 76, 0x0F, gvol, 16, 2, 0);
		writestring(buf[1], 78, 0x0F, (gi.globvolslide==xfxGVSUp)?"\x18":(gi.globvolslide==xfxGVSDown)?"\x19":" ", 1);
		writestring(buf[2],  0, 0x09, "    module \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa.\xfa\xfa\xfa: ...............................  composer: ...............................                  time: ..:..    ", 132);
		writestring(buf[2], 11, 0x0F, currentmodname, _MAX_FNAME);
		writestring(buf[2], 19, 0x0F, currentmodext, _MAX_EXT);
#warning modname and composer is now UTF-8
		writestring(buf[2], 25, 0x0F, modname, 31);
		writestring(buf[2], 68, 0x0F, composer, 31);
		if (plPause)
			writestring(buf[2], 100, 0x0C, "playback paused", 15);
		writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
		writestring(buf[2], 125, 0x0F, ":", 1);
		writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
	}
}

static void xmpCloseFile(void)
{
	xmpStopModule();
#ifdef RESTRICTED
	mcpSet(-1, mcpGRestrict, 0);
#endif
	xmpFreeModule(&mod);
}

/***********************************************************************/

static void xmpMarkInsSamp(char *ins, char *smp)
{
	int i;
	int in, sm;

	for (i=0; i<plNLChan; i++)
	{
		if (!xmpChanActive(i)||plMuteCh[i])
			continue;
		in=xmpGetChanIns(i);
		sm=xmpGetChanSamp(i);
		ins[in-1]=((plSelCh==i)||(ins[in-1]==3))?3:2;
		smp[sm]=((plSelCh==i)||(smp[sm]==3))?3:2;
	}
}

/*************************************************************************/

static void logvolbar(int *l, int *r)
{
	if (*l>32)
		*l=32+((*l-32)>>1);
	if (*l>48)
		*l=48+((*l-48)>>1);
	if (*l>56)
		*l=56+((*l-56)>>1);
	if (*l>64)
		*l=64;
	if (*r>32)
		*r=32+((*r-32)>>1);
	if (*r>48)
		*r=48+((*r-48)>>1);
	if (*r>56)
		*r=56+((*r-56)>>1);
	if (*r>64)
		*r=64;
}

static void drawvolbar(unsigned short *buf, int i, unsigned char st)
{
	int l,r;
	xmpGetRealVolume(i, &l, &r);
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

static void drawlongvolbar(unsigned short *buf, int i, unsigned char st)
{
	int l,r;
	xmpGetRealVolume(i, &l, &r);
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

static char *getfxstr6(unsigned char fx)
{
	switch (fx)
	{
		case xfxVolSlideUp: return "volsl\x18";
		case xfxVolSlideDown: return "volsl\x19";
		case xfxRowVolSlideUp: return "fvols\x18";
		case xfxRowVolSlideDown: return "fvols\x19";
		case xfxPitchSlideUp: return "porta\x18";
		case xfxPitchSlideDown: return "porta\x19";
		case xfxPitchSlideToNote: return "porta\x0d";
		case xfxRowPitchSlideUp: return "fport\x18";
		case xfxRowPitchSlideDown: return "fport\x19";
		case xfxPanSlideRight: return "pansl\x1A";
		case xfxPanSlideLeft: return "pansl\x1B";
		case xfxVolVibrato: return "tremol";
		case xfxTremor: return "tremor";
		case xfxPitchVibrato: return "vibrat";
		case xfxArpeggio: return "arpegg";
		case xfxNoteCut: return " \x0e""cut ";
		case xfxRetrig: return "retrig";
		case xfxOffset: return "offset";
		case xfxDelay: return "\x0e""delay";
		case xfxEnvPos: return "envpos";
		case xfxSetFinetune: return "set ft";
		default: return 0;
	}
}

static char *getfxstr15(unsigned char fx)
{
	switch (fx)
	{
		case xfxVolSlideUp: return "volume slide \x18";
		case xfxVolSlideDown: return "volume slide \x19";
		case xfxPanSlideRight: return "panning slide \x1A";
		case xfxPanSlideLeft: return "panning slide \x1B";
		case xfxRowVolSlideUp: return "fine volslide \x18";
		case xfxRowVolSlideDown: return "fine volslide \x19";
		case xfxPitchSlideUp: return "portamento \x18";
		case xfxPitchSlideDown: return "portamento \x19";
		case xfxRowPitchSlideUp: return "fine porta \x18";
		case xfxRowPitchSlideDown: return "fine porta \x19";
		case xfxPitchSlideToNote: return "portamento to \x0d";
		case xfxTremor: return "tremor";
		case xfxPitchVibrato: return "vibrato";
		case xfxVolVibrato: return "tremolo";
		case xfxArpeggio: return "arpeggio";
		case xfxNoteCut: return "note cut";
		case xfxRetrig: return "retrigger";
		case xfxOffset: return "sample offset";
		case xfxDelay: return "delay";
		case xfxEnvPos: return "set env pos'n";
		case xfxSetFinetune: return "set finetune";
		default: return 0;
	}
}



static void drawchannel(uint16_t *buf, int len, int i)
{
	unsigned char st=plMuteCh[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
	unsigned char tcolr=st?0x08:0x0B;

	int ins, smp;
	char *fxstr;
	struct xmpchaninfo ci;

	switch (len)
	{
		case 36:
			writestring(buf, 0, tcold, " -- --- -- ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 36);
			break;
		case 62:
			writestring(buf, 0, tcold, "                        ---\xfa --\xfa -\xfa ------  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 62);
			break;
		case 128:
			writestring(buf,  0, tcold, "                             \xb3                   \xb3    \xb3   \xb3  \xb3               \xb3  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 128);
			break;
		case 76:
			writestring(buf,  0, tcold, "                             \xb3    \xb3   \xb3  \xb3               \xb3 \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 76);
			break;
		case 44:
			writestring(buf, 0, tcold, " --  ---\xfa --\xfa -\xfa ------   \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 44);
			break;
	}

	if (!xmpChanActive(i))
		return;

	ins=xmpGetChanIns(i);
	smp=xmpGetChanSamp(i);
	xmpGetChanInfo(i, &ci);
	switch (len)
	{
		case 36:
			writenum(buf,  1, tcol, ins, 16, 2, 0);
			writestring(buf,  4, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writenum(buf, 8, tcol, ci.vol, 16, 2, 0);
			fxstr=getfxstr6(ci.fx);
			if (fxstr)
				writestring(buf, 11, tcol, fxstr, 6);
			drawvolbar(buf+18, i, st);
			break;
		case 62:
			if (ins) {
				if (*insts[ins-1].name)
					writestring(buf,  1, tcol, insts[ins-1].name, 21);
				else {
					writestring(buf,  1, 0x08, "(  )", 4);
					writenum(buf,  2, 0x08, ins, 16, 2, 0);
				}
			}
			writestring(buf, 24, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 27, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 29, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 31, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			writestring(buf, 33, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
			writestring(buf, 34, tcol, &" \x1A\x1B"[ci.panslide], 1);
			fxstr=getfxstr6(ci.fx);
			if (fxstr)
				writestring(buf, 36, tcol, fxstr, 6);
			drawvolbar(buf+44, i, st);
			break;
		case 76:
			if (ins)
			{
				if (*insts[ins-1].name)
					writestring(buf,  1, tcol, insts[ins-1].name, 28);
				else {
					writestring(buf,  1, 0x08, "(  )", 4);
					writenum(buf,  2, 0x08, ins, 16, 2, 0);
				}
			}
			writestring(buf, 30, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 33, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 35, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 37, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			writestring(buf, 39, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
			writestring(buf, 40, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=getfxstr15(ci.fx);
			if (fxstr)
				writestring(buf, 42, tcol, fxstr, 15);

			drawvolbar(buf+59, i, st);
			break;
		case 128:
			if (ins)
			{
				if (*insts[ins-1].name)
					writestring(buf,  1, tcol, insts[ins-1].name, 28);
				else {
					writestring(buf,  1, 0x08, "(  )", 4);
					writenum(buf,  2, 0x08, ins, 16, 2, 0);
				}
			}
			if (smp!=0xFFFF)
			{
				if (*samps[smp].name)
					writestring(buf, 31, tcol, samps[smp].name, 17);
				else {
					writestring(buf, 31, 0x08, "(    )", 6);
					writenum(buf, 32, 0x08, smp, 16, 4, 0);
				}
			}
			writestring(buf, 50, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 53, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 55, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 57, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide]: &" ~"[ci.volfx], 1);
			writestring(buf, 59, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
			writestring(buf, 60, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=getfxstr15(ci.fx);
			if (fxstr)
				writestring(buf, 62, tcol, fxstr, 15);
			drawlongvolbar(buf+80, i, st);
			break;
		case 44:
			writenum(buf,  1, tcol, xmpGetChanIns(i), 16, 2, 0);
			writestring(buf,  5, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
			writestring(buf, 8, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			writenum(buf, 10, tcol, ci.vol, 16, 2, 0);
			writestring(buf, 12, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			writestring(buf, 14, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
			writestring(buf, 15, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=getfxstr6(ci.fx);
			if (fxstr)
				writestring(buf, 17, tcol, fxstr, 6);
			drawvolbar(buf+26, i, st);
			break;
	}
}

/*************************************************************************/

static int xmpGetDots(struct notedotsdata *d, int max)
{
	int pos=0;
	int i;

	int smp,frq,voll,volr,sus;

	for (i=0; i<plNLChan; i++)
	{
		if (pos>=max)
			break;
		if (!xmpGetDotsData(i, &smp, &frq, &voll, &volr, &sus))
			continue;
		d[pos].voll=voll;
		d[pos].volr=volr;
		d[pos].chan=i;
		d[pos].note=frq;
		d[pos].col=(sus?32:16)+(smp&15);
		pos++;
	}
	return pos;
}

static int xmpOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *file, const char *ldlink, const char *_loader) /* no loader needed/used by this plugin */
{
	int (*loader)(struct xmodule *, struct ocpfilehandle_t *)=0;
	int retval;

	if (!mcpOpenPlayer)
		return errGen;

	if (!file)
		return errFileOpen;

#warning currentmodname currentmodext
	//strncpy(currentmodname, info->name, _MAX_FNAME);
	//strncpy(currentmodext, info->name + _MAX_FNAME, _MAX_EXT);

	fprintf(stderr, "loading %s%s (%uk)...\n", currentmodname, currentmodext, (unsigned int)(file->filesize (file) >> 10));

	     if (info->modtype.integer.i == MODULETYPE("XM"))   loader=xmpLoadModule;
	else if (info->modtype.integer.i == MODULETYPE("MOD"))  loader=xmpLoadMOD;
	else if (info->modtype.integer.i == MODULETYPE("MODt")) loader=xmpLoadMODt;
	else if (info->modtype.integer.i == MODULETYPE("MODd")) loader=xmpLoadMODd;
	else if (info->modtype.integer.i == MODULETYPE("M31"))  loader=xmpLoadM31;
	else if (info->modtype.integer.i == MODULETYPE("M15"))  loader=xmpLoadM15;
	else if (info->modtype.integer.i == MODULETYPE("M15t")) loader=xmpLoadM15t;
	else if (info->modtype.integer.i == MODULETYPE("WOW"))  loader=xmpLoadWOW;
	else if (info->modtype.integer.i == MODULETYPE("MXM"))  loader=xmpLoadMXM;
	else if (info->modtype.integer.i == MODULETYPE("MODf")) loader=xmpLoadMODf;

	if (!loader)
		return errFormStruc;

	if (!(retval=loader(&mod, file)))
		if (!xmpLoadSamples(&mod))
			retval=-1;

/*
	fclose(file);   Parent does this for us */

	if (retval)
	{
		xmpFreeModule(&mod);
		return -1;
	}

	xmpOptimizePatLens(&mod);

	mcpNormalize(1);
	if (!xmpPlayModule(&mod, file))
		retval=errPlay;

	if (retval)
	{
		xmpFreeModule(&mod);
		return retval;
	}

	insts=mod.instruments;
	samps=mod.samples;
	plNLChan=mod.nchan;

	plIsEnd=xmpLooped;
	plIdle=xmpIdle;
	plProcessKey=xmpProcessKey;
	plDrawGStrings=xmpDrawGStrings;
	plSetMute=xmpMute;
	plGetLChanSample=xmpGetLChanSample;

	plUseDots(xmpGetDots);

	plUseChannels(drawchannel);

	xmpInstSetup(mod.instruments, mod.ninst, mod.samples, mod.nsamp, mod.sampleinfos, mod.nsampi, 0, xmpMarkInsSamp);
	xmTrkSetup(&mod);

	plNPChan=mcpNChan;
#warning TODO, this is not UTF-8 at the moment.....
	modname=mod.name;
	composer="";
	if (!plCompoMode)
	{
		if (!*modname)
			modname=info->title;
		composer=info->composer;
	} else
		modname=info->comment;

	plGetRealMasterVolume=mcpGetRealMasterVolume;
	plGetMasterSample=mcpGetMasterSample;
	plGetPChanSample=mcpGetChanSample;


	starttime=dos_clock();
	plPause=0;
	mcpSet(-1, mcpMasterPause, 0);
	pausefadedirect=0;

	return errOk;
}

struct cpifaceplayerstruct xmpPlayer = {xmpOpenFile, xmpCloseFile};
struct linkinfostruct dllextinfo = {.name = "playxm", .desc = "OpenCP XM/MOD Player (c) 1995-21 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
