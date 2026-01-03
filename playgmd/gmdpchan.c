/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay channel display routines
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
 */

#include "config.h"
#include "types.h"
#include "gmdplay.h"
#include "gmdpchan.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

static const struct gmdinstrument *plChanInstr;
static const struct gmdsample *plChanModSamples;

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
	mpGetRealVolume (cpifaceSession, i, &l, &r);
	logvolbar(&l, &r);

	l=(l+4)>>3;
	r=(r+4)>>3;
	if (cpifaceSession->InPause)
	{
		l=r=0;
	}
	if (st)
	{
		cpifaceSession->console->WriteString (buf, 8-l, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", l);
		cpifaceSession->console->WriteString (buf, 9  , 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		const uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
		const uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 8-l, left+8-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 9  , right   , r);
	}
}

static void drawlongvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, unsigned char st)
{
	int l,r;
	mpGetRealVolume (cpifaceSession, i, &l, &r);
	logvolbar(&l, &r);
	l=(l+2)>>2;
	r=(r+2)>>2;
	if (cpifaceSession->InPause)
	{
		l=r=0;
	}
	if (st)
	{
		cpifaceSession->console->WriteString (buf, 16-l, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", l);
		cpifaceSession->console->WriteString (buf, 17  , 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		const uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		const uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 16-l, left+16-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 17  , right    , r);
	}
}

static char *getfxstr6(unsigned char fx)
{
	switch (fx)
	{
		case fxVolSlideUp:
			return "volsl\x18";
		case fxVolSlideDown:
			return "volsl\x19";
		case fxPanSlideRight:
			return "pansl\x1A";
		case fxPanSlideLeft:
			return "pansl\x1B";
		case fxRowVolSlideUp:
			return "fvols\x18";
		case fxRowVolSlideDown:
			return "fvols\x19";
		case fxPitchSlideUp:
			return "porta\x18";
		case fxPitchSlideDown:
			return "porta\x19";
		case fxRowPitchSlideUp:
			return "fport\x18";
		case fxRowPitchSlideDown:
			return "fport\x19";
		case fxPitchSlideToNote:
			return "porta\x0d";
		case fxVolVibrato:
			return "tremol";
		case fxTremor:
			return "tremor";
		case fxPitchVibrato:
			return "vibrat";
		case fxArpeggio:
			return "arpegg";
		case fxNoteCut:
			return " \x0e""cut ";
		case fxRetrig:
			return "retrig";
		case fxOffset:
			return "offset";
		case fxDelay:
			return "delay ";
		default: return 0;
	}
}

static char *getfxstr15(unsigned char fx)
{
	switch (fx)
	{
		case fxVolSlideUp:
			return "volume slide \x18";
		case fxVolSlideDown:
			return "volume slide \x19";
		case fxPanSlideRight:
			return "panning slide \x1A";
		case fxPanSlideLeft:
			return "panning slide \x1B";
		case fxRowVolSlideUp:
			return "fine volslide \x18";
		case fxRowVolSlideDown:
			return "fine volslide \x19";
		case fxPitchSlideUp:
			return "portamento \x18";
		case fxPitchSlideDown:
			return "portamento \x19";
		case fxRowPitchSlideUp:
			return "fine porta \x18";
		case fxRowPitchSlideDown:
			return "fine porta \x19";
		case fxPitchSlideToNote:
			return "portamento to \x0d";
		case fxTremor:
			return "tremor";
		case fxPitchVibrato:
			return "vibrato";
		case fxVolVibrato:
			return "tremolo";
		case fxArpeggio:
			return "arpeggio";
		case fxNoteCut:
			return "note cut";
		case fxRetrig:
			return "retrigger";
		case fxOffset:
			return "sample offset";
		case fxDelay:
			return "delay";
		default:
			return 0;
	}
}


static void drawchannel36 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct chaninfo ci;
	unsigned char st = cpifaceSession->MuteChannel[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
	unsigned char tcolr=st?0x08:0x0B;

	mpGetChanInfo(i, &ci);

	cpifaceSession->console->WriteString (buf, 0, tcold, " -- --- -- ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 36);
	if (mpGetChanStatus (cpifaceSession, i) && ci.vol)
	{
		char *fxstr;

		cpifaceSession->console->WriteNum    (buf, 1, tcol, ci.ins+1, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 4, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
		cpifaceSession->console->WriteNum    (buf, 8, tcol, ci.vol, 16, 2, 0);
		fxstr=getfxstr6(ci.fx);
		if (fxstr)
			cpifaceSession->console->WriteString (buf, 11, tcol, fxstr, 6);
		drawvolbar (cpifaceSession, buf+18, i, st);
	}
}

static void drawchannel62 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct chaninfo ci;
	unsigned char st = cpifaceSession->MuteChannel[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
	unsigned char tcolr=st?0x08:0x0B;

	mpGetChanInfo(i, &ci);

	cpifaceSession->console->WriteString (buf, 0, tcold, "                        ---\xfa --\xfa -\xfa ------  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 62);
	if (mpGetChanStatus (cpifaceSession, i) && ci.vol)
	{
		char *fxstr;
		if (ci.ins!=0xFF)
		{
			if ((*plChanInstr[ci.ins].name) && (!compoMode))
			{
				cpifaceSession->console->WriteString (buf, 1, tcol, plChanInstr[ci.ins].name, 21);
			} else {
				cpifaceSession->console->WriteString (buf, 1, 0x08, "(  )", 4);
				cpifaceSession->console->WriteNum    (buf, 2, 0x08, ci.ins+1, 16, 2, 0);
			}
		}
		cpifaceSession->console->WriteString (buf, 24, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
		cpifaceSession->console->WriteString (buf, 27, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
		cpifaceSession->console->WriteNum    (buf, 29, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 31, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
		cpifaceSession->console->WriteString (buf, 33, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
		cpifaceSession->console->WriteString (buf, 34, tcol, &" \x1A\x1B"[ci.panslide], 1);
		fxstr=getfxstr6(ci.fx);
		if (fxstr)
			cpifaceSession->console->WriteString (buf, 36, tcol, fxstr, 6);
		drawvolbar (cpifaceSession, buf+44, i, st);
	}
}

static void drawchannel76 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct chaninfo ci;
	unsigned char st = cpifaceSession->MuteChannel[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
	unsigned char tcolr=st?0x08:0x0B;

	mpGetChanInfo(i, &ci);

	cpifaceSession->console->WriteString (buf,  0, tcold, "                             \xb3    \xb3   \xb3  \xb3               \xb3 \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 76);
	if (mpGetChanStatus (cpifaceSession, i) && ci.vol)
	{
		char *fxstr;
		if (ci.ins!=0xFF)
		{
			if ((*plChanInstr[ci.ins].name) && (!compoMode))
			{
				cpifaceSession->console->WriteString (buf, 1, tcol, plChanInstr[ci.ins].name, 28);
			} else {
				cpifaceSession->console->WriteString (buf, 1, 0x08, "(  )", 4);
				cpifaceSession->console->WriteNum    (buf, 2, 0x08, ci.ins+1, 16, 2, 0);
			}
		}
		cpifaceSession->console->WriteString (buf, 30, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
		cpifaceSession->console->WriteString (buf, 33, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
		cpifaceSession->console->WriteNum    (buf, 35, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 37, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
		cpifaceSession->console->WriteString (buf, 39, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
		cpifaceSession->console->WriteString (buf, 40, tcol, &" \x1A\x1B"[ci.panslide], 1);

		fxstr=getfxstr15(ci.fx);
		if (fxstr)
			cpifaceSession->console->WriteString (buf, 42, tcol, fxstr, 15);

		drawvolbar (cpifaceSession, buf+59, i, st);
	}
}

static void drawchannel128 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct chaninfo ci;
	unsigned char st = cpifaceSession->MuteChannel[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
	unsigned char tcolr=st?0x08:0x0B;

	mpGetChanInfo(i, &ci);

	cpifaceSession->console->WriteString (buf,  0, tcold, "                             \xb3                   \xb3    \xb3   \xb3  \xb3               \xb3  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 128);

	if (mpGetChanStatus (cpifaceSession, i) && ci.vol)
	{
		char *fxstr;
		if (ci.ins!=0xFF)
		{
			if ((*plChanInstr[ci.ins].name) && (!compoMode))
			{
				cpifaceSession->console->WriteString (buf, 1, tcol, plChanInstr[ci.ins].name, 28);
			} else {
				cpifaceSession->console->WriteString (buf, 1, 0x08, "(  )", 4);
				cpifaceSession->console->WriteNum    (buf, 2, 0x08, ci.ins+1, 16, 2, 0);
			}
		}
		if (ci.smp!=0xFFFF)
		{
			if ((*plChanModSamples[ci.smp].name) && (!compoMode))
			{
				cpifaceSession->console->WriteString (buf, 31, tcol, plChanModSamples[ci.smp].name, 17);
			} else {
				cpifaceSession->console->WriteString (buf, 31, 0x08, "(    )", 6);
				cpifaceSession->console->WriteNum    (buf, 32, 0x08, ci.smp, 16, 4, 0);
			}
		}
		cpifaceSession->console->WriteString (buf, 50, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
		cpifaceSession->console->WriteString (buf, 53, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
		cpifaceSession->console->WriteNum    (buf, 55, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 57, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
		cpifaceSession->console->WriteString (buf, 59, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
		cpifaceSession->console->WriteString (buf, 60, tcol, &" \x1A\x1B"[ci.panslide], 1);

		fxstr=getfxstr15(ci.fx);
		if (fxstr)
			cpifaceSession->console->WriteString (buf, 62, tcol, fxstr, 15);

		drawlongvolbar (cpifaceSession, buf+80, i, st);
	}
}

static void drawchannel44 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct chaninfo ci;
	unsigned char st = cpifaceSession->MuteChannel[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
	unsigned char tcolr=st?0x08:0x0B;

	mpGetChanInfo(i, &ci);

	cpifaceSession->console->WriteString (buf, 0, tcold, " --  ---\xfa --\xfa -\xfa ------   \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 44);
	if (mpGetChanStatus (cpifaceSession, i) && ci.vol)
	{
		char *fxstr;
		cpifaceSession->console->WriteNum    (buf,  1, tcol, ci.ins+1, 16, 2, 0);
		cpifaceSession->console->WriteString (buf,  5, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
		cpifaceSession->console->WriteString (buf,  8, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
		cpifaceSession->console->WriteNum    (buf, 10, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 12, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
		cpifaceSession->console->WriteString (buf, 14, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
		cpifaceSession->console->WriteString (buf, 15, tcol, &" \x1A\x1B"[ci.panslide], 1);

		fxstr=getfxstr6(ci.fx);
		if (fxstr)
			cpifaceSession->console->WriteString (buf, 17, tcol, fxstr, 6);
		drawvolbar (cpifaceSession, buf+26, i, st);
	}
}

static void drawchannel (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode)
{
	switch (width)
	{
		case cpiChanWidth_36:
			drawchannel36 (cpifaceSession, buf, i, compoMode);
			break;
		case cpiChanWidth_44:
			drawchannel44 (cpifaceSession, buf, i, compoMode);
			break;
		case cpiChanWidth_62:
			drawchannel62 (cpifaceSession, buf, i, compoMode);
			break;
		case cpiChanWidth_76:
			drawchannel76 (cpifaceSession, buf, i, compoMode);
			break;
		case cpiChanWidth_128:
			drawchannel128 (cpifaceSession, buf, i, compoMode);
			break;
	}
}

OCP_INTERNAL void gmdChanSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct gmdmodule *mod)
{
	plChanInstr=mod->instruments;
	plChanModSamples=mod->modsamples;
	cpifaceSession->UseChannels (cpifaceSession, drawchannel);
}
