/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay instrument display routines
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 *     therefore I had to add this file and search/replace some identifiers
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "xmplay.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

static int instnum;
static int sampnum;
static char *plInstUsed;
static char *plSampUsed;
static uint8_t *plBigInstNum;
static uint16_t *plBigSampNum;

static const struct xmpinstrument *plInstr;
static const struct sampleinfo *plSamples;
static const struct xmpsample *plModSamples;

static char plInstShowFreq;

static void xmpDisplayIns40 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf, 1, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":plInstr[n].name, 35);
}

static void xmpDisplayIns33 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf, 1, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":plInstr[n].name, 28);
}

static void xmpDisplayIns52 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"    \xfe##: ":"     ##: ", 9);
	cpifaceSession->console->WriteNum    (buf, 5, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 9, col, compoMode?"#":plInstr[n].name, 43);
}

static void xmpDisplayIns80 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col;
	cpifaceSession->console->WriteString (buf, 0, 0, "", 80);

	if (plBigInstNum[n]!=0xFF)
	{
		const struct xmpinstrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[plBigInstNum[n]]];
		cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		cpifaceSession->console->WriteNum    (buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":ins->name, 31);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct xmpsample *sm=&plModSamples[plBigSampNum[n]];
		const struct sampleinfo *si=&plSamples[sm->handle];

		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plSampUsed[plBigSampNum[n]]];
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
			cpifaceSession->console->WriteString (buf, 60, col, plNoteStr[(sm->normnote+60*256)>>8], 3);
			cpifaceSession->console->WriteNum    (buf, 64, col, sm->normnote&0xFF, 16, 2, 0);
		} else {
			if (plInstShowFreq==1)
				cpifaceSession->console->WriteNum (buf, 60, col, cpifaceSession->mcpAPI->GetFreq8363(-sm->normnote), 10, 6, 1);
			else
				cpifaceSession->console->WriteNum (buf, 60, col, si->samprate, 10, 6, 1);
		}
		if (sm->stdvol!=-1)
			cpifaceSession->console->WriteNum    (buf, 68, col, sm->stdvol, 16, 2, 0);
		else
			cpifaceSession->console->WriteString (buf, 68, col, " -", 2);
		if (sm->stdpan!=-1)
			cpifaceSession->console->WriteNum    (buf, 72, col, sm->stdpan, 16, 2, 0);
		else
			cpifaceSession->console->WriteString (buf, 72, col, " -", 2);

		if (sm->volenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 76, col, "v", 1);
		if (sm->panenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 77, col, "p", 1);
		if (sm->vibdepth&&sm->vibrate)
			cpifaceSession->console->WriteString (buf, 78, col, "~", 1);
		if (sm->volfade&&(sm->volfade!=0xFFFF))
			cpifaceSession->console->WriteString (buf, 79, col, "\x19", 1);
	}
}

static void xmpDisplayIns132 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col;
	cpifaceSession->console->WriteString (buf, 0, 0, "", 132);

	if (plBigInstNum[n]!=0xFF)
	{
		const struct xmpinstrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[plBigInstNum[n]]];
		cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		cpifaceSession->console->WriteNum (buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":ins->name, 35);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct xmpsample *sm=&plModSamples[plBigSampNum[n]];
		const struct sampleinfo *si=&plSamples[sm->handle];

		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plSampUsed[plBigSampNum[n]]];
		cpifaceSession->console->WriteString (buf, 34, col, (!plInstMode&&plSampUsed[plBigSampNum[n]])?"\xfe###: ":" ###: ", 6);
		cpifaceSession->console->WriteNum (buf, 35, col, plBigSampNum[n], 16, 3, 0);
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
			cpifaceSession->console->WriteString (buf, 90, col, plNoteStr[(sm->normnote+60*256)>>8], 3);
			cpifaceSession->console->WriteNum    (buf, 94, col, sm->normnote&0xFF, 16, 2, 0);
		} else
			if (plInstShowFreq==1)
				cpifaceSession->console->WriteNum (buf, 90, col, cpifaceSession->mcpAPI->GetFreq8363(-sm->normnote), 10, 6, 1);
			else
				cpifaceSession->console->WriteNum (buf, 90, col, si->samprate, 10, 6, 1);

		if (sm->stdvol!=-1)
			cpifaceSession->console->WriteNum    (buf, 98, col, sm->stdvol, 16, 2, 0);
		else
			cpifaceSession->console->WriteString (buf, 98, col, " -", 2);
		if (sm->stdpan!=-1)
			cpifaceSession->console->WriteNum    (buf, 102, col, sm->stdpan, 16, 2, 0);
		else
			cpifaceSession->console->WriteString (buf, 102, col, " -", 2);

		if (sm->volenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 106, col, "v", 1);
		if (sm->panenv!=0xFFFF)
			cpifaceSession->console->WriteString (buf, 107, col, "p", 1);
		if (sm->vibdepth&&sm->vibrate)
			cpifaceSession->console->WriteString (buf, 108, col, "~", 1);

		if (sm->volfade&&(sm->volfade!=0xFFFF))
			cpifaceSession->console->WriteNum    (buf, 110, col, sm->volfade, 16, 4, 1);
		else
			cpifaceSession->console->WriteString (buf, 113, col, "-", 1);
	}
}

static void xmpDisplayIns (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiInstWidth width, int n, int plInstMode, int compoMode)
{
	switch (width)
	{
		case cpiInstWidth_33:
			xmpDisplayIns33 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_40:
			xmpDisplayIns40 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_52:
			xmpDisplayIns52 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_80:
			xmpDisplayIns80 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_132:
			xmpDisplayIns132 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
	}
}

static void (*Mark) (struct cpifaceSessionAPI_t *cpifaceSession, char *, char *);

static void xmpMark (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;
	for (i=0; i<instnum; i++)
		if (plInstUsed[i])
			plInstUsed[i]=1;
	for (i=0; i<sampnum; i++)
		if (plSampUsed[i])
			plSampUsed[i]=1;
	Mark(cpifaceSession, plInstUsed, plSampUsed);
}

void __attribute__ ((visibility ("internal"))) xmpInstClear (struct cpifaceSessionAPI_t *cpifaceSession)
{
	memset(plInstUsed, 0, instnum);
	memset(plSampUsed, 0, sampnum);
}

static void Done (struct cpifaceSessionAPI_t *cpifaceSession)
{
	free(plInstUsed);
	free(plSampUsed);
	free(plBigInstNum);
	free(plBigSampNum);
}

void __attribute__ ((visibility ("internal"))) xmpInstSetup(struct cpifaceSessionAPI_t *cpifaceSession, const struct xmpinstrument *ins, int nins, const struct xmpsample *smp, int nsmp, const struct sampleinfo *smpi, int dummy, int type, void (*MarkyBoy)(struct cpifaceSessionAPI_t *cpifaceSession, char *, char *))
{
	int i,j;
	int biginstlen=0;

	instnum=nins;
	sampnum=nsmp;
	plSampUsed=malloc(sizeof(char)*sampnum);
	plInstUsed=malloc(sizeof(char)*instnum);
	if (!plSampUsed||!plInstUsed)
		return;

	Mark=MarkyBoy;

	plInstr=ins;
	plModSamples=smp;
	plSamples=smpi;

	for (i=0; i<instnum; i++)
	{
		const struct xmpinstrument *ins=&plInstr[i];
		int num=0;

		memset(plSampUsed, 0, sampnum);
		for (j=0; j<128; j++)
			if (ins->samples[j]<sampnum)
				if (plModSamples[ins->samples[j]].handle<nsmp)
					plSampUsed[ins->samples[j]]=1;
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
		const struct xmpinstrument *ins=&plInstr[i];
		int num=0;

		memset(plSampUsed, 0, sampnum);
		for (j=0; j<128; j++)
			if (ins->samples[j]<sampnum)
				if (plModSamples[ins->samples[j]].handle<nsmp)
					plSampUsed[ins->samples[j]]=1;

		plBigInstNum[biginstlen]=i;
		for (j=0; j<sampnum; j++)
			if (plSampUsed[j])
				plBigSampNum[biginstlen+num++]=j;
		biginstlen+=num?num:1;
	}

	{
		struct insdisplaystruct plInsDisplay;
		plInstShowFreq=type;
		plInsDisplay.Clear=xmpInstClear;
		plInsDisplay.height=instnum;
		plInsDisplay.bigheight=biginstlen;
		plInsDisplay.title80=plInstShowFreq?
			" ##   instrument name / song message    length replen bit samprate vol pan  flgs":
			" ##   instrument name / song message    length replen bit  base ft vol pan  flgs";
		plInsDisplay.title132=plInstShowFreq?
			" ##   instrument name / song message       sample name                length replen bit samprate vol pan  fl  fade           ":
			" ##   instrument name / song message       sample name                length replen bit  base ft vol pan  fl  fade           ";
		plInsDisplay.Mark=xmpMark;
		plInsDisplay.Display=xmpDisplayIns;
		plInsDisplay.Done=Done;
		xmpInstClear (cpifaceSession);
		cpifaceSession->UseInstruments (cpifaceSession, &plInsDisplay);
	}
}
