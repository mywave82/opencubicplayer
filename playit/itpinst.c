/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

/* extern uint8_t plNoteStr[132][4];  cpiface.h */

static const struct it_instrument *plInstr;
static const struct it_sampleinfo *plSamples;
static const struct it_sample *plModSamples;

static uint8_t plInstShowFreq;

static void itDisplayIns40(uint16_t *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum(buf, 1, col, n+1, 16, 2, 0);
	writestring(buf, 5, col, plInstr[n].name, 35);
}

static void itDisplayIns33(uint16_t *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum(buf, 1, col, n+1, 16, 2, 0);
	writestring(buf, 5, col, plInstr[n].name, 28);
}

static void itDisplayIns52(uint16_t *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"    \xfe##: ":"     ##: ", 9);
	writenum(buf, 5, col, n+1, 16, 2, 0);
	writestring(buf, 9, col, plInstr[n].name, 43);
}

static void itDisplayIns80(uint16_t *buf, int n, int plInstMode)
{
	char col;
	writestring(buf, 0, 0, "", 80);

	if (plBigInstNum[n]!=0xFF)
	{
		const struct it_instrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[plBigInstNum[n]]];
		writestring(buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		writenum(buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		writestring(buf, 5, col, ins->name, 31);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct it_sample *sm=&plModSamples[plBigSampNum[n]];
		const struct it_sampleinfo *si=&plSamples[sm->handle];

		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plSampUsed[plBigSampNum[n]]];
		writestring(buf, 34, col, (!plInstMode&&plSampUsed[plBigSampNum[n]])?"\xfe###: ":" ###: ", 6);
		writenum(buf, 35, col, plBigSampNum[n], 16, 3, 0);
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
				writenum(buf, 60, col, mcpGetFreq8363(-sm->normnote), 10, 6, 1);
			else
				writenum(buf, 60, col, si->samprate, 10, 6, 1);

		writenum(buf, 68, col, sm->vol, 16, 2, 0);
/*
		if (ins->stdpan!=-1)
				writenum(buf, 72, col, ins->stdpan, 16, 2, 0);
			else
				writestring(buf, 72, col, " -", 2);
		if (ins->volenv!=0xFFFF)
			writestring(buf, 76, col, "v", 1);
		if (ins->panenv!=0xFFFF)
			writestring(buf, 77, col, "p", 1);
		if (ins->vibdepth&&sm.vibrate)
			writestring(buf, 78, col, "~", 1);
		if (ins->fadeout)
			writestring(buf, 79, col, "\x19", 1);
*/
	}
}

static void itDisplayIns132(uint16_t *buf, int n, int plInstMode)
{
	char col;
	writestring(buf, 0, 0, "", 132);

	if (plBigInstNum[n]!=0xFF)
	{
		const struct it_instrument *ins=&plInstr[plBigInstNum[n]];
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[plBigInstNum[n]]];
		writestring(buf, 0, col, (!plInstMode&&plInstUsed[plBigInstNum[n]])?"\xfe##: ":" ##: ", 5);
		writenum(buf, 1, col, plBigInstNum[n]+1, 16, 2, 0);
		writestring(buf, 5, col, ins->name, 35);
	}

	if (plBigSampNum[n]!=0xFFFF)
	{
		const struct it_sample *sm=&plModSamples[plBigSampNum[n]];
		const struct it_sampleinfo *si=&plSamples[sm->handle];

		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plSampUsed[plBigSampNum[n]]];
		writestring(buf, 34, col, (!plInstMode&&plSampUsed[plBigSampNum[n]])?"\xfe###: ":" ###: ", 6);
		writenum(buf, 35, col, plBigSampNum[n], 16, 3, 0);
		writestring(buf, 40, col, sm->name, 28);
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
			writenum(buf, 90, col, mcpGetFreq8363(-sm->normnote), 10, 6, 1);
		else
			writenum(buf, 90, col, si->samprate, 10, 6, 1);

		writenum(buf, 98, col, sm->vol, 16, 2, 0);
/*
		if (ins->stdpan!=-1)
			writenum(buf, 102, col, ins->stdpan, 16, 2, 0);
		else
			writestring(buf, 102, col, " -", 2);

		if (ins->volenv!=0xFFFF)
			writestring(buf, 106, col, "v", 1);
		if (ins->panenv!=0xFFFF)
			writestring(buf, 107, col, "p", 1);
		if (ins->vibdepth&&ins->vibrate)
			writestring(buf, 108, col, "~", 1);

		if (ins->fadeout)
			writenum(buf, 110, col, ins->fadeout, 16, 4, 1);
		else
			writestring(buf, 113, col, "-", 1);
 */
	}
}

static void itDisplayIns(uint16_t *buf, int len, int n, int plInstMode)
{
	switch (len)
	{
		case 33:
			itDisplayIns33(buf, n, plInstMode);
			break;
		case 40:
			itDisplayIns40(buf, n, plInstMode);
			break;
		case 52:
			itDisplayIns52(buf, n, plInstMode);
			break;
		case 80:
			itDisplayIns80(buf, n, plInstMode);
			break;
		case 132:
			itDisplayIns132(buf, n, plInstMode);
			break;
	}
}

static void (*Mark)(uint8_t *, uint8_t *);

static void itMark(void)
{
	int i;
	for (i=0; i<instnum; i++)
		if (plInstUsed[i])
			plInstUsed[i]=1;
	for (i=0; i<sampnum; i++)
		if (plSampUsed[i])
			plSampUsed[i]=1;
	Mark(plInstUsed, plSampUsed);
}

static void itpInstClear(void)
{
	memset(plInstUsed, 0, instnum);
	memset(plSampUsed, 0, sampnum);
}

static void Done(void)
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

void __attribute__ ((visibility ("internal"))) itpInstSetup(const struct it_instrument *ins, int nins, const struct it_sample *smp, int nsmp, const struct it_sampleinfo *smpi, /*int unused,*/ int type, void (*MarkyBoy)(uint8_t *, uint8_t *))
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

	itpInstClear();

	Mark=MarkyBoy;

	plInstr=ins;
	plModSamples=smp;
	plSamples=smpi;

	for (i=0; i<instnum; i++)
	{
		const struct it_instrument *ins=&plInstr[i];
		int num=0;

		for (j=0; j<128; j++)
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

		for (j=0; j<128; j++)
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
/*
	plInsDisplay.n40=instnum;
	plInsDisplay.n52=instnum;
	plInsDisplay.n80=biginstlen;
*/
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
	itpInstClear();
	plUseInstruments(&plInsDisplay);
}
