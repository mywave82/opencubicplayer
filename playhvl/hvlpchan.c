/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVLPlay channel display routines
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
#include "hvlplay.h"
#include "hvlpchan.h"
#include "cpiface/cpiface.h"
#include "dev/mix.h"
#include "stuff/poutput.h"

static int logvolbar(int l, int r)
{
	int retval;

	l = l >> 16;
	r = r >> 16;

	if (l>32)
		l=32+((l-32)>>1);
	if (l>48)
		l=48+((l-48)>>1);
	if (l>56)
		l=56+((l-56)>>1);
	if (l>64)
		l=64;
	if (r>32)
		r=32+((r-32)>>1);
	if (r>48)
		r=48+((r-48)>>1);
	if (r>56)
		r=56+((r-56)>>1);
	if (r>64)
		r=64;

	retval = r + l + 3;

	retval = retval / 5;
	if (retval > 10)
		retval = 10;

	return retval;
}

static void drawvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, unsigned char st)
{
	int v;

	if (cpifaceSession->InPause)
	{
		v = 0;
	} else {
		int l, r;
		hvlGetChanVolume (cpifaceSession, i, &l, &r);
		v = logvolbar(l, r);
	}

	if (st)
	{
		cpifaceSession->conFunc->WriteString (buf, 9-v, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", v);
//		cpifaceSession->conFunc->WriteString (buf, 9  , 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		const uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
//		const uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe};
		cpifaceSession->conFunc->WriteStringAttr (buf, 10-v, left+10-v, v);
//		cpifaceSession->conFunc->WriteStringAttr (buf, 9   , right    , r);
	}
}

#if 0
static void drawlongvolbar (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, unsigned char st)
{
	int l,r;
	mixGetRealVolume(i, &l, &r);
	logvolbar(&l, &r);
	l=(l+2)>>2;
	r=(r+2)>>2;
	if (cpifaceSession->InPause)
	{
		l=r=0;
	}
	if (st)
	{
		cpifaceSession->conFunc->WriteString (buf, 16-l, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", l);
		cpifaceSession->conFunc->WriteString (buf, 17, 0x08, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", r);
	} else {
		const uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		const uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		cpifaceSession->conFunc->WriteStringAttr (buf, 16-l, left+16-l, l);
		cpifaceSession->conFunc->WriteStringAttr (buf, 1  7, right    , r);
	}
}
#endif

static char *getfxstr6(unsigned char fx, unsigned char fxparam)
{
	switch (fx)
	{
		case 0x0: /* global command - Position Jump Hi*/
		case 0x6: /* not used */
		case 0x8: /* External timing */
		case 0xb: /* global command - Position Jump */
		case 0xd: /* global command - Break */
		case 0xf: /* global command - Tempo */
		default: return 0;
		case 0x1:
			return "porta\x18";
		case 0x2:
			return "porta\x19";
		case 0x3:
			return "porta\x0d";
		case 0x4:
			return "filter";
		case 0x5:
			return "port+v";
		case 0x7:
			return "pan   ";
		case 0x9:
			return "square";
		case 0xa:
			if (fxparam & 0xf0)
			{
				return "volsl\x18";
			} else {
				return "volsl\x19";
			}
		case 0xc:
			if (fxparam < 0x40)
			{
				return "volins";
			} else if ((fxparam >= 0x50) && (fxparam < 0x90))
			{
				return "volall";
			} else if ((fxparam >= 0xA0) && (fxparam < 0xE0))
			{
				return "volch ";
			}
			return 0;
		case 0xe:
		{
			switch (fxparam & 0xf0)
			{
				case 0x00: /* not used */
				case 0x30: /* not used */
				case 0x50: /* not used */
				case 0x60: /* not used */
				case 0x70: /* not used */
				case 0x80: /* not used */
				case 0x90: /* not used */
				case 0xe0: /* not used */
				default: return 0;
				case 0x10:
					return "fport\x18";
				case 0x20:
					return "fport\x19";
				case 0x40:
					return "vibrat";
				case 0xa0:
					return "fvols\x18";
				case 0xb0:
					return "fvols\x19";
				case 0xc0:
					return " \x0e""cut ";
				case 0xd0:
					return "delay ";
				case 0xf0:
				{
					switch (fxparam & 0x0f)
					{
						default: return 0;
						case 0x01:
							return "preser";
					}
				}
			}
		}
	}
}

static char *getpfxstr6(unsigned char fx, unsigned char fxparam)
{
	switch (fx)
	{
		case 0x0:
			if ((fxparam >= 0x01) && (fxparam <= 0x1f))
			{
				return "filtLo";
			} else if (fxparam == 0x20)
			{
				return "nofilt";
			} else if ((fxparam >= 0x21) && (fxparam <= 0x3f))
			{
				return "filtHi";
			}
			return 0;
		case 0x1:
			return "porta\x18";
		case 0x2:
			return "porta\x19";
		case 0x3:
			return "sq-rel";
		case 0x4:
			return "togMod";
		case 0x7:
			if ((fxparam == 0x00) || (fxparam == 0x80))
			{
				return "no-tri";
			} else {
				return "triMod";
			}
			return 0;
		case 0x8:
			if ((fxparam == 0x00) || (fxparam == 0x80))
			{
				return "no-saw";
			} else {
				return "sawMod";
			}
			return 0;
		case 0x9:
			return "pan   ";
		case 0xc:
			if (fxparam <= 0x40)
			{
				return "volume";
			} else if ((fxparam >= 0x50) && (fxparam <= 0x90))
			{
				return "insvol";
			} else if ((fxparam >= 0xa0) && (fxparam <= 0xf0))
			{
				return "trkvol";
			}
			return 0;
		case 0xf:
			return "speed ";

		default:
		case 0x5: /* internal command - Position Jump */
		case 0x6: /* unused */
		case 0xa: /* unused */
		case 0xb: /* unused */
		case 0xd: /* unused */
			return 0;
	}
}

#if 0
static char *getfxstr15(unsigned char fx, unsigned char fxparam)
{
	switch (fx)
	{
		case 0x0: /* global command - Position Jump Hi */
		case 0x6: /* not used */
		case 0x8: /* External timing */
		case 0xb: /* global command - Position Jump */
		case 0xd: /* global command - Break */
		case 0xf: /* global command - Tempo */
		default: return 0;
		case 0x1:
			return "portamento \x18";
		case 0x2:
			return "portamento \x19";
		case 0x3:
			return "portamento to \x0d";
		case 0x4:
			if (fxparam < 0x40)
			{
				return "filter on jump";
			} else {
				return "filter change";
			}
		case 0x5:
			return "porta+volume";
		case 0x7:
			return "panning";
		case 0x9:
			return "set square rel";
		case 0xa:
			if (fxparam & 0xf0)
			{
				return "volume slide \x18";
			} else {
				return "volume slide \x19";
			}
		case 0xc:
			if (fxparam < 0x40)
			{
				return "volume channel";
			} else {
				return "volume all chan"
			}
		case 0xe:
		{
			switch (fxparam & 0xf0)
			{
				case 0x00: /* not used */
				case 0x30: /* not used */
				case 0x50: /* not used */
				case 0x60: /* not used */
				case 0x70: /* not used */
				case 0x80: /* not used */
				case 0x90: /* not used */
				case 0xe0: /* not used */
				default: return 0;
				case 0x10:
					return "fine porta \x18";
				case 0x20:
					return "fine porta \x19";
				case 0x40:
					return "vibrato control";
				case 0xa0:
					return "fine volslide \x18";
				case 0xb0:
					return "fine volslide \x19";
				case 0xc0:
					return "note cut";
				case 0xd0:
					return "delay";
				case 0xf0:
				{
					switch (fxparam & 0x0f)
					{
						default: return 0;
						case 0x01:
							return "pres. transpose";
					}
				}

			}
		}
	}
}

#endif

static void drawchannel36 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct hvl_chaninfo ci;

	unsigned char tcol, tcold, tcolr;

	hvlGetChanInfo (i, &ci);

	tcol =ci.muted?0x08:0x0F;
	tcold=ci.muted?0x08:0x07;
	tcolr=ci.muted?0x08:0x0B;

	cpifaceSession->conFunc->WriteString (buf, 0, tcold, " -- --- -- ------ ------ \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 36);

	if ((ci.ins>=0) || ci.vol)
	{
		char *fxstr;

		if (ci.ins>=0)
		{
			cpifaceSession->conFunc->WriteNum (buf,  1, tcol, ci.ins + 1, 16, 2, 0);
		}
		cpifaceSession->conFunc->WriteString (buf,  4, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
		cpifaceSession->conFunc->WriteNum    (buf, 8, tcol, ci.vol, 16, 2, 0);
		fxstr=getfxstr6(ci.fx, ci.fxparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 11, tcol, fxstr, 6);
		fxstr=getfxstr6(ci.fxB, ci.fxBparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 18, tcol, fxstr, 6);

		drawvolbar (cpifaceSession, buf+25, i, ci.muted);
	}
}

static void drawchannel44 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct hvl_chaninfo ci;

	unsigned char tcol, tcold, tcolr;

	hvlGetChanInfo (i, &ci);

	tcol =ci.muted?0x08:0x0F;
	tcold=ci.muted?0x08:0x07;
	tcolr=ci.muted?0x08:0x0B;

	cpifaceSession->conFunc->WriteString (buf, 0, tcold, " --  ---\xfa --\xfa - ------ ------    \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 44);
	if ((ci.ins>=0) || ci.vol)
	{
		char *fxstr;
		if (ci.ins>=0)
		{
			cpifaceSession->conFunc->WriteNum (buf,  1, tcol, ci.ins + 1, 16, 2, 0);
		}
		cpifaceSession->conFunc->WriteString (buf,  5, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
		cpifaceSession->conFunc->WriteString (buf,  8, tcol, ci.pitchslide ? &" \x18\x19\x0D"[ci.pitchslide] : " ", 1);
		cpifaceSession->conFunc->WriteNum    (buf, 10, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->conFunc->WriteString (buf, 12, tcol, ci.volslide ? &" \x18\x19\x12"[ci.volslide] : " ", 1);
		cpifaceSession->conFunc->WriteString (buf, 14, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);

		fxstr=getfxstr6(ci.fx, ci.fxparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 16, tcol, fxstr, 6);
		fxstr=getfxstr6(ci.fxB, ci.fxBparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 23, tcol, fxstr, 6);

		drawvolbar (cpifaceSession, buf+33, i, ci.muted);
	}
}

static void drawchannel62 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct hvl_chaninfo ci;

	unsigned char tcol, tcold, tcolr;

	hvlGetChanInfo (i, &ci);

	tcol =ci.muted?0x08:0x0F;
	tcold=ci.muted?0x08:0x07;
	tcolr=ci.muted?0x08:0x0B;

	cpifaceSession->conFunc->WriteString (buf, 0, tcold, "                        ---\xfa --\xfa - ------ ------   \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 66);
	if ((ci.ins>=0) || ci.vol)
	{
		char *fxstr;
		if (ci.ins>=0)
		{
			if (ci.name && (!compoMode))
			{
				cpifaceSession->conFunc->WriteString (buf, 1, tcol, ci.name, 21);
			} else {
				cpifaceSession->conFunc->WriteString (buf, 1, 0x08, "(  )", 4);
				cpifaceSession->conFunc->WriteNum    (buf, 2, 0x08, ci.ins + 1, 16, 2, 0);
			}
		}
		cpifaceSession->conFunc->WriteString (buf, 24, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
		cpifaceSession->conFunc->WriteString (buf, 27, tcol, ci.pitchslide ? &" \x18\x19\x0D"[ci.pitchslide] : " ", 1);
		cpifaceSession->conFunc->WriteNum    (buf, 29, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->conFunc->WriteString (buf, 31, tcol, ci.volslide ? &" \x18\x19\x12"[ci.volslide] : " ", 1);
		cpifaceSession->conFunc->WriteString (buf, 33, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);

		fxstr=getfxstr6(ci.fx, ci.fxparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 35, tcol, fxstr, 6);
		fxstr=getfxstr6(ci.fxB, ci.fxBparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 42, tcol, fxstr, 6);

		drawvolbar (cpifaceSession, buf+51, i, ci.muted);
	}
}

static void drawchannel76 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct hvl_chaninfo ci;

	unsigned char tcol, tcold, tcolr;

	hvlGetChanInfo (i, &ci);

	tcol =ci.muted?0x08:0x0F;
	tcold=ci.muted?0x08:0x07;
	tcolr=ci.muted?0x08:0x0B;

	cpifaceSession->conFunc->WriteString (buf,  0, tcold, "                             \xb3    \xb3   \xb3 \xb3          \xb3          \xb3  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 76);
	if ((ci.ins>=0) || ci.vol)
	{
		char *fxstr;
		if (ci.ins>=0)
		{
			if (ci.name && (!compoMode))
			{
				cpifaceSession->conFunc->WriteString (buf, 1, tcol, ci.name, 28);
			} else {
				cpifaceSession->conFunc->WriteString (buf, 1, 0x08, "(  )", 4);
				cpifaceSession->conFunc->WriteNum    (buf, 2, 0x08, ci.ins + 1, 16, 2, 0);
			}
		}
		cpifaceSession->conFunc->WriteString (buf, 30, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
		cpifaceSession->conFunc->WriteString (buf, 33, tcol, ci.pitchslide ? &" \x18\x19\x0D"[ci.pitchslide] : " ", 1);
		cpifaceSession->conFunc->WriteNum    (buf, 35, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->conFunc->WriteString (buf, 37, tcol, ci.volslide ? &" \x18\x19\x12"[ci.volslide] : " ", 1);
		cpifaceSession->conFunc->WriteString (buf, 39, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);

		fxstr=getfxstr6(ci.fx, ci.fxparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 41, tcol, fxstr, 6);
		cpifaceSession->conFunc->WriteNum (buf, 48, tcol, ci.fx,       16, 1, 0);
		cpifaceSession->conFunc->WriteNum (buf, 49, tcol, ci.fxparam,  16, 2, 0);
		fxstr=getfxstr6(ci.fxB, ci.fxBparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 52, tcol, fxstr, 6);
		cpifaceSession->conFunc->WriteNum (buf, 59, tcol, ci.fxB,      16, 1, 0);
		cpifaceSession->conFunc->WriteNum (buf, 60, tcol, ci.fxBparam, 16, 2, 0);

		drawvolbar (cpifaceSession, buf+65, i, ci.muted);
	}
}

static void drawchannel128 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int i, int compoMode)
{
	struct hvl_chaninfo ci;
	unsigned char tcol, tcold, tcolr;

	hvlGetChanInfo (i, &ci);

	tcol =ci.muted?0x08:0x0F;
	tcold=ci.muted?0x08:0x07;
	tcolr=ci.muted?0x08:0x0B;

	cpifaceSession->conFunc->WriteString (buf,  0, tcold, "                                      \xb3    \xb3   \xb3 \xb3          \xb3          \xb3          \xb3        \xb3          \xb3          \xb3  \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 128);
	if ((ci.ins>=0) || ci.vol)
	{
		char *fxstr;
		if (ci.ins>=0)
		{
			if (ci.name && (!compoMode))
			{
				cpifaceSession->conFunc->WriteString (buf, 1, tcol, ci.name, 37);
			} else {
				cpifaceSession->conFunc->WriteString (buf, 1, 0x08, "(  )", 4);
				cpifaceSession->conFunc->WriteNum    (buf, 2, 0x08, ci.ins + 1, 16, 2, 0);
			}
		}
		cpifaceSession->conFunc->WriteString (buf, 39, ci.notehit?tcolr:tcol, plNoteStr[ci.note], 3);
		cpifaceSession->conFunc->WriteString (buf, 42, tcol, ci.pitchslide ? &" \x18\x19\x0D"[ci.pitchslide] : " ", 1);
		cpifaceSession->conFunc->WriteNum    (buf, 44, tcol, ci.vol, 16, 2, 0);
		cpifaceSession->conFunc->WriteString (buf, 46, tcol, ci.volslide ? &" \x18\x19\x12"[ci.volslide] : " ", 1);
		cpifaceSession->conFunc->WriteString (buf, 48, tcol, &"L123456MM9ABCDER"[ci.pan>>4], 1);

		if (ci.filter)
		{
			if (ci.filter < 0x20)
			{
				cpifaceSession->conFunc->WriteString (buf, 50, tcol, "lowpass ", 8);
				cpifaceSession->conFunc->WriteNum    (buf, 58, tcol, 0x20 - ci.filter, 10, 2, 0);
			} else if (ci.filter == 0x20)
			{
				cpifaceSession->conFunc->WriteString (buf, 50, tcol, "minimal   ", 10);
			} else {
				cpifaceSession->conFunc->WriteString (buf, 58, tcol, "highpass", 8);
				cpifaceSession->conFunc->WriteNum    (buf, 58, tcol, ci.filter - 0x20, 10, 2, 0);
			}
		}

		fxstr=getfxstr6(ci.fx, ci.fxparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 61, tcol, fxstr, 6);
		cpifaceSession->conFunc->WriteNum (buf, 68, tcol, ci.fx,       16, 1, 0);
		cpifaceSession->conFunc->WriteNum (buf, 69, tcol, ci.fxparam,  16, 2, 0);
		fxstr=getfxstr6(ci.fxB, ci.fxBparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 72, tcol, fxstr, 6);
		cpifaceSession->conFunc->WriteNum (buf, 79, tcol, ci.fxB,      16, 1, 0);
		cpifaceSession->conFunc->WriteNum (buf, 80, tcol, ci.fxBparam, 16, 2, 0);

		switch (ci.waveform)
		{
			case 0x00: cpifaceSession->conFunc->WriteString (buf, 83, tcol, "triangle", 8); break;
			case 0x01: cpifaceSession->conFunc->WriteString (buf, 83, tcol, "sawtooth", 8); break;
			case 0x02: cpifaceSession->conFunc->WriteString (buf, 83, tcol, "square  ", 8); break;
			case 0x03: cpifaceSession->conFunc->WriteString (buf, 83, tcol, "whitenoi", 8); break;
		}

		fxstr=getpfxstr6(ci.pfx, ci.pfxparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf,  92, tcol, fxstr, 6);
		cpifaceSession->conFunc->WriteNum (buf,  99, tcol, ci.pfx,       16, 1, 0);
		cpifaceSession->conFunc->WriteNum (buf, 100, tcol, ci.pfxparam,  16, 2, 0);
		fxstr=getpfxstr6(ci.pfxB, ci.pfxBparam);
		if (fxstr)
			cpifaceSession->conFunc->WriteString (buf, 103, tcol, fxstr, 6);
		cpifaceSession->conFunc->WriteNum (buf, 110, tcol, ci.pfxB,       16, 1, 0);
		cpifaceSession->conFunc->WriteNum (buf, 111, tcol, ci.pfxBparam,  16, 2, 0);

		drawvolbar (cpifaceSession, buf+116, i, ci.muted);
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

void __attribute__ ((visibility ("internal"))) hvlChanSetup (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->UseChannels (cpifaceSession, drawchannel);
}

