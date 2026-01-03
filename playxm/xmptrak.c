/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay track/channel display routines
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
 */

#include "config.h"
#include "types.h"

#include "stuff/poutput.h"
#include "cpiface/cpiface.h"
#include "xmplay.h"

#define COLPTNOTE 0x0A
#define COLNOTE 0x0F
#define COLPITCH 0x02
#define COLSPEED 0x02
#define COLPAN 0x05
#define COLVOL 0x09
#define COLACT 0x04
#define COLINS 0x07

static uint16_t *plPatLens;
static uint8_t (**plPatterns)[5];
static const uint16_t *plOrders;
static uint8_t (*xmcurpat)[5];
static int xmcurchan;
static int xmcurrow;
static int xmcurpatlen;

static int xm_getpatlen (struct cpifaceSessionAPI_t *cpifaceSession, int n)
{
	if (plOrders[n]==0xFFFF)
		return 0;
	return plPatLens[plOrders[n]];
}

static void xm_seektrack (struct cpifaceSessionAPI_t *cpifaceSession, int n, int c)
{
	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	xmcurpat=plPatterns[plOrders[n]] - mod.nchan;
	xmcurchan=c;
	xmcurrow=-1;
	xmcurpatlen=plPatLens[plOrders[n]];
}

static int xm_startrow (struct cpifaceSessionAPI_t *cpifaceSession)
{
	xmcurrow++;
	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	xmcurpat += mod.nchan;
	if (xmcurrow>=xmcurpatlen)
		return -1;
	return xmcurrow;
}

static int xm_getnote (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int small)
{
	int note=xmcurpat[xmcurchan][0];
	int porta;

	if (!note)
		return 0;
	note--;
	porta=0;
	if (xmcurpat[xmcurchan][3]==xmpCmdPortaNote)
		porta=1;
	if (xmcurpat[xmcurchan][3]==xmpCmdPortaVol)
		porta=1;
	if ((xmcurpat[xmcurchan][2]>>4)==xmpVCmdPortaNote)
		porta=1;
	switch (small)
	{
		case 0:
			if (note==96)
				cpifaceSession->console->WriteString (bp, 0, COLINS, "---", 3);
			else {
				cpifaceSession->console->WriteString (bp, 0, porta?COLPTNOTE:COLNOTE, &"CCDDEFFGGAAB"[note%12], 1);
				cpifaceSession->console->WriteString (bp, 1, porta?COLPTNOTE:COLNOTE, &"-#-#--#-#-#-"[note%12], 1);
				cpifaceSession->console->WriteString (bp, 2, porta?COLPTNOTE:COLNOTE, &"01234567"[note/12], 1);
			}
			break;
		case 1:
			if (note==96)
				cpifaceSession->console->WriteString (bp, 0, COLINS, "--", 2);
			else {
				cpifaceSession->console->WriteString (bp, 0, porta?COLPTNOTE:COLNOTE, &"cCdDefFgGaAb"[note%12], 1);
				cpifaceSession->console->WriteString (bp, 1, porta?COLPTNOTE:COLNOTE, &"01234567"[note/12], 1);
			}
			break;
		case 2:
			if (note==96)
				cpifaceSession->console->WriteString (bp, 0, COLINS, "-", 1);
			else
				cpifaceSession->console->WriteString (bp, 0, porta?COLPTNOTE:COLNOTE, &"cCdDefFgGaAb"[note%12], 1);
			break;
	}
	return 1;
}

static int xm_getins (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	int ins=xmcurpat[xmcurchan][1];
	if (!ins)
		return 0;
	cpifaceSession->console->WriteNum (bp, 0, COLINS, ins, 16, 2, 0);
	return 1;
}

static int xm_getvol (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	int vol=xmcurpat[xmcurchan][2];
	if ((vol>=0x10)&&(vol<0x60))
	{
		cpifaceSession->console->WriteNum (bp, 0, COLVOL, vol-0x10, 16, 2, 0);
		return 1;
	}
	if (xmcurpat[xmcurchan][3]==xmpCmdVolume)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLVOL, xmcurpat[xmcurchan][4], 16, 2, 0);
		return 1;
	}
	return 0;
}

static int xm_getpan (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	if ((xmcurpat[xmcurchan][2]>>4)==xmpVCmdPanning)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, (xmcurpat[xmcurchan][2]&0xF)*0x11, 16, 2, 0);
		return 1;
	}
	if (xmcurpat[xmcurchan][3]==xmpCmdPanning)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, xmcurpat[xmcurchan][4], 16, 2, 0);
		return 1;
	}
	if (xmcurpat[xmcurchan][3]==xmpCmdSPanning)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, xmcurpat[xmcurchan][4]*0x11, 16, 2, 0);
		return 1;
	}
	return 0;
}

static void xm_getfx (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int n)
{
	int p=0;
	int data=xmcurpat[xmcurchan][2]&0xF;
	switch (xmcurpat[xmcurchan][2]>>4)
	{
		case xmpVCmdVolSlideD:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x19", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpVCmdVolSlideU:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x18", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpVCmdFVolSlideD:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "-", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpVCmdFVolSlideU:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "+", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpVCmdVibRate:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~\x1A", 2);
			cpifaceSession->console->WriteNum    (bp, 2, COLPITCH, data, 16, 1, 0);
			break;
		case xmpVCmdVibDep:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case xmpVCmdPanSlideL:
			cpifaceSession->console->WriteString (bp, 0, COLPAN, "\x1B", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data, 16, 2, 0);
			break;
		case xmpVCmdPanSlideR:
			cpifaceSession->console->WriteString (bp, 0, COLPAN, "\x1A", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data, 16, 2, 0);
			break;
		case xmpVCmdPortaNote:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		default:
			bp-=3;
			p--;
	}
	bp+=3;
	p++;

	if (p==n)
		return;
	data=xmcurpat[xmcurchan][4];
	switch (xmcurpat[xmcurchan][3])
	{
		case xmpCmdArpeggio:
			if (data)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\xf0", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			}
			break;
		case xmpCmdPortaU:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x18", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case xmpCmdPortaD:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x19", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case xmpCmdPortaNote:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case xmpCmdVibrato:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case xmpCmdPortaVol:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D", 1);
			if (!data)
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x12""0", 2);
			else
				if (data&0xF0)
				{
					cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x18", 1);
					cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data>>4, 16, 1, 0);
				} else {
					cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x19", 1);
					cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data&0xF, 16, 1, 0);
				}
			break;
		case xmpCmdVibVol:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~", 1);
			if (!data)
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x12""0", 2);
			else
				if (data&0xF0)
				{
					cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x18", 1);
					cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data>>4, 16, 1, 0);
				} else {
					cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x19", 1);
					cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data&0xF, 16, 1, 0);
				}
			break;
		case xmpCmdTremolo:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "~", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpCmdOffset:
			cpifaceSession->console->WriteString (bp, 0, COLACT, "\x1A", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
			break;
		case xmpCmdVolSlide:
			if (!data)
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x12""00", 3);
			else
				if (data&0xF0)
				{
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x18", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data>>4, 16, 2, 0);
				} else {
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x19", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data&0xF, 16, 2, 0);
				}
			break;
		case xmpCmdKeyOff:
			cpifaceSession->console->WriteString (bp, 0, COLINS, "-", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLINS, data&0xF, 16, 2, 0);
			break;
		case xmpCmdEnvPos:
			cpifaceSession->console->WriteString (bp, 0, COLINS, "\x1A", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLINS, data, 16, 2, 0);
			break;
		case xmpCmdMRetrigger: case xmpCmdRetrigger:
			cpifaceSession->console->WriteString (bp, 0, COLACT, "\x13", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
			break;
		case xmpCmdTremor:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "\xA9", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpCmdNoteCut:
			cpifaceSession->console->WriteString (bp, 0, COLACT, "^", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
			break;
		case xmpCmdFPortaU:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "+", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data*4, 16, 2, 0);
			break;
		case xmpCmdFPortaD:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "-", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data*4, 16, 2, 0);
			break;
		case xmpCmdDelayNote:
			cpifaceSession->console->WriteString (bp, 0, COLACT, "d", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
			break;
		case xmpCmdFVolSlideU:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "+", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpCmdFVolSlideD:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "-", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case xmpCmdPanSlide:
			if (!data)
				cpifaceSession->console->WriteString (bp, 0, COLPAN, "\1D""00", 3);
			else
				if (data&0xF0)
				{
					cpifaceSession->console->WriteString (bp, 0, COLPAN, "\x1A", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data>>4, 16, 2, 0);
				} else {
					cpifaceSession->console->WriteString (bp, 0, COLPAN, "\x1B", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data&0xF, 16, 2, 0);
				}
			break;
		case xmpCmdXPorta:
			if ((data>>4)==1)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "+", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data&0xF, 16, 2, 0);
			} else
				if ((data>>4)==2)
				{
					cpifaceSession->console->WriteString (bp, 0, COLPITCH, "-", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data&0xF, 16, 2, 0);
				}
			break;
		case xmpCmdGlissando:
			if (data)
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D=\xA9", 3);
			else
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D=/", 3);
			break;
		case xmpCmdVibType:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~=", 2);
			cpifaceSession->console->WriteString (bp, 2, COLPITCH, &"~\\\xA9?"[data&3], 1);
			break;
		case xmpCmdTremType:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "~=", 2);
			cpifaceSession->console->WriteString (bp, 2, COLVOL, &"~\\\xA9?"[data&3], 1);
			break;
		case xmpCmdSFinetune:
			cpifaceSession->console->WriteString (bp, 0, COLINS, "ft", 2);
			cpifaceSession->console->WriteNum    (bp, 2, COLINS, data, 16, 1, 0);
			break;
	}
}

static void xm_getgcmd (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int n)
{
	int p=0;
	int i;
	/* mod.nchan == cpifaceSession->LogicalChannelCount */
	for (i=0; i<mod.nchan; i++)
	{
		int data;
		if (p==n)
			break;
		data=xmcurpat[i][4];
		switch (xmcurpat[i][3])
		{
			case xmpCmdJump:
				cpifaceSession->console->WriteString (bp, 0, COLACT, "\x1A", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
				break;
			case xmpCmdBreak:
				cpifaceSession->console->WriteString (bp, 0, COLACT, "\x19", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
				break;
			case xmpCmdSpeed:
				if (!data)
					cpifaceSession->console->WriteString (bp, 0, COLACT, "end", 3);
				else
					if (data<0x20)
					{
						cpifaceSession->console->WriteString (bp, 0, COLSPEED, "t", 1);
						cpifaceSession->console->WriteNum    (bp, 1, COLSPEED, data, 16, 2, 0);
					} else {
						cpifaceSession->console->WriteString (bp, 0, COLSPEED, "b", 1);
						cpifaceSession->console->WriteNum    (bp, 1, COLSPEED, data, 16, 2, 0);
					}
				break;
			case xmpCmdGVolume:
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "v", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
				break;
			case xmpCmdGVolSlide:
				if (!data)
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x12""00", 3);
				else
					if (data&0xF0)
					{
						cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x18", 1);
						cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data>>4, 16, 2, 0);
					} else {
						cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x19", 1);
						cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data&0xF, 16, 2, 0);
					}
				break;
			case xmpCmdPatLoop:
				cpifaceSession->console->WriteString (bp, 0, COLACT, "pl", 2);
				cpifaceSession->console->WriteNum    (bp, 2, COLACT, data, 16, 1, 0);
				break;
			case xmpCmdPatDelay:
				cpifaceSession->console->WriteString (bp, 0, COLACT, "pd", 2);
				cpifaceSession->console->WriteNum    (bp, 2, COLACT, data, 16, 1, 0);
				break;
			default:
				bp-=4;
				p--;
		}
		bp+=4;
		p++;
	}
}

static const char *xm_getpatname (struct cpifaceSessionAPI_t *cpifaceSession, int dummy)
{
	return 0;
}

static int xm_getcurpos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return xmpGetRealPos (cpifaceSession) >> 8;
}

static struct cpitrakdisplaystruct xmtrakdisplay=
{
	xm_getcurpos, xm_getpatlen, xm_getpatname, xm_seektrack, xm_startrow, xm_getnote,
	xm_getins, xm_getvol, xm_getpan, xm_getfx, xm_getgcmd
};

OCP_INTERNAL void xmTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct xmodule *mod)
{
	plPatterns=mod->patterns;
	plOrders=mod->orders;
	plPatLens=mod->patlens;
	cpifaceSession->TrackSetup (cpifaceSession, &xmtrakdisplay, mod->nord);
}
