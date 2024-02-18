/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay channel display routines
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
 */

#include "config.h"
#include "types.h"
#include "cpiface/cpiface.h"
#include "xmchan.h"
#include "xmplay.h"
#include "stuff/poutput.h"

static struct xmpinstrument *insts;
static struct xmpsample *samps;

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
		cpifaceSession->console->WriteString (buf, 8-l, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", l);
		cpifaceSession->console->WriteString (buf, 9, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 8-l, left+8-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 9, right, r);
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
		cpifaceSession->console->WriteString (buf, 16-l, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", l);
		cpifaceSession->console->WriteString (buf, 17, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 16-l, left+16-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 17, right, r);
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



static void drawchannel (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode)
{
	unsigned char st = cpifaceSession->MuteChannel[i];

	unsigned char tcol=st?0x08:0x0F;
	unsigned char tcold=st?0x08:0x07;
	unsigned char tcolr=st?0x08:0x0B;

	int ins, smp;
	char *fxstr;
	struct xmpchaninfo ci;

	switch (width)
	{
		case cpiChanWidth_36:
			cpifaceSession->console->WriteString (buf, 0, tcold, " -- --- -- ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 36);
			break;
		case cpiChanWidth_62:
			cpifaceSession->console->WriteString (buf, 0, tcold, "                        ---\xfa --\xfa -\xfa ------  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 62);
			break;
		case cpiChanWidth_128:
			cpifaceSession->console->WriteString (buf, 0, tcold, "                             \xb3                   \xb3    \xb3   \xb3  \xb3               \xb3  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 128);
			break;
		case cpiChanWidth_76:
			cpifaceSession->console->WriteString (buf,  0, tcold, "                             \xb3    \xb3   \xb3  \xb3               \xb3 \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 76);
			break;
		case cpiChanWidth_44:
			cpifaceSession->console->WriteString (buf, 0, tcold, " --  ---\xfa --\xfa -\xfa ------   \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 44);
			break;
	}

	if (!xmpChanActive (cpifaceSession, i))
		return;

	ins=xmpGetChanIns(i);
	smp=xmpGetChanSamp(i);
	xmpGetChanInfo(i, &ci);
	switch (width)
	{
		case cpiChanWidth_36:
			cpifaceSession->console->WriteNum    (buf,  1, tcol, ins, 16, 2, 0);
			cpifaceSession->console->WriteString (buf,  4, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
			cpifaceSession->console->WriteNum    (buf,  8, tcol, ci.vol, 16, 2, 0);
			fxstr=getfxstr6(ci.fx);
			if (fxstr)
				cpifaceSession->console->WriteString (buf, 11, tcol, fxstr, 6);
			drawvolbar (cpifaceSession, buf+18, i, st);
			break;
		case cpiChanWidth_62:
			if (ins) {
				if ((*insts[ins-1].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf,  1, tcol, insts[ins-1].name, 21);
				} else {
					cpifaceSession->console->WriteString (buf,  1, 0x08, "(  )", 4);
					cpifaceSession->console->WriteNum    (buf,  2, 0x08, ins, 16, 2, 0);
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
			break;
		case cpiChanWidth_76:
			if (ins)
			{
				if ((*insts[ins-1].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf,  1, tcol, insts[ins-1].name, 28);
				} else {
					cpifaceSession->console->WriteString (buf,  1, 0x08, "(  )", 4);
					cpifaceSession->console->WriteNum    (buf,  2, 0x08, ins, 16, 2, 0);
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
			break;
		case cpiChanWidth_128:
			if (ins)
			{
				if ((*insts[ins-1].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf,  1, tcol, insts[ins-1].name, 28);
				} else {
					cpifaceSession->console->WriteString (buf,  1, 0x08, "(  )", 4);
					cpifaceSession->console->WriteNum    (buf,  2, 0x08, ins, 16, 2, 0);
				}
			}
			if (smp!=0xFFFF)
			{
				if ((*samps[smp].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf, 31, tcol, samps[smp].name, 17);
				} else {
					cpifaceSession->console->WriteString (buf, 31, 0x08, "(    )", 6);
					cpifaceSession->console->WriteNum    (buf, 32, 0x08, smp, 16, 4, 0);
				}
			}
			cpifaceSession->console->WriteString (buf, 50, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
			cpifaceSession->console->WriteString (buf, 53, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			cpifaceSession->console->WriteNum    (buf, 55, tcol, ci.vol, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 57, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide]: &" ~"[ci.volfx], 1);
			cpifaceSession->console->WriteString (buf, 59, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);
			cpifaceSession->console->WriteString (buf, 60, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=getfxstr15(ci.fx);
			if (fxstr)
				cpifaceSession->console->WriteString (buf, 62, tcol, fxstr, 15);
			drawlongvolbar (cpifaceSession, buf+80, i, st);
			break;
		case cpiChanWidth_44:
			cpifaceSession->console->WriteNum    (buf,  1, tcol, xmpGetChanIns(i), 16, 2, 0);
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
			break;
	}
}

OCP_INTERNAL void xmChanSetup (struct cpifaceSessionAPI_t *cpifaceSession, struct xmpinstrument *_insts, struct xmpsample *_samps)
{
	insts = _insts;
	samps = _samps;
	cpifaceSession->UseChannels (cpifaceSession, drawchannel);
}
