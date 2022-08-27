/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay instrument display routines
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
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "gmdinst.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

static int instnum;
static int sampnum;
static uint8_t *plInstUsed;
static uint8_t *plSampUsed;
static unsigned char *plBigInstNum;
static unsigned short *plBigSampNum;

/*extern char plNoteStr[132][4]; */

static const struct gmdinstrument *plInstr;
static const struct sampleinfo *plSamples;
static const struct gmdsample *plModSamples;

static char plInstShowFreq;

static void gmdDisplayIns40 (struct cpifaceSessionAPI_t *cpifaceSession, unsigned short *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum(buf, 1, col, n+1, 16, 2, 0);
	writestring(buf, 5, col, plInstr[n].name, 35);
}

static void gmdDisplayIns33 (struct cpifaceSessionAPI_t *cpifaceSession, unsigned short *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum(buf, 1, col, n+1, 16, 2, 0);
	writestring(buf, 5, col, plInstr[n].name, 28);
}

static void gmdDisplayIns52 (struct cpifaceSessionAPI_t *cpifaceSession, unsigned short *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"    \xfe##: ":"     ##: ", 9);
	writenum(buf, 5, col, n+1, 16, 2, 0);
	writestring(buf, 9, col, plInstr[n].name, 43);
}

static void gmdDisplayIns80 (struct cpifaceSessionAPI_t *cpifaceSession, unsigned short *buf, int n, int plInstMode)
{
	char col;
	writestring(buf, 0, 0, "", 80);
	if (plBigInstNum[n]!=0xFF)
	{
		const struct gmdinstrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[plBigInstNum[n]]];
		writestring(buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		writenum(buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		writestring(buf, 5, col, ins->name, 31);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct gmdsample *sm=&plModSamples[plBigSampNum[n]];
		const struct sampleinfo *si;
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plSampUsed[plBigSampNum[n]]];
		writestring(buf, 34, col, (!plInstMode&&plSampUsed[plBigSampNum[n]])?"\xfe###: ":" ###: ", 6);
		writenum(buf, 35, col, plBigSampNum[n], 16, 3, 0);
		si=&plSamples[sm->handle];
		if (si->type&mcpSampLoop)
		{
			_writenum(buf, 40, col, si->loopend, 10, 6);
			_writenum(buf, 47, col, si->loopend-si->loopstart, 10, 6);
			if (si->type&mcpSampBiDi)
				writestring(buf, 53, col, "\x1D", 1);
		} else {
			_writenum(buf, 40, col, si->length, 10, 6);
			writestring(buf, 52, col, "-", 1);
		}
		writestring(buf, 55, col, (si->type&mcpSamp16Bit)?"16":" 8", 2);
		writestring(buf, 57, col, (si->type&mcpSampRedRate4)?"\xac":(si->type&mcpSampRedRate2)?"\xab":(si->type&mcpSampRedBits)?"!":" ", 2);

		if (!plInstShowFreq)
		{
			writestring(buf, 60, col, plNoteStr[(sm->normnote+60*256)>>8], 3);
			writenum(buf, 64, col, sm->normnote&0xFF, 16, 2, 0);
		} else
			if (plInstShowFreq==1)
				writenum(buf, 60, col, cpifaceSession->mcpAPI->GetFreq8363(-sm->normnote), 10, 6, 1);
			else
				writenum(buf, 60, col, si->samprate, 10, 6, 1);

		if (sm->stdvol!=-1)
			writenum(buf, 68, col, sm->stdvol, 16, 2, 0);
		else
			writestring(buf, 68, col, " -", 2);
		if (sm->stdpan!=-1)
			writenum(buf, 72, col, sm->stdpan, 16, 2, 0);
		else
			writestring(buf, 72, col, " -", 2);

		if (sm->volenv!=0xFFFF)
			writestring(buf, 76, col, "v", 1);
		if (sm->panenv!=0xFFFF)
			writestring(buf, 77, col, "p", 1);
		if (sm->vibdepth&&sm->vibrate)
			writestring(buf, 78, col, "~", 1);
		if (sm->volfade&&(sm->volfade!=0xFFFF))
			writestring(buf, 79, col, "\x19", 1);
	}
}

static void gmdDisplayIns132 (struct cpifaceSessionAPI_t *cpifaceSession, unsigned short *buf, int n, int plInstMode)
{
	char col;

	writestring(buf, 0, 0, "", 132);
	if (plBigInstNum[n]!=0xFF)
	{
		const struct gmdinstrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[plBigInstNum[n]]];
		writestring(buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		writenum(buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		writestring(buf, 5, col, ins->name, 35);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct gmdsample *sm=&plModSamples[plBigSampNum[n]];
		const struct sampleinfo *si;
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plSampUsed[plBigSampNum[n]]];
		writestring(buf, 34, col, (!plInstMode&&plSampUsed[plBigSampNum[n]])?"\xfe###: ":" ###: ", 6);
		writenum(buf, 35, col, plBigSampNum[n], 16, 3, 0);
		writestring(buf, 40, col, sm->name, 28);
		si=&plSamples[sm->handle];
		if (si->type&mcpSampLoop)
		{
			_writenum(buf, 70, col, si->loopend, 10, 6);
			_writenum(buf, 77, col, si->loopend-si->loopstart, 10, 6);
			if (si->type&mcpSampBiDi)
				writestring(buf, 83, col, "\x1D", 1);
		} else {
			_writenum(buf, 70, col, si->length, 10, 6);
			writestring(buf, 82, col, "-", 1);
		}
		writestring(buf, 85, col, (si->type&mcpSamp16Bit)?"16":" 8", 2);
		writestring(buf, 87, col, (si->type&mcpSampRedRate4)?"\xac":(si->type&mcpSampRedRate2)?"\xab":(si->type&mcpSampRedBits)?"!":" ", 2);

		if (!plInstShowFreq)
		{
			writestring(buf, 90, col, plNoteStr[(sm->normnote+60*256)>>8], 3);
			writenum(buf, 94, col, sm->normnote&0xFF, 16, 2, 0);
		} else if (plInstShowFreq==1)
			writenum(buf, 90, col, cpifaceSession->mcpAPI->GetFreq8363(-sm->normnote), 10, 6, 1);
		else
			writenum(buf, 90, col, si->samprate, 10, 6, 1);

		if (sm->stdvol!=-1)
			writenum(buf, 98, col, sm->stdvol, 16, 2, 0);
		else
			writestring(buf, 98, col, " -", 2);
		if (sm->stdpan!=-1)
			writenum(buf, 102, col, sm->stdpan, 16, 2, 0);
		else
			writestring(buf, 102, col, " -", 2);

		if (sm->volenv!=0xFFFF)
			writestring(buf, 106, col, "v", 1);
		if (sm->panenv!=0xFFFF)
			writestring(buf, 107, col, "p", 1);
		if (sm->vibdepth&&sm->vibrate)
			writestring(buf, 108, col, "~", 1);

		if (sm->volfade&&(sm->volfade!=0xFFFF))
			writenum(buf, 110, col, sm->volfade, 16, 4, 1);
		else
			writestring(buf, 113, col, "-", 1);
	}
}

static void gmdDisplayIns (struct cpifaceSessionAPI_t *cpifaceSession, unsigned short *buf, int len, int n, int plInstMode)
{
	switch (len)
	{
		case 33:
			gmdDisplayIns33 (cpifaceSession, buf, n, plInstMode);
			break;
		case 40:
			gmdDisplayIns40 (cpifaceSession, buf, n, plInstMode);
			break;
		case 52:
			gmdDisplayIns52 (cpifaceSession, buf, n, plInstMode);
			break;
		case 80:
			gmdDisplayIns80 (cpifaceSession, buf, n, plInstMode);
			break;
		case 132:
			gmdDisplayIns132 (cpifaceSession, buf, n, plInstMode);
			break;
	}
}

static void (*Mark) (struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *, uint8_t *);

static void gmdMark (struct cpifaceSessionAPI_t *cpifaceSession)
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

void __attribute__ ((visibility ("internal"))) gmdInstClear (struct cpifaceSessionAPI_t *cpifaceSession)
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

void __attribute__ ((visibility ("internal"))) gmdInstSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct gmdinstrument *ins, int nins, const struct gmdsample *smp, int nsmp, const struct sampleinfo *smpi, int _notused, int type, void (*MarkyBoy)(struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *, uint8_t *))
{
	int i,j;
	int biginstlen;
	struct insdisplaystruct plInsDisplay;

	instnum=nins;
	sampnum=nsmp;
	plSampUsed=malloc(sampnum);
	plInstUsed=malloc(instnum);
	if (!plSampUsed||!plInstUsed)
		return;

	Mark=MarkyBoy;

	plInstr=ins;
	plModSamples=smp;
	plSamples=smpi;

	biginstlen=0;
	for (i=0; i<instnum; i++)
	{
		const struct gmdinstrument *ins=&plInstr[i];
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
	plBigInstNum=malloc(biginstlen);
	plBigSampNum=malloc(sizeof(short)*biginstlen);
	if (!plBigInstNum||!plBigSampNum)
		return;
	memset(plBigInstNum, -1, biginstlen);
	memset(plBigSampNum, -1, biginstlen*2);

	biginstlen=0;
	for (i=0; i<instnum; i++)
	{
		const struct gmdinstrument *ins=&plInstr[i];
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

	plInstShowFreq=type;
	plInsDisplay.Clear=gmdInstClear;
	plInsDisplay.title80=plInstShowFreq?
		" ##   instrument name / song message    length replen bit samprate vol pan  flgs":
		" ##   instrument name / song message    length replen bit  base ft vol pan  flgs";
	plInsDisplay.title132=plInstShowFreq?
		" ##   instrument name / song message       sample name                length replen bit samprate vol pan  fl  fade           ":
		" ##   instrument name / song message       sample name                length replen bit  base ft vol pan  fl  fade           ";
	plInsDisplay.Mark=gmdMark;
	plInsDisplay.Display=gmdDisplayIns;
	plInsDisplay.Done=Done;
	gmdInstClear (cpifaceSession);
	plInsDisplay.height=instnum;
	plInsDisplay.bigheight=biginstlen;
	cpifaceSession->UseInstruments (cpifaceSession, &plInsDisplay);
}
