/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlay Instrument display code
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
 *    -changed some minor things for better output
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"
#include "itplay.h"

static int instnum;
static int sampnum;
static uint8_t *plInstUsed = NULL;
static uint8_t *plSampUsed = NULL;
static uint8_t *plBigInstNum = NULL;
static uint16_t *plBigSampNum = NULL;

static const struct it_instrument *plInstr;
static const struct it_sampleinfo *plSamples;
static const struct it_sample *plModSamples;

static uint8_t plInstShowFreq;

static void itDisplayIns40 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf, 1, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":plInstr[n].name, 35);
}

static void itDisplayIns33 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf, 1, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":plInstr[n].name, 28);
}

static void itDisplayIns52 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"    \xfe##: ":"     ##: ", 9);
	cpifaceSession->console->WriteNum    (buf, 5, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 9, col, compoMode?"#":plInstr[n].name, 43);
}

static void itDisplayIns80 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col;
	cpifaceSession->console->WriteString (buf, 0, 0, "", 80);

	if (plBigInstNum[n]!=0xFF)
	{
		const struct it_instrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[plBigInstNum[n]]];
		cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		cpifaceSession->console->WriteNum    (buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":ins->name, 31);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct it_sample *sm=&plModSamples[plBigSampNum[n]];
		const struct it_sampleinfo *si=&plSamples[sm->handle];

		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plSampUsed[plBigSampNum[n]]];
		cpifaceSession->console->WriteString (buf, 34, col, (!plInstMode&&plSampUsed[plBigSampNum[n]])?"\xfe###: ":" ###: ", 6);
		cpifaceSession->console->WriteNum (buf, 35, col, plBigSampNum[n], 16, 3, 0);
		if (si->type&mcpSampLoop)
		{
			cpifaceSession->console->WriteNum (buf, 40, col, si->loopend, 10, 6, 1);
			cpifaceSession->console->WriteNum (buf, 47, col, si->loopend-si->loopstart, 10, 6, 1);
			if (si->type&mcpSampBiDi)
				cpifaceSession->console->WriteString (buf, 53, col, "\x1D", 1);
		} else {
			cpifaceSession->console->WriteNum    (buf, 40, col, si->length, 10, 6, 1);
			cpifaceSession->console->WriteString (buf, 52, col, "-", 1);
		}
		cpifaceSession->console->WriteString (buf, 55, col, (si->type&mcpSamp16Bit)?"16":" 8", 2);
		cpifaceSession->console->WriteString (buf, 57, col, (si->type&mcpSampRedRate4)?"\xac":(si->type&mcpSampRedRate2)?"\xab":(si->type&mcpSampRedBits)?"!":" ", 2);

		if (!plInstShowFreq)
		{
			cpifaceSession->console->WriteString (buf, 60, col, cpifaceSession->plNoteStr((sm->normnote+60*256)>>8), 3);
			cpifaceSession->console->WriteNum    (buf, 64, col, sm->normnote&0xFF, 16, 2, 0);
		} else
			if (plInstShowFreq==1)
				cpifaceSession->console->WriteNum (buf, 60, col, cpifaceSession->mcpAPI->GetFreq8363(-sm->normnote), 10, 6, 1);
			else
				cpifaceSession->console->WriteNum (buf, 60, col, si->samprate, 10, 6, 1);

		cpifaceSession->console->WriteNum (buf, 68, col, sm->vol, 16, 2, 0);
/*
		if (ins->stdpan!=-1)
				cpifaceSession->console->WriteNum (buf, 72, col, ins->stdpan, 16, 2, 0);
			else
				cpifaceSession->console->WriteString (buf, 72, col, " -", 2);
		if (ins->volenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 76, col, "v", 1);
		if (ins->panenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 77, col, "p", 1);
		if (ins->vibdepth&&sm.vibrate)
			cpifaceSession->console->WriteString (buf, 78, col, "~", 1);
		if (ins->fadeout)
			cpifaceSession->console->WriteString (buf, 79, col, "\x19", 1);
*/
	}
}

static void itDisplayIns132 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col;
	cpifaceSession->console->WriteString (buf, 0, 0, "", 132);

	if (plBigInstNum[n]!=0xFF)
	{
		const struct it_instrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[plBigInstNum[n]]];
		cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		cpifaceSession->console->WriteNum    (buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":ins->name, 35);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct it_sample *sm=&plModSamples[plBigSampNum[n]];
		const struct it_sampleinfo *si=&plSamples[sm->handle];

		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plSampUsed[plBigSampNum[n]]];
		cpifaceSession->console->WriteString (buf, 34, col, (!plInstMode&&plSampUsed[plBigSampNum[n]])?"\xfe###: ":" ###: ", 6);
		cpifaceSession->console->WriteNum    (buf, 35, col, plBigSampNum[n], 16, 3, 0);
		cpifaceSession->console->WriteString (buf, 40, col, compoMode?"#":sm->name, 28);
		if (si->type&mcpSampLoop)
		{
			cpifaceSession->console->WriteNum (buf, 70, col, si->loopend, 10, 6, 1);
			cpifaceSession->console->WriteNum (buf, 77, col, si->loopend-si->loopstart, 10, 6, 1);
			if (si->type&mcpSampBiDi)
				cpifaceSession->console->WriteString (buf, 83, col, "\x1D", 1);
		} else {
			cpifaceSession->console->WriteNum    (buf, 70, col, si->length, 10, 6, 1);
			cpifaceSession->console->WriteString (buf, 82, col, "-", 1);
		}
		cpifaceSession->console->WriteString (buf, 85, col, (si->type&mcpSamp16Bit)?"16":" 8", 2);
		cpifaceSession->console->WriteString (buf, 87, col, (si->type&mcpSampRedRate4)?"\xac":(si->type&mcpSampRedRate2)?"\xab":(si->type&mcpSampRedBits)?"!":" ", 2);

		if (!plInstShowFreq)
		{
			cpifaceSession->console->WriteString (buf, 90, col, cpifaceSession->plNoteStr((sm->normnote+60*256)>>8), 3);
			cpifaceSession->console->WriteNum    (buf, 94, col, sm->normnote&0xFF, 16, 2, 0);
		} else if (plInstShowFreq==1)
			cpifaceSession->console->WriteNum    (buf, 90, col, cpifaceSession->mcpAPI->GetFreq8363(-sm->normnote), 10, 6, 1);
		else
			cpifaceSession->console->WriteNum    (buf, 90, col, si->samprate, 10, 6, 1);

		cpifaceSession->console->WriteNum (buf, 98, col, sm->vol, 16, 2, 0);
/*
		if (ins->stdpan!=-1)
			cpifaceSession->console->WriteNum    (buf, 102, col, ins->stdpan, 16, 2, 0);
		else
			cpifaceSession->console->WriteString (buf, 102, col, " -", 2);

		if (ins->volenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 106, col, "v", 1);
		if (ins->panenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 107, col, "p", 1);
		if (ins->vibdepth&&ins->vibrate)
			cpifaceSession->console->WriteString (buf, 108, col, "~", 1);

		if (ins->fadeout)
			cpifaceSession->console->WriteNum    (buf, 110, col, ins->fadeout, 16, 4, 1);
		else
			cpifaceSession->console->WriteString (buf, 113, col, "-", 1);
 */
	}
}

static void itDisplayIns (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiInstWidth width, int n, int plInstMode, int compoMode)
{
	switch (width)
	{
		case cpiInstWidth_33:
			itDisplayIns33 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_40:
			itDisplayIns40 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_52:
			itDisplayIns52 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_80:
			itDisplayIns80 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_132:
			itDisplayIns132 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
	}
}

static void (*Mark) (struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *, uint8_t *);

static void itMark (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;
	for (i=0; i<instnum; i++)
		if (plInstUsed[i])
			plInstUsed[i]=1;
	for (i=0; i<sampnum; i++)
		if (plSampUsed[i])
			plSampUsed[i]=1;
	Mark (cpifaceSession, plInstUsed, plSampUsed);
}

OCP_INTERNAL void itpInstClear (struct cpifaceSessionAPI_t *cpifaceSession)
{
	memset(plInstUsed, 0, instnum);
	memset(plSampUsed, 0, sampnum);
}

static void Done (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (plInstUsed)
	{
		free(plInstUsed);
		plInstUsed=NULL;
	}
	if (plSampUsed)
	{
		free(plSampUsed);
		plSampUsed=NULL;
	}
	if (plBigInstNum)
	{
		free(plBigInstNum);
		plBigInstNum=NULL;
	}
	if (plBigSampNum)
	{
		free(plBigSampNum);
		plBigSampNum=NULL;
	}
}

OCP_INTERNAL void itpInstSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct it_instrument *ins, int nins, const struct it_sample *smp, int nsmp, const struct it_sampleinfo *smpi, int type, void (*MarkyBoy)(struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *, uint8_t *))
{
	int i,j;
	int biginstlen=0;
	struct insdisplaystruct plInsDisplay;

	instnum=nins;
	sampnum=nsmp;

	plSampUsed=malloc(sizeof(uint8_t)*sampnum);
	plInstUsed=malloc(sizeof(uint8_t)*instnum);
	if (!plSampUsed||!plInstUsed)
		return;

	itpInstClear (cpifaceSession);

	Mark=MarkyBoy;

	plInstr=ins;
	plModSamples=smp;
	plSamples=smpi;

	for (i=0; i<instnum; i++)
	{
		const struct it_instrument *ins=&plInstr[i];
		int num=0;

		for (j=0; j<IT_KEYTABS; j++)
			if (ins->keytab[j][1]&&(ins->keytab[j][1]<=sampnum))
				if (plModSamples[ins->keytab[j][1]-1].handle<nsmp)
					plSampUsed[ins->keytab[j][1]-1]=1;
		for (j=0; j<sampnum; j++)
			if (plSampUsed[j])
				num++;
		biginstlen+=num?num:1;
	}
	plBigInstNum=malloc(sizeof(uint8_t)*biginstlen);
	plBigSampNum=malloc(sizeof(uint16_t)*biginstlen);
	if (!plBigInstNum||!plBigSampNum)
		return;
	memset(plBigInstNum, -1, biginstlen);
	memset(plBigSampNum, -1, biginstlen*2);

	biginstlen=0;
	for (i=0; i<instnum; i++)
	{
		const struct it_instrument *ins=&plInstr[i];
		int num=0;

		memset(plSampUsed, 0, sampnum);

		for (j=0; j<IT_KEYTABS; j++)
			if (ins->keytab[j][1]&&(ins->keytab[j][1]<=sampnum))
				if (plModSamples[ins->keytab[j][1]-1].handle<nsmp)
					plSampUsed[ins->keytab[j][1]-1]=1;

		plBigInstNum[biginstlen]=i;
		for (j=0; j<sampnum; j++)
			if (plSampUsed[j])
				plBigSampNum[biginstlen+num++]=j;

		biginstlen+=num?num:1;
	}

	plInstShowFreq=type;
	plInsDisplay.Clear=itpInstClear;
	plInsDisplay.height=instnum;
	plInsDisplay.bigheight=biginstlen;
	plInsDisplay.title80=plInstShowFreq?
                         " ##   instrument name / song message    length replen bit samprate vol pan  flgs":
                         " ##   instrument name / song message    length replen bit  base ft vol pan  flgs";
	plInsDisplay.title132=plInstShowFreq?
                          " ##   instrument name / song message       sample name                length replen bit samprate vol pan  fl  fade           ":
                          " ##   instrument name / song message       sample name                length replen bit  base ft vol pan  fl  fade           ";

	plInsDisplay.Mark=itMark;
	plInsDisplay.Display=itDisplayIns;
	plInsDisplay.Done=Done;
	itpInstClear (cpifaceSession);
	cpifaceSession->UseInstruments (cpifaceSession, &plInsDisplay);
}
