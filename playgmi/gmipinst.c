/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay Instrument display routines
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
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "gmiplay.h"
#include "stuff/poutput.h"
#include "cpiface/cpiface.h"

static uint8_t plInstUsed[256];
static uint8_t plSampUsed[1024];
static uint16_t plInstSampNum[256];

static const struct minstrument *plMInstr;
static const struct sampleinfo *plSamples;

static void gmiDisplayIns40(uint16_t *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"\376##: ":" ##: ", 5);
	writenum(buf, 1, col, plMInstr[n].prognum, 16, 2, 0);
	writestring(buf, 5, col, plMInstr[n].name, 35);
}

static void gmiDisplayIns33(uint16_t *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[n]];
	writestring(buf, 0, col, (!plInstMode&&plInstUsed[n])?"\376##: ":" ##: ", 5);
	writenum(buf, 1, col, plMInstr[n].prognum, 16, 2, 0);
	writestring(buf, 5, col, plMInstr[n].name, 28);
}

static void gmiDisplayIns52(uint16_t *buf, int n, int plInstMode)
{
	int i,j;
	char col;
	const struct minstrument *ins;
	const struct msample *sm;

	for (i=0; n>=plInstSampNum[i+1]; i++);
	j=n-plInstSampNum[i];

	writestring(buf, 0, 0, "", 52);

	ins=&plMInstr[i];
	if (!j)
	{
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[i]];
		writestring(buf, 0, col, (!plInstMode&&plInstUsed[i])?"    \376##: ":"     ##: ", 9);
		writenum(buf, 5, col, ins->prognum, 16, 2, 0);
		writestring(buf, 9, col, ins->name, 16);
	}
	col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plSampUsed[plInstSampNum[i]+j]];
	sm=&ins->samples[j];

	writestring(buf, 26, col, (!plInstMode&&plSampUsed[plInstSampNum[i]+j])?"\376##: ":" ##: ", 5);
	_writenum(buf, 27, col, sm->sampnum, 16, 2);
	writestring(buf, 31, col, sm->name, 16);
}

static void gmiDisplayIns80(uint16_t *buf, int n, int plInstMode)
{
	char col;
	int i,j;
	const struct minstrument *ins;
	const struct msample *sm;

	writestring(buf, 0, 0, "", 80);

	for (i=0; n>=plInstSampNum[i+1]; i++);
	j=n-plInstSampNum[i];

	ins=&plMInstr[i];
	if (!j)
	{
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[i]];
		writestring(buf, 0, col, (!plInstMode&&plInstUsed[i])?"\376##: ":" ##: ", 5);
		writenum(buf, 1, col, ins->prognum, 16, 2, 0);
		writestring(buf, 5, col, ins->name, 16);
	}
	col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plSampUsed[plInstSampNum[i]+j]];
	sm=&ins->samples[j];

	writestring(buf, 22, col, (!plInstMode&&plSampUsed[plInstSampNum[i]+j])?"\376##: ":" ##: ", 5);
	_writenum(buf, 23, col, sm->sampnum, 16, 2);
	writestring(buf, 27, col, sm->name, 16);

	if (sm->handle!=-1)
	{
		const struct sampleinfo *si=&plSamples[sm->handle];

		if (si->type&mcpSampLoop)
		{
			_writenum(buf, 44, col, si->loopend, 10, 6);
			_writenum(buf, 51, col, si->loopend-si->loopstart, 10, 6);
			if (si->type&mcpSampBiDi)
				writestring(buf, 57, col, "\x1D", 1);
		} else {
			_writenum(buf, 44, col, si->length, 10, 6);
			writestring(buf, 56, col, "-", 1);
		}
		writestring(buf, 59, col, (si->type&mcpSamp16Bit)?"16":" 8", 2);
		writestring(buf, 61, col, (si->type&mcpSampRedRate4)?"\376":(si->type&mcpSampRedRate2)?"\376":(si->type&mcpSampRedBits)?"!":" ", 2);
		_writenum(buf, 63, col, si->samprate, 10, 6);
		writestring(buf, 69, col, "Hz", 2);
		writestring(buf, 73, col, plNoteStr[(sm->normnote+12*256)>>8], 3);
		writenum(buf, 77, col, sm->normnote&0xFF, 16, 2, 0);
	}
}

static void gmiDisplayIns132(uint16_t *buf, int n, int plInstMode)
{
	char col;
	int i,j;
	const struct minstrument *ins;
	const struct msample *sm;

	writestring(buf, 0, 0, "", 132);

	for (i=0; n>=plInstSampNum[i+1]; i++);
	j=n-plInstSampNum[i];

	ins=&plMInstr[i];
	if (!j)
	{
		col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plInstUsed[i]];
		writestring(buf, 0, col, (!plInstMode&&plInstUsed[i])?"\376##: ":" ##: ", 5);
		writenum(buf, 1, col, ins->prognum, 16, 2, 0);
		writestring(buf, 5, col, ins->name, 16);
	}
	col=plInstMode?0x07:"\x08\x08\x0B\x0A"[plSampUsed[plInstSampNum[i]+j]];
	sm=&ins->samples[j];

	writestring(buf, 22, col, (!plInstMode&&plSampUsed[plInstSampNum[i]+j])?"\376##: ":" ##: ", 5);
	_writenum(buf, 23, col, sm->sampnum, 16, 2);
	writestring(buf, 27, col, sm->name, 16);

	if (sm->handle!=-1)
	{
		const struct sampleinfo *si=&plSamples[sm->handle];

		if (si->type&mcpSampLoop)
		{
			_writenum(buf, 44, col, si->loopend, 10, 6);
			_writenum(buf, 51, col, si->loopend-si->loopstart, 10, 6);
			if (si->type&mcpSampBiDi)
				writestring(buf, 57, col, "\x1D", 1);
		} else {
			_writenum(buf, 44, col, si->length, 10, 6);
			writestring(buf, 56, col, "-", 1);
		}
		writestring(buf, 59, col, (si->type&mcpSamp16Bit)?"16":" 8", 2);
		writestring(buf, 61, col, (si->type&mcpSampRedRate4)?"\376":(si->type&mcpSampRedRate2)?"\376":(si->type&mcpSampRedBits)?"!":" ", 2);
		_writenum(buf, 63, col, si->samprate, 10, 6);
		writestring(buf, 69, col, "Hz", 2);
		writestring(buf, 73, col, plNoteStr[(sm->normnote+12*256)>>8], 3);
		writenum(buf, 77, col, sm->normnote&0xFF, 16, 2, 0);
	}
}

static void gmiDisplayIns(uint16_t *buf, int len, int n, int plInstMode)
{
	switch (len)
	{
		case 33:
			gmiDisplayIns33(buf, n, plInstMode);
			break;
		case 40:
			gmiDisplayIns40(buf, n, plInstMode);
			break;
		case 52:
			gmiDisplayIns52(buf, n, plInstMode);
			break;
		case 80:
			gmiDisplayIns80(buf, n, plInstMode);
			break;
		case 132:
			gmiDisplayIns132(buf, n, plInstMode);
			break;
	}
}

static void gmiMarkIns(void)
{
	int i,j;
	for (i=0; i<256; i++)
		if (plInstUsed[i])
			plInstUsed[i]=1;
	for (i=0; i<1024; i++)
		if (plSampUsed[i])
			plSampUsed[i]=1;

	for (i=0; i<16; i++)
	{
		struct mchaninfo ci;
		int mute;
		midGetChanInfo(i, &ci);
		mute=midGetMute(i);

		if (!mute&&ci.notenum)
		{
			plInstUsed[ci.ins]=((plSelCh==i)||(plInstUsed[ci.ins]==3))?3:2;
			for (j=0; j<ci.notenum; j++)
/*
				if (ci.opt[j]&1) */
					plSampUsed[plInstSampNum[ci.ins]+plMInstr[ci.ins].note[ci.note[j]]]=((plSelCh==i)||(plSampUsed[plInstSampNum[ci.ins]+plMInstr[ci.ins].note[ci.note[j]]]==3))?3:2;
		}
	}
}

void __attribute__ ((visibility ("internal")))gmiClearInst(void)
{
	memset(plInstUsed, 0, 256);
	memset(plSampUsed, 0, 1024);
}

void __attribute__ ((visibility ("internal"))) gmiInsSetup(const struct midifile *mid)
{
	struct insdisplaystruct plInsDisplay;
	int i;
	int plInstNum=mid->instnum;
	int plSampNum=0;

	plMInstr=mid->instruments;
	plSamples=mid->samples;
	for (i=0; i<plInstNum; i++)
	{
		plInstSampNum[i]=plSampNum;
		plSampNum+=plMInstr[i].sampnum;
	}
	plInstSampNum[i]=plSampNum;

	plInsDisplay.Clear=gmiClearInst;
	plInsDisplay.height=plInstNum;
	plInsDisplay.bigheight=plSampNum;
/*
	plInsDisplay.n40=plInstNum;
	plInsDisplay.n52=plSampNum;
	plInsDisplay.n80=plSampNum;
*/
	plInsDisplay.title80=plInsDisplay.title132=" ##   instrument name                       length replen bit  samprate  basenote    ";
	plInsDisplay.Mark=gmiMarkIns;
	plInsDisplay.Display=gmiDisplayIns;
	plInsDisplay.Done=0;
	gmiClearInst();
	plUseInstruments(&plInsDisplay);
}
