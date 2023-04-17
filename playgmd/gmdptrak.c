/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay track/pattern display routines
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
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "gmdplay.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"
#include "gmdptrak.h"

#define COLPTNOTE 0x0A
#define COLNOTE 0x0F
#define COLPITCH 0x02
#define COLSPEED 0x02
#define COLPAN 0x05
#define COLVOL 0x09
#define COLACT 0x04
#define COLINS 0x07

static const struct gmdtrack *plTracks;
static const struct gmdpattern *plPatterns;
static const uint16_t *plOrders;
static const uint8_t *curtrk;
static const uint8_t *curtrkend;
static const uint8_t *currow;
static const uint8_t *currowend;

static int gmd_startrow (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (curtrk >= curtrkend)
	{
		return -1;
	}
	currow=curtrk+2;
	currowend=curtrk+=2+curtrk[1];
	return currow[-2];
}

static void gmd_seektrack (struct cpifaceSessionAPI_t *cpifaceSession, int n, int c)
{
	n=(c==-1)?plPatterns[plOrders[n]].gtrack:plPatterns[plOrders[n]].tracks[c];
	curtrk=plTracks[n].ptr;
	curtrkend=plTracks[n].end;
}

static const char *gmd_getpatname (struct cpifaceSessionAPI_t *cpifaceSession, int n)
{
	return plPatterns[plOrders[n]].name;
}

static int gmd_getpatlen (struct cpifaceSessionAPI_t *cpifaceSession, int n)
{
	if (plOrders[n] == 0xffff)
	{
		return 0;
	}
	return plPatterns[plOrders[n]].patlen;
}

static int gmd_getcurpos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return mpGetRealPos (cpifaceSession)>>8;
}

static int gmd_getnote (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int small)
{
	const uint8_t *ptr=currow;

	while (ptr<currowend)
	{
		if (*ptr&cmdPlayNote)
		{
			uint8_t pn=*ptr++;
			if (pn&cmdPlayIns)
				ptr++;
			if (pn&cmdPlayNte)
			{
				switch (small)
				{
					case 0:
						cpifaceSession->console->WriteString (bp, 0, (*ptr&0x80)?COLPTNOTE:COLNOTE, &"CCDDEFFGGAAB"[(*ptr&~0x80)%12], 1);
						cpifaceSession->console->WriteString (bp, 1, (*ptr&0x80)?COLPTNOTE:COLNOTE, &"-#-#--#-#-#-"[(*ptr&~0x80)%12], 1);
						cpifaceSession->console->WriteString (bp, 2, (*ptr&0x80)?COLPTNOTE:COLNOTE, &"-0123456789"[(*ptr&~0x80)/12], 1);
						break;
					case 1:
						cpifaceSession->console->WriteString (bp, 0, (*ptr&0x80)?COLPTNOTE:COLNOTE, &"cCdDefFgGaAb"[(*ptr&0x7F)%12], 1);
						cpifaceSession->console->WriteString (bp, 1, (*ptr&0x80)?COLPTNOTE:COLNOTE, &"-0123456789"[(*ptr&0x7F)/12], 1);
						break;
					case 2:
						cpifaceSession->console->WriteString (bp, 0, (*ptr&0x80)?COLPTNOTE:COLNOTE, &"cCdDefFgGaAb"[(*ptr&0x7F)%12], 1);
						break;
				}
				return 1;
			}
			if (pn&cmdPlayVol)
				ptr++;
			if (pn&cmdPlayPan)
				ptr++;
			if (pn&cmdPlayDelay)
				ptr++;
		} else
			ptr+=2;
	}
	return 0;
}

static int gmd_getvol (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	const uint8_t *ptr=currow;

	while (ptr<currowend)
	{
		if (*ptr&cmdPlayNote)
		{
			uint8_t pn=*ptr++;
			if (pn&cmdPlayIns)
				ptr++;
			if (pn&cmdPlayNte)
				ptr++;
			if (pn&cmdPlayVol)
			{
				cpifaceSession->console->WriteNum (bp, 0, COLVOL, *ptr++, 16, 2, 0);
				return 1;
			}
			if (pn&cmdPlayPan)
				ptr++;
			if (pn&cmdPlayDelay)
				ptr++;
		} else
			ptr+=2;
	}
	return 0;
}

static int gmd_getins (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	const uint8_t *ptr=currow;

	while (ptr<currowend)
	{
		if (*ptr&cmdPlayNote)
		{
			uint8_t pn=*ptr++;
			if (pn&cmdPlayIns)
			{
				cpifaceSession->console->WriteNum (bp, 0, COLINS, *ptr++, 16, 2, 0);
				return 1;
			}
			if (pn&cmdPlayNte)
				ptr++;
			if (pn&cmdPlayVol)
				ptr++;
			if (pn&cmdPlayPan)
				ptr++;
			if (pn&cmdPlayDelay)
				ptr++;
		} else
			ptr+=2;
	}
	return 0;
}

static int gmd_getpan (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	const uint8_t *ptr=currow;
	while (ptr<currowend)
	{
		if (*ptr&cmdPlayNote)
		{
			uint8_t pn=*ptr++;
			if (pn&cmdPlayIns)
				ptr++;
			if (pn&cmdPlayNte)
				ptr++;
			if (pn&cmdPlayVol)
				ptr++;
			if (pn&cmdPlayPan)
			{
				cpifaceSession->console->WriteNum (bp, 0, COLPAN, *ptr++, 16, 2, 0);
				return 1;
			}
			if (pn&cmdPlayDelay)
				ptr++;
		} else
			ptr+=2;
	}
	return 0;
}

static void gmd_getgcmd (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n)
{
	const uint8_t *ptr=currow;

	while (n&&(ptr<currowend))
	{
		switch (*ptr++)
		{
			case cmdTempo:
				cpifaceSession->console->WriteString (buf, 0, COLSPEED, "t", 1);
				cpifaceSession->console->WriteNum    (buf, 1, COLSPEED, *ptr, 16, 2, 0);
				break;
			case cmdSpeed:
				cpifaceSession->console->WriteString (buf, 0, COLSPEED, "s", 1);
				cpifaceSession->console->WriteNum    (buf, 1, COLSPEED, *ptr, 16, 2, 0);
				break;
			case cmdFineSpeed:
				cpifaceSession->console->WriteString (buf, 0, COLSPEED, "s.", 2);
				cpifaceSession->console->WriteNum    (buf, 2, COLSPEED, *ptr, 16, 1, 0);
				break;
			case cmdBreak:
				cpifaceSession->console->WriteString (buf, 0, COLACT, "\x19", 1);
				cpifaceSession->console->WriteNum    (buf, 1, COLACT, *ptr, 16, 2, 0);
				break;
			case cmdGoto:
				cpifaceSession->console->WriteString (buf, 0, COLACT, "\x1A", 1);
				cpifaceSession->console->WriteNum    (buf, 1, COLACT, *ptr, 16, 2, 0);
				break;
			case cmdPatLoop:
				cpifaceSession->console->WriteString (buf, 0, COLACT, "pl", 2);
				cpifaceSession->console->WriteNum    (buf, 2, COLACT, *ptr, 16, 1, 0);
				break;
			case cmdGlobVol:
				cpifaceSession->console->WriteString (buf, 0, COLVOL, "v", 1);
				cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
				break;
			case cmdGlobVolSlide:
				cpifaceSession->console->WriteString (buf, 0, COLVOL, (*(int8_t*)ptr>0)?"\x18":(*(int8_t*)ptr<0)?"\x19":"\x12", 1);
				cpifaceSession->console->WriteNum    (buf, 1, COLVOL, abs(*(int8_t*)ptr), 16, 2, 0);
				break;
			case cmdPatDelay:
				cpifaceSession->console->WriteString (buf, 0, COLACT, "pd", 2);
				cpifaceSession->console->WriteNum    (buf, 2, COLACT, *ptr, 16, 1, 0);
				break;
			default:
				buf-=4;
				n++;
		}
		ptr++;
		buf+=4;
		n--;
	}
}

static void gmd_getfx (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n)
{
	const uint8_t *ptr=currow;
	while (n&&(ptr<currowend))
	{
		if (*ptr&cmdPlayNote)
		{
			uint8_t pn=*ptr++;
			if (pn&cmdPlayIns)
				ptr++;
			if (pn&cmdPlayNte)
				ptr++;
			if (pn&cmdPlayVol)
				ptr++;
			if (pn&cmdPlayPan)
				ptr++;
			if (pn&cmdPlayDelay)
			{
				cpifaceSession->console->WriteString (buf, 0, COLACT, "d", 1);
				cpifaceSession->console->WriteNum    (buf, 1, COLACT, *ptr++, 16, 2, 0);
				buf+=3;
				n--;
			}
		} else {
			switch (*ptr++)
			{
				case cmdSpecial:
					switch (*ptr)
					{
						case cmdContVolSlide:
							cpifaceSession->console->WriteString (buf, 0, COLVOL, "\x12""00", 3);
							break;
						case cmdContRowVolSlide:
							cpifaceSession->console->WriteString (buf, 0, COLVOL, "\xfa""00", 3);
							break;
						case cmdContMixVolSlide:
							cpifaceSession->console->WriteString (buf, 0, COLVOL, "\x12\xfa""0", 3);
							break;
						case cmdContMixVolSlideUp:
							cpifaceSession->console->WriteString (buf, 0, COLVOL, "\x18""+0", 3);
							break;
						case cmdContMixVolSlideDown:
							cpifaceSession->console->WriteString (buf, 0, COLVOL, "\x19""-0", 3);
							break;
						case cmdContMixPitchSlideUp:
							cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x18""+0", 3);
							break;
						case cmdContMixPitchSlideDown:
							cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x19""-0", 3);
							break;
					}
					break;
				case cmdChannelVol:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "V", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
					break;
				case cmdPitchSlideUp: case cmdPitchSlideUDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x18", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdPitchSlideDown: case cmdPitchSlideDDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x19", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdRowPitchSlideUp:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "+", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdRowPitchSlideDown:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "-", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdRowPitchSlideDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, ((int8_t)*ptr<0)?"-":"+", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, abs((int8_t)*ptr), 16, 2, 0);
					break;
				case cmdPitchSlideToNote: case cmdPitchSlideNDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x0D", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdPitchVibrato:
				case cmdPitchVibratoSinDMF:
				case cmdPitchVibratoTrgDMF:
				case cmdPitchVibratoRecDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "~", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdPitchVibratoFine:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "~", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdPitchVibratoSetSpeed:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "~\x1A", 2);
					cpifaceSession->console->WriteNum    (buf, 2, COLPITCH, *ptr, 16, 1, 0);
					break;
				case cmdPitchVibratoSetWave:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "~=", 2);
					cpifaceSession->console->WriteString (buf, 2, COLPITCH, &"~\\*\x1C?           ~\\\x1C?"[*ptr], 1);
					break;
				case cmdArpeggio:
					cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\xf0", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, *ptr, 16, 2, 0);
					break;
				case cmdVolSlideUp:
				case cmdVolSlideUDMF:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "\x18", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
					break;
				case cmdVolSlideDown:
				case cmdVolSlideDDMF:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "\x19", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
					break;
				case cmdRowVolSlideUp:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "+", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
					break;
				case cmdRowVolSlideDown:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "-", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
					break;
				case cmdVolVibrato:
				case cmdVolVibratoSinDMF:
				case cmdVolVibratoTrgDMF:
				case cmdVolVibratoRecDMF:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "~", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
					break;
				case cmdVolVibratoSetWave:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "~=", 2);
					cpifaceSession->console->WriteString (buf, 2, COLVOL, &"~\\*\x1C?           ~/\x1C?"[*ptr], 1);
					break;
				case cmdTremor:
					cpifaceSession->console->WriteString (buf, 0, COLVOL, "\xA9", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLVOL, *ptr, 16, 2, 0);
					break;
				case cmdPanSlide:
					cpifaceSession->console->WriteString (buf, 0, COLPAN, (*(int8_t*)ptr>0)?"\x1A":(*(int8_t*)ptr<0)?"\x1B":"\x1D", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPAN, abs(*(int8_t*)ptr), 16, 2, 0);
					break;
				case cmdPanSlideLDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPAN, "\x1B", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPAN, *ptr, 16, 2, 0);
					break;
				case cmdPanSlideRDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPAN, "\x1A", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPAN, *ptr, 16, 2, 0);
					break;
				case cmdPanVibratoSinDMF:
					cpifaceSession->console->WriteString (buf, 0, COLPAN, "~", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPAN, *ptr, 16, 2, 0);
					break;
				case cmdPanSurround:
					cpifaceSession->console->WriteString (buf, 0, COLPAN, ">\x01<", 3);
					break;
				case cmdRetrig:
					cpifaceSession->console->WriteString (buf, 0, COLACT, "\x13", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLACT, *ptr, 16, 2, 0);
					break;
				case cmdOffset:
					cpifaceSession->console->WriteString (buf, 0, COLACT, "\x1A", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLACT, *ptr, 16, 2, 0);
					break;
				case cmdOffsetEnd:
					cpifaceSession->console->WriteString (buf, 0, COLACT, "\x1B", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLACT, *ptr, 16, 2, 0);
					break;
				case cmdSetDir:
					cpifaceSession->console->WriteString (buf, 0, COLACT, (*ptr==0)?"-->":(*ptr==1)?"<--":"<->", 3);
					break;
				case cmdSetLoop:
					cpifaceSession->console->WriteString (buf, 0, COLACT, (*ptr==1)?"lp1":(*ptr==2)?"lp2":"lp-", 3);
					break;
				case cmdNoteCut:
					cpifaceSession->console->WriteString (buf, 0, COLACT, "^", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLACT, *ptr, 16, 2, 0);
					break;
				case cmdKeyOff:
					cpifaceSession->console->WriteString (buf, 0, COLINS, "off", 3);
					break;
				case cmdSetEnvPos:
					cpifaceSession->console->WriteString (buf, 0, COLINS, "\x1A", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLINS, *ptr, 16, 2, 0);
					break;
				case cmdPanHeight:
					cpifaceSession->console->WriteString (buf, 0, COLPAN, "Y", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPAN, *ptr, 16, 2, 0);
					break;
				case cmdPanDepth:
					cpifaceSession->console->WriteString (buf, 0, COLPAN, "Z", 1);
					cpifaceSession->console->WriteNum    (buf, 1, COLPAN, *ptr, 16, 2, 0);
					break;
				default:
					buf-=3;
					n++;
			}
			ptr++;
			buf+=3;
			n--;
		}
	}
}

static struct cpitrakdisplaystruct gmdptrkdisplay=
{
	gmd_getcurpos, gmd_getpatlen, gmd_getpatname, gmd_seektrack, gmd_startrow, gmd_getnote,
	gmd_getins, gmd_getvol, gmd_getpan, gmd_getfx, gmd_getgcmd
};


OCP_INTERNAL void gmdTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct gmdmodule *mod)
{
	plPatterns=mod->patterns;
	plOrders=mod->orders;
	plTracks=mod->tracks;
	cpifaceSession->TrackSetup (cpifaceSession, &gmdptrkdisplay, mod->ordnum);
}
