/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlay channel display routines
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
#include "itchan.h"
#include "itplay.h"
#include "stuff/poutput.h"

static struct it_instrument *insts;
static struct it_sample *samps;

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

static void drawvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, uint8_t st)
{
	int l,r;
	itplayer_getrealvol (cpifaceSession, &itplayer, i, &l, &r);
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
		uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 8-l, left+8-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 9  , right   , r);
	}
}

static void drawlongvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, uint8_t st)
{
	int l,r;
	itplayer_getrealvol (cpifaceSession, &itplayer, i, &l, &r);
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
		uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		cpifaceSession->console->WriteStringAttr (buf, 16-l, left+16-l, l);
		cpifaceSession->console->WriteStringAttr (buf, 17  , right,     r);
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


static void drawchannel (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiChanWidth width, int i, int compoMode)
{
	uint8_t st = cpifaceSession->MuteChannel[i];

	uint8_t tcol=st?0x08:0x0F;
	uint8_t tcold=st?0x08:0x07;
	uint8_t tcolr=st?0x08:0x0B;

	int av;

	struct it_chaninfo ci;

	char *fxstr;

	switch (width)
	{
		case cpiChanWidth_36:
			cpifaceSession->console->WriteString (buf, 0, tcold, " \xfa\xfa -- --- -- --- \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 36);
			break;
		case cpiChanWidth_62:
			cpifaceSession->console->WriteString (buf, 0, tcold, " \xfa\xfa                      ---\xfa --\xfa -\xfa ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 62);
			break;
		case cpiChanWidth_128:
			cpifaceSession->console->WriteString (buf,  0, tcold, " \xfa\xfa                             \xb3                   \xb3    \xb3   \xb3  \xb3            \xb3  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 128);
			break;
		case cpiChanWidth_76:
			cpifaceSession->console->WriteString (buf,  0, tcold, " \xfa\xfa                             \xb3    \xb3   \xb3  \xb3            \xb3 \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa", 76);
			break;
		case cpiChanWidth_44:
			cpifaceSession->console->WriteString (buf, 0, tcold, " \xfa\xfa -- ---\xfa --\xfa -\xfa ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 44);
			break;
	}

	av=getchanalloc(&itplayer, i);
	if (av)
		cpifaceSession->console->WriteNum (buf, 1, tcold, av, 16, 2, 0);

	if (!lchanactive (cpifaceSession, &itplayer, i))
		return;

	getchaninfo(&itplayer, i, &ci);

	switch (width)
	{
		case cpiChanWidth_36:
			cpifaceSession->console->WriteNum    (buf,  4, tcol, ci.ins, 16, 2, 0);
			cpifaceSession->console->WriteString (buf,  7, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
			cpifaceSession->console->WriteNum    (buf, 11, tcol, ci.vol, 16, 2, 0);
			fxstr=fxstr3[ci.fx];
			if (fxstr)
				cpifaceSession->console->WriteString (buf, 14, tcol, fxstr, 3);
			drawvolbar (cpifaceSession, buf+18, i, st);
			break;
		case cpiChanWidth_62:
			if (ci.ins)
			{
				if ((*insts[ci.ins-1].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf, 4, tcol, insts[ci.ins-1].name, 19);
				} else {
					cpifaceSession->console->WriteString (buf, 4, 0x08, "(  )", 4);
					cpifaceSession->console->WriteNum    (buf, 5, 0x08, ci.ins, 16, 2, 0);
				}
			}
			cpifaceSession->console->WriteString (buf, 25, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
			cpifaceSession->console->WriteString (buf, 28, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			cpifaceSession->console->WriteNum    (buf, 30, tcol, ci.vol, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 32, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			cpifaceSession->console->WriteString (buf, 34, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			cpifaceSession->console->WriteString (buf, 35, tcol, &" \x1A\x1B"[ci.panslide], 1);
			fxstr=fxstr6[ci.fx];
			if (fxstr)
				cpifaceSession->console->WriteString (buf, 37, tcol, fxstr, 6);
			drawvolbar (cpifaceSession, buf+44, i, st);
			break;
		case cpiChanWidth_76:
			if (ci.ins)
			{
				if ((*insts[ci.ins-1].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf, 4, tcol, insts[ci.ins-1].name, 28);
				} else {
					cpifaceSession->console->WriteString (buf, 4, 0x08, "(  )", 4);
					cpifaceSession->console->WriteNum    (buf, 5, 0x08, ci.ins, 16, 2, 0);
				}
			}
			cpifaceSession->console->WriteString (buf, 33, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
			cpifaceSession->console->WriteString (buf, 36, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			cpifaceSession->console->WriteNum    (buf, 38, tcol, ci.vol, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 40, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			cpifaceSession->console->WriteString (buf, 42, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			cpifaceSession->console->WriteString (buf, 43, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=fxstr12[ci.fx];
			if (fxstr)
				cpifaceSession->console->WriteString (buf, 45, tcol, fxstr, 12);

			drawvolbar (cpifaceSession, buf+59, i, st);
			break;
		case cpiChanWidth_128:
			if (ci.ins)
			{
				if ((*insts[ci.ins-1].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf, 4, tcol, insts[ci.ins-1].name, 28);
				} else {
					cpifaceSession->console->WriteString (buf, 4, 0x08, "(  )", 4);
					cpifaceSession->console->WriteNum    (buf, 5, 0x08, ci.ins, 16, 2, 0);
				}
			}
			if (ci.smp!=0xFFFF)
			{
				if ((*samps[ci.smp].name) && (!compoMode))
				{
					cpifaceSession->console->WriteString (buf, 34, tcol, samps[ci.smp].name, 17);
				} else {
					cpifaceSession->console->WriteString (buf, 34, 0x08, "(    )", 6);
					cpifaceSession->console->WriteNum    (buf, 35, 0x08, ci.smp, 16, 4, 0);
				}
			}
			cpifaceSession->console->WriteString (buf, 53, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
			cpifaceSession->console->WriteString (buf, 56, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			cpifaceSession->console->WriteNum    (buf, 58, tcol, ci.vol, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 60, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			cpifaceSession->console->WriteString (buf, 62, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			cpifaceSession->console->WriteString (buf, 63, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=fxstr12[ci.fx];
			if (fxstr)
				cpifaceSession->console->WriteString (buf, 65, tcol, fxstr, 12);
			drawlongvolbar (cpifaceSession, buf+80, i, st);
			break;
		case cpiChanWidth_44:
			cpifaceSession->console->WriteNum    (buf,  4, tcol, ci.ins, 16, 2, 0);
			cpifaceSession->console->WriteString (buf,  7, ci.notehit?tcolr:tcol, cpifaceSession->plNoteStr(ci.note), 3);
			cpifaceSession->console->WriteString (buf, 10, tcol, ci.pitchslide ? &" \x18\x19\x0D\x18\x19\x0D"[ci.pitchslide] : &" ~\xf0"[ci.pitchfx], 1);
			cpifaceSession->console->WriteNum    (buf, 12, tcol, ci.vol, 16, 2, 0);
			cpifaceSession->console->WriteString (buf, 14, tcol, ci.volslide ? &" \x18\x19\x18\x19"[ci.volslide] : &" ~"[ci.volfx], 1);
			cpifaceSession->console->WriteString (buf, 16, tcol, &"L123456MM9ABCDERS"[ci.pan], 1);
			cpifaceSession->console->WriteString (buf, 17, tcol, &" \x1A\x1B"[ci.panslide], 1);

			fxstr=fxstr6[ci.fx];
			if (fxstr)
				cpifaceSession->console->WriteString (buf, 19, tcol, fxstr, 6);
			drawvolbar (cpifaceSession, buf+26, i, st);
			break;
	}
}

void __attribute__ ((visibility ("internal"))) itChanSetup(struct cpifaceSessionAPI_t *cpifaceSession, struct it_instrument *_insts, struct it_sample *_samps)
{
	insts = _insts;
	samps = _samps;
	cpifaceSession->UseChannels (cpifaceSession, drawchannel);
}
