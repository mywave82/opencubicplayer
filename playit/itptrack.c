/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlay track/pattern display code
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
 *    -added some effects to effect output
 */

#include "config.h"
#include "types.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "itplay.h"

#define COLPTNOTE 0x0A
#define COLNOTE 0x0F
#define COLPITCH 0x02
#define COLSPEED 0x02
#define COLPAN 0x05
#define COLVOL 0x09
#define COLACT 0x04
#define COLINS 0x07

static uint16_t *plPatLens;
static uint8_t **plPatterns;
static const uint16_t *plOrders;
static uint8_t *xmcurpat;
static int xmcurchan;
static int xmcurrow;
static int xmcurpatlen;
static uint8_t *curdata;

static int it_getpatlen (struct cpifaceSessionAPI_t *cpifaceSession, int n)
{
	return (plOrders[n]==0xFFFF)?0:plPatLens[plOrders[n]];
}

static void it_seektrack (struct cpifaceSessionAPI_t *cpifaceSession, int n, int c)
{
	xmcurpat=plPatterns[plOrders[n]];
	xmcurchan=c;
	xmcurrow=0;
	xmcurpatlen=plPatLens[plOrders[n]];
}

static int it_startrow (struct cpifaceSessionAPI_t *cpifaceSession)
{
	for (curdata=0; !curdata&&(xmcurrow<xmcurpatlen); xmcurrow++)
	{
		if (xmcurchan==-1)
		{
			if (*xmcurpat)
				curdata=xmcurpat;
			while (*xmcurpat)
				xmcurpat+=6;
		} else {
			while (*xmcurpat)
			{
				if (*xmcurpat==(xmcurchan+1))
					curdata=xmcurpat+1;
				xmcurpat+=6;
			}
		}
		xmcurpat++;
	}
	return curdata?(xmcurrow-1):-1;
}

static int it_getnote (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int small)
{
	int note=curdata[0];
	int porta;
	if (!note)
		return 0;
	porta=0;
	if ((curdata[3]==cmdPortaNote)||(curdata[3]==cmdPortaVol))
		porta=1;
	if ((curdata[2]>=cmdVPortaNote)&&(curdata[2]<(cmdVPortaNote+10)))
		porta=1;
	switch (small)
	{
		case 0:
			if (note>=cmdNNoteFade)
				cpifaceSession->console->WriteString (bp, 0, COLINS, (note==cmdNNoteOff)?"---":(note==cmdNNoteCut)?"^^^":"'''", 3);
			else {
				note-=cmdNNote;
				cpifaceSession->console->WriteString (bp, 0, porta?COLPTNOTE:COLNOTE, &"CCDDEFFGGAAB"[note%12], 1);
				cpifaceSession->console->WriteString (bp, 1, porta?COLPTNOTE:COLNOTE, &"-#-#--#-#-#-"[note%12], 1);
				cpifaceSession->console->WriteString (bp, 2, porta?COLPTNOTE:COLNOTE, &"0123456789"  [note/12], 1);
			}
			break;
		case 1:
			if (note>=cmdNNoteFade)
				cpifaceSession->console->WriteString (bp, 0, COLINS, (note==cmdNNoteOff)?"--":(note==cmdNNoteCut)?"^^":"''", 2);
			else {
				note-=cmdNNote;
				cpifaceSession->console->WriteString (bp, 0, porta?COLPTNOTE:COLNOTE, &"cCdDefFgGaAb"[note%12], 1);
				cpifaceSession->console->WriteString (bp, 1, porta?COLPTNOTE:COLNOTE, &"0123456789"  [note/12], 1);
			}
			break;
		case 2:
			if (note>=cmdNNoteFade)
				cpifaceSession->console->WriteString (bp, 0, COLINS, (note==cmdNNoteOff)?"-":(note==cmdNNoteCut)?"^":"'", 1);
			else {
				note-=cmdNNote;
				cpifaceSession->console->WriteString (bp, 0, porta?COLPTNOTE:COLNOTE, &"cCdDefFgGaAb"[note%12], 1);
			}
			break;
	}
	return 1;
}

static int it_getins (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	int ins=curdata[1];
	if (!ins)
		return 0;
	cpifaceSession->console->WriteNum (bp, 0, COLINS, ins, 16, 2, 0);
	return 1;
}

static int it_getvol (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	int vol=curdata[2];
	if ((vol>=cmdVVolume)&&(vol<=(cmdVVolume+64)))
	{
		cpifaceSession->console->WriteNum (bp, 0, COLVOL, vol-cmdVVolume, 16, 2, 0);
		return 1;
	}
	return 0;
}

static int it_getpan (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	int pan=curdata[2];
	if ((pan>=cmdVPanning)&&(pan<=(cmdVPanning+64)))
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, pan-cmdVPanning, 16, 2, 0);
		return 1;
	}
	if (curdata[3]==cmdPanning)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, (curdata[4]+1)>>2, 16, 2, 0);
		return 1;
	}
	if ((curdata[3]==cmdSpecial)&&((curdata[4]>>4)==cmdSPanning))
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, ((curdata[4]&0xF)*0x11+1)>>2, 16, 2, 0);
		return 1;
	}
	return 0;
}

static char *instfx[]={ "p-c","p-o","p-f","N:c","N:-","N:o","N:f","ve0",
                        "ve1","pe0","pe1","fe0","fe1","???","???","???" };

static void it_getfx (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int n)
{
	int data=curdata[2];

	int p=0;
	if ((data>=cmdVFVolSlU)&&(data<(cmdVFVolSlU+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLVOL, "+", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data-cmdVFVolSlU, 16, 2, 0);
	} else if ((data>=cmdVFVolSlD)&&(data<(cmdVFVolSlD+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLVOL, "-", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data-cmdVFVolSlD, 16, 2, 0);
	}
	else if ((data>=cmdVVolSlU)&&(data<(cmdVVolSlU+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x18", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data-cmdVVolSlU, 16, 2, 0);
	} else if ((data>=cmdVVolSlD)&&(data<(cmdVVolSlD+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x19", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data-cmdVVolSlD, 16, 2, 0);
	} else if ((data>=cmdVPortaNote)&&(data<(cmdVPortaNote+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, "\x00\x01\x04\x08\x10\x20\x40\x60\x80\xFF"[data-cmdVPortaNote], 16, 2, 0);
	} else if ((data>=cmdVPortaU)&&(data<(cmdVPortaU+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x18", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, (data-cmdVPortaU)*4, 16, 2, 0);
	} else if ((data>=cmdVPortaD)&&(data<(cmdVPortaD+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x19", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, (data-cmdVPortaD)*4, 16, 2, 0);
	} else if ((data>=cmdVVibrato)&&(data<(cmdVVibrato+10)))
	{
		cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~", 1);
		cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data-cmdVVibrato, 16, 2, 0);
	} else {
		bp-=3;
		p--;
	}
	bp+=3;
	p++;

	if (p==n)
		return;

	data=curdata[4];
	switch (curdata[3])
	{
		case cmdArpeggio:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\xf0", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case cmdVibrato:
		case cmdFineVib:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case cmdPanbrello:
			cpifaceSession->console->WriteString (bp, 0, COLPAN, "~", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data, 16, 2, 0);
			break;
		case cmdChanVol:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "V", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case cmdOffset:
			cpifaceSession->console->WriteString (bp, 0, COLACT, "\x1A", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
			break;
		case cmdRetrigger:
			cpifaceSession->console->WriteString (bp, 0, COLACT, "\x13", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
			break;
		case cmdTremolo:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "~", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case cmdTremor:
			cpifaceSession->console->WriteString (bp, 0, COLVOL, "\xA9", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data, 16, 2, 0);
			break;
		case cmdVolSlide:
		case cmdChanVolSlide:
			if (!data)
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x12""00", 3);
			else if ((data&0x0F)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x18", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data>>4, 16, 2, 0);
			} else if ((data&0xF0)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x19", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data&0xF, 16, 2, 0);
			} else if ((data&0x0F)==0x0F)
			{
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "+", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data>>4, 16, 2, 0);
			} else if ((data&0xF0)==0xF0)
			{
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "-", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data&0xF, 16, 2, 0);
			}
			break;
		case cmdPanSlide:
			if (!data)
				cpifaceSession->console->WriteString (bp, 0, COLPAN, "\x1D""00", 3);
			else if ((data&0x0F)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPAN, "\x1B", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data>>4, 16, 2, 0);
			} else if ((data&0xF0)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPAN, "\x1A", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data&0xF, 16, 2, 0);
			} else if ((data&0x0F)==0x0F)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPAN, "-", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data>>4, 16, 2, 0);
			} else if ((data&0xF0)==0xF0)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPAN, "+", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPAN, data&0xF, 16, 2, 0);
			}
			break;
		case cmdPortaVol:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D", 1);
			if (!data)
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x12""0", 2);
			else if ((data&0x0F)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x18", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data>>4, 16, 1, 0);
			} else if ((data&0xF0)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x19", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data&0xF, 16, 1, 0);
			} else if ((data&0x0F)==0x0F)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "+", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data>>4, 16, 1, 0);
			} else if ((data&0xF0)==0xF0)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "-", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data&0xF, 16, 1, 0);
			}
			break;
		case cmdVibVol:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~", 1);
			if (!data)
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x12""0", 2);
			else if ((data&0x0F)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x18", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data>>4, 16, 1, 0);
			} else if ((data&0xF0)==0x00)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "\x19", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data&0xF, 16, 1, 0);
			} else if ((data&0x0F)==0x0F)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "+", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data>>4, 16, 1, 0);
			} else if ((data&0xF0)==0xF0)
			{
				cpifaceSession->console->WriteString (bp, 1, COLVOL, "-", 1);
				cpifaceSession->console->WriteNum    (bp, 2, COLVOL, data&0xF, 16, 1, 0);
			}
			break;
		case cmdPortaNote:
			cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x0D", 1);
			cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			break;
		case cmdPortaU:
			if (data>=0xF0)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "+0", 2);
				cpifaceSession->console->WriteNum    (bp, 2, COLPITCH, data&0xF, 16, 1, 0);
			} else if (data>=0xE0)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "+x", 2);
				cpifaceSession->console->WriteNum    (bp, 2, COLPITCH, data&0xF, 16, 1, 0);
			} else {
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x18", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			}
			break;
		case cmdPortaD:
			if (data>=0xF0)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "-0", 2);
				cpifaceSession->console->WriteNum    (bp, 2, COLPITCH, data&0xF, 16, 1, 0);
			} else if (data>=0xE0)
			{
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "-x", 2);
				cpifaceSession->console->WriteNum    (bp, 2, COLPITCH, data&0xF, 16, 1, 0);
			} else {
				cpifaceSession->console->WriteString (bp, 0, COLPITCH, "\x19", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLPITCH, data, 16, 2, 0);
			}
			break;
		case cmdSpecial:
			if (!data)
			{
				cpifaceSession->console->WriteString (bp, 0, COLACT, "S00", 3);
				break;
			}
			data&=0xF;
			switch (curdata[4]>>4)
			{
				case cmdSVibType:
					if (data>=4)
						break;
					cpifaceSession->console->WriteString (bp, 0, COLPITCH, "~=", 2);
					cpifaceSession->console->WriteString (bp, 2, COLPITCH, &"~\\\xA9?"[data], 1);
					break;
				case cmdSTremType:
					if (data>=4)
						break;
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "~=", 2);
					cpifaceSession->console->WriteString (bp, 2, COLVOL, &"~\\\xA9?"[data], 1);
					break;
				case cmdSPanbrType:
					if (data>=4)
						break;
					cpifaceSession->console->WriteString (bp, 0, COLPAN, "~=", 2);
					cpifaceSession->console->WriteString (bp, 2, COLPAN, &"~\\\xA9?"[data], 1);
					break;
				case cmdSNoteCut:
					cpifaceSession->console->WriteString (bp, 0, COLACT, "^", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
					break;
				case cmdSNoteDelay:
					cpifaceSession->console->WriteString (bp, 0, COLACT, "d", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 2, 0);
					break;
				case cmdSSurround:
					cpifaceSession->console->WriteString (bp, 0, COLPAN, "srd", 3);
					break;
				case cmdSInstFX:
					cpifaceSession->console->WriteString (bp, 0, COLINS, instfx[data], 3);
					break;
				case cmdSOffsetHigh:
					cpifaceSession->console->WriteString (bp, 0, COLACT, "\x1A", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLACT, data, 16, 1, 0);
					cpifaceSession->console->WriteString (bp, 2, COLACT, "x", 1);
					break;
			}
			break;
	}
}

static void it_getgcmd (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int n)
{
	int p=0;
	while (*curdata)
	{
		int data;
		if (p==n)
			break;
		data=curdata[5];
		switch (curdata[4])
		{
			case cmdSpeed:
				cpifaceSession->console->WriteString (bp, 0, COLSPEED, "s", 1);
				cpifaceSession->console->WriteNum    (bp, 1, COLSPEED, data, 16, 2, 0);
				break;
			case cmdTempo:
				cpifaceSession->console->WriteString (bp, 0, COLSPEED, "b", 1);
				if ((data>=0x20)||!data||(data==0x10))
				{
					cpifaceSession->console->WriteNum (bp, 1, COLSPEED, data, 16, 2, 0);
				} else {
					cpifaceSession->console->WriteString (bp, 1, COLSPEED, &"-+"[data>>4], 1);
					cpifaceSession->console->WriteNum    (bp, 2, COLSPEED, data&0xF, 16, 1, 0);
				}
				break;
			case cmdJump:
				cpifaceSession->console->WriteString (bp, 0, COLACT, "\x1A", 1);
				cpifaceSession->console->WriteNum (bp, 1, COLACT, data, 16, 2, 0);
				break;
			case cmdBreak:
				cpifaceSession->console->WriteString (bp, 0, COLACT, "\x19", 1);
				cpifaceSession->console->WriteNum (bp, 1, COLACT, data, 16, 2, 0);
				break;
			case cmdGVolume:
				cpifaceSession->console->WriteString (bp, 0, COLVOL, "v", 1);
				cpifaceSession->console->WriteNum (bp, 1, COLVOL, data, 16, 2, 0);
				break;
			case cmdGVolSlide:
				if (!data)
				{
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x12""00", 3);
				} else if ((data&0x0F)==0x00)
				{
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x18", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data>>4, 16, 2, 0);
				} else if ((data&0xF0)==0x00)
				{
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "\x19", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data&0xF, 16, 2, 0);
				} else if ((data&0x0F)==0x0F)
				{
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "+", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data>>4, 16, 2, 0);
				} else if ((data&0xF0)==0xF0)
				{
					cpifaceSession->console->WriteString (bp, 0, COLVOL, "-", 1);
					cpifaceSession->console->WriteNum    (bp, 1, COLVOL, data&0xF, 16, 2, 0);
				}
				break;
			case cmdSpecial:
				data&=0xF;
				switch (curdata[5]>>4)
				{
					case cmdSPatLoop:
						cpifaceSession->console->WriteString (bp, 0, COLACT, "pl", 2);
						cpifaceSession->console->WriteNum    (bp, 2, COLACT, data, 16, 1, 0);
						break;
					case cmdSPatDelayRow:
						cpifaceSession->console->WriteString (bp, 0, COLACT, "dr", 2);
						cpifaceSession->console->WriteNum    (bp, 2, COLACT, data, 16, 1, 0);
						break;
					case cmdSPatDelayTick:
						cpifaceSession->console->WriteString (bp, 0, COLACT, "dt", 2);
						cpifaceSession->console->WriteNum    (bp, 2, COLACT, data, 16, 1, 0);
						break;
					default:
						bp-=4;
						p--;
				}
				break;
			default:
				bp-=4;
				p--;
		}
		bp+=4;
		p++;
		curdata+=6;
	}
}

static const char *it_getpatname (struct cpifaceSessionAPI_t *cpifaceSession, int unused)
{
	return 0;
}

static int it_getcurpos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return getrealpos (cpifaceSession, &itplayer)>>8;
}

static struct cpitrakdisplaystruct xmtrakdisplay=
{
	it_getcurpos, it_getpatlen, it_getpatname, it_seektrack, it_startrow, it_getnote,
	it_getins, it_getvol, it_getpan, it_getfx, it_getgcmd
};

OCP_INTERNAL void itTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct it_module *mod)
{
	plPatterns=mod->patterns;
	plOrders=mod->orders;
	plPatLens=mod->patlens;
	cpifaceSession->TrackSetup (cpifaceSession, &xmtrakdisplay, mod->nord);
}
