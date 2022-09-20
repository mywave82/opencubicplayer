/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "xmplay.h"
#include "xmtype.h"

__attribute__ ((visibility ("internal"))) struct xmodule mod;

static time_t starttime;      /* when did the song start, if paused, this is slided if unpaused */
static time_t pausetime;      /* when did the pause start (fully paused) */
static time_t pausefadestart; /* when did the pause fade start, used to make the slide */
static int8_t pausefadedirection; /* 0 = no slide, +1 = sliding from pause to normal, -1 = sliding from normal to pause */

static struct xmpinstrument *insts;
static struct xmpsample *samps;

static void togglepausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (pausefadedirection)
	{ /* we are already in a pause-fade, reset the fade-start point */
		pausefadestart = clock_ms() - 1000 + (clock_ms() - pausefadestart);
		pausefadedirection *= -1; /* inverse the direction */
	} else if (cpifaceSession->InPause)
	{ /* we are in full pause already */
		pausefadestart = clock_ms();
		starttime = starttime + pausefadestart - pausetime; /* we are unpausing, so push starttime the amount we have been paused */
		cpifaceSession->mcpSet (-1, mcpMasterPause, cpifaceSession->InPause = 0);
		pausefadedirection = 1;
	} else { /* we were not in pause, start the pause fade */
		pausefadestart = clock_ms();
		pausefadedirection = -1;
	}
}

static void dopausefade (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int16_t i;
	if (pausefadedirection > 0)
	{ /* unpause fade */
		i = ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i < 1)
		{
			i = 1;
		}
		if (i >= 64)
		{
			i = 64;
			pausefadedirection = 0; /* we reached the end of the slide */
		}
	} else { /* pause fade */
		i = 64 - ((int_fast32_t)(clock_ms() - pausefadestart)) * 64 / 1000;
		if (i >= 64)
		{
			i = 64;
		}
		if (i <= 0)
		{ /* we reached the end of the slide, finish the pause command */
			pausefadedirection = 0;
			pausetime = clock_ms();
			cpifaceSession->mcpSet (-1, mcpMasterPause, cpifaceSession->InPause = 1);
			return;
		}
	}
	cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, i);
}


static int xmpProcessKey(struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	int row;
	int pat, p;

	switch (key)
	{
		case KEY_ALT_K:
			cpifaceSession->KeyHelp ('p', "Start/stop pause with fade");
			cpifaceSession->KeyHelp ('P', "Start/stop pause with fade");
			cpifaceSession->KeyHelp (KEY_CTRL_P, "Start/stop pause");
			cpifaceSession->KeyHelp ('<', "Jump back (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_LEFT, "Jump back (big)");
			cpifaceSession->KeyHelp ('>', "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_RIGHT, "Jump forward (big)");
			cpifaceSession->KeyHelp (KEY_CTRL_UP, "Jump back (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_DOWN, "Jump forward (small)");
			cpifaceSession->KeyHelp (KEY_CTRL_HOME, "Jump to start of track");
			return 0;
		case 'p': case 'P':
			togglepausefade (cpifaceSession);
			break;
		case KEY_CTRL_P:
			/* cancel any pause-fade that might be in progress */
			pausefadedirection = 0;
			cpifaceSession->mcpAPI->SetMasterPauseFadeParameters (cpifaceSession, 64);

			if (cpifaceSession->InPause)
			{
				starttime = starttime + clock_ms() - pausetime; /* we are unpausing, so push starttime for the amount we have been paused */
			} else {
				pausetime = clock_ms();
			}
			cpifaceSession->InPause = !cpifaceSession->InPause;
			cpifaceSession->mcpSet (-1, mcpMasterPause, cpifaceSession->InPause);
			break;
		case KEY_CTRL_HOME:
			xmpInstClear (cpifaceSession);
			xmpSetPos (cpifaceSession, 0, 0);
			if (cpifaceSession->InPause)
			{
				starttime = pausetime;
			} else {
				starttime = clock_ms();
			}
			break;
		case '<':
		case KEY_CTRL_LEFT:
			p=xmpGetPos();
			pat=p>>8;
			xmpSetPos (cpifaceSession, pat-1, 0);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
			p=xmpGetPos();
			pat=p>>8;
			xmpSetPos (cpifaceSession, pat+1, 0);
			break;
		case KEY_CTRL_UP:
			p=xmpGetPos();
			pat=p>>8;
			row=p&0xFF;
			xmpSetPos (cpifaceSession, pat, row-8);
			break;
		case KEY_CTRL_DOWN:
			p=xmpGetPos();
			pat=p>>8;
			row=p&0xFF;
			xmpSetPos (cpifaceSession, pat, row+8);
			break;
		default:
			return 0;
	}
	return 1;
}

static int xmpLooped (struct cpifaceSessionAPI_t *cpifaceSession, int LoopMod)
{
	if (pausefadedirection)
	{
		dopausefade (cpifaceSession);
	}
	xmpSetLoop (LoopMod);
	mcpDevAPI->mcpIdle (cpifaceSession);

	return (!LoopMod) && xmpLoop();
}

static void xmpDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos = xmpGetRealPos (cpifaceSession);
	int gvol,bpm,tmp;
	struct xmpglobinfo gi;

	xmpGetGlobInfo(&tmp, &bpm, &gvol);
	xmpGetGlobInfo2(&gi);

	cpifaceSession->drawHelperAPI->GStringsTracked
	(
		cpifaceSession,
		0,          /* song X */
		0,          /* song Y */
		(pos>>8)&0xFF,/* row X */
		mod.patlens[mod.orders[(pos>>16)&0xFF]]-1,/* row Y */
		(pos>>16)&0xFF,/* order X */
		mod.nord-1, /* order Y */
		tmp,        /* speed */
		bpm,        /* tempo */
		gvol,
		(gi.globvolslide==xfxGVSUp)?1:(gi.globvolslide==xfxGVSDown)?-1:0,
		0,          /* chan X */
		0,          /* chan Y */
		cpifaceSession->InPause ? ((pausetime - starttime) / 1000) : ((clock_ms() - starttime) / 1000)
	);
}

static void xmpCloseFile (struct cpifaceSessionAPI_t *cpifaceSession)
{
	xmpStopModule();
	xmpFreeModule(&mod);
}

/***********************************************************************/

static void xmpMarkInsSamp (struct cpifaceSessionAPI_t *cpifaceSession, char *ins, char *smp)
{
	int i;
	int in, sm;

	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		if (!xmpChanActive (cpifaceSession, i) || cpifaceSession->MuteChannel[i])
			continue;
		in=xmpGetChanIns(i);
		sm=xmpGetChanSamp(i);
		ins[in-1]=((cpifaceSession->SelectedChannel==i)||(ins[in-1]==3))?3:2;
		smp[sm]=((cpifaceSession->SelectedChannel==i)||(smp[sm]==3))?3:2;
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

static void drawvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, unsigned char st)
{
	int l,r;
	xmpGetRealVolume (cpifaceSession, i, &l, &r);
	logvolbar(&l, &r);

	l=(l+4)>>3;
	r=(r+4)>>3;
	if (cpifaceSession->InPause)
	{
		l=r=0;
	}
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

static void drawlongvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, unsigned char st)
{
	int l,r;
	xmpGetRealVolume (cpifaceSession, i, &l, &r);
	logvolbar(&l, &r);
	l=(l+2)>>2;
	r=(r+2)>>2;
	if (cpifaceSession->InPause)
	{
		l=r=0;
	}
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



static void drawchannel (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int len, int i)
{
	unsigned char st = cpifaceSession->MuteChannel[i];

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

	if (!xmpChanActive (cpifaceSession, i))
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
			drawvolbar (cpifaceSession, buf+18, i, st);
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
			drawvolbar (cpifaceSession, buf+44, i, st);
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

			drawvolbar (cpifaceSession, buf+59, i, st);
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
			drawlongvolbar (cpifaceSession, buf+80, i, st);
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
			drawvolbar (cpifaceSession, buf+26, i, st);
			break;
	}
}

/*************************************************************************/

static int xmpGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *d, int max)
{
	int pos=0;
	int i;

	int smp,frq,voll,volr,sus;

	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		if (pos>=max)
			break;
		if (!xmpGetDotsData (cpifaceSession, i, &smp, &frq, &voll, &volr, &sus))
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

static int xmpOpenFile (struct cpifaceSessionAPI_t *cpifaceSession, struct moduleinfostruct *info, struct ocpfilehandle_t *file)
{
	const char *filename;
	int (*loader)(struct xmodule *, struct ocpfilehandle_t *)=0;
	int retval;

	if (!mcpDevAPI->mcpOpenPlayer)
		return errGen;

	if (!file)
		return errFileOpen;

	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	fprintf(stderr, "loading %s (%uk)...\n", filename, (unsigned int)(file->filesize (file) >> 10));

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

	if (!xmpPlayModule(&mod, file, cpifaceSession))
		retval=errPlay;

	if (retval)
	{
		xmpFreeModule(&mod);
		return retval;
	}

	insts=mod.instruments;
	samps=mod.samples;

	cpifaceSession->IsEnd = xmpLooped;
	cpifaceSession->ProcessKey = xmpProcessKey;
	cpifaceSession->DrawGStrings = xmpDrawGStrings;
	cpifaceSession->SetMuteChannel = xmpMute;
	cpifaceSession->GetLChanSample = cpifaceSession->mcpGetChanSample;
	cpifaceSession->GetPChanSample = cpifaceSession->mcpGetChanSample;

	cpifaceSession->LogicalChannelCount = mod.nchan;

	cpifaceSession->UseDots(xmpGetDots);

	cpifaceSession->UseChannels (cpifaceSession, drawchannel);

	xmpInstSetup (cpifaceSession, mod.instruments, mod.ninst, mod.samples, mod.nsamp, mod.sampleinfos, mod.nsampi, 0, xmpMarkInsSamp);
	xmTrkSetup (cpifaceSession, &mod);

	starttime = clock_ms();
	cpifaceSession->InPause = 0;
	cpifaceSession->mcpSet(-1, mcpMasterPause, 0);
	pausefadedirection = 0;

	return errOk;
}

static int xmInit (void)
{
	return xm_type_init ();
}

static void xmClose (void)
{
	xm_type_done ();
}

const struct cpifaceplayerstruct __attribute__((visibility ("internal"))) xmpPlayer = {"[FastTracker II plugin]", xmpOpenFile, xmpCloseFile};
DLLEXTINFO_PLAYBACK_PREFIX struct linkinfostruct dllextinfo = {.name = "playxm", .desc = "OpenCP XM/MOD Player (c) 1995-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 95, .Init = xmInit, .Close = xmClose};
