/* OpenCP Module Player
 * copyright (c) 2019-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVLPlay instrument display routines
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "hvlpinst.h"
#include "hvlplay.h"
#include "cpiface/cpiface.h"
#include "player.h"
#include "stuff/poutput.h"

static void hvlDisplayIns40 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf, 1, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":ht->ht_Instruments[n].ins_Name, 35);
}

static void hvlDisplayIns33 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];

	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf, 1, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 5, col, compoMode?"#":ht->ht_Instruments[n].ins_Name, 28);
}

static void hvlDisplayIns52 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	cpifaceSession->console->WriteString (buf, 0, col, (!plInstMode&&plInstUsed[n])?"    \xfe##: ":"     ##: ", 9);
	cpifaceSession->console->WriteNum    (buf, 5, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf, 9, col, compoMode?"#":ht->ht_Instruments[n].ins_Name, 43);
}

static void hvlDisplayIns80 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col;

	cpifaceSession->console->WriteString (buf,  0, 0, "", 80);

	col = plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	cpifaceSession->console->WriteString (buf,  0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf,  1, col, n+1,                                        16, 2, 0);
	cpifaceSession->console->WriteString (buf,  5, col, compoMode?"#":ht->ht_Instruments[n].ins_Name,     50);
	cpifaceSession->console->WriteNum    (buf, 56, col, ht->ht_Instruments[n].ins_Volume,           10, 3, 0);
	cpifaceSession->console->WriteNum    (buf, 63, col, ht->ht_Instruments[n].ins_WaveLength,       10, 3, 0);
	cpifaceSession->console->WriteNum    (buf, 73, col, ht->ht_Instruments[n].ins_PList.pls_Speed,  10, 3, 0);
	cpifaceSession->console->WriteString (buf, 76, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf, 77, col, ht->ht_Instruments[n].ins_PList.pls_Length, 10, 3, 0);
}

static void hvlDisplayIns132 (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n, int plInstMode, int compoMode)
{
	char col;

	cpifaceSession->console->WriteString (buf,  0, 0, "", 132);

	col = plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];

	cpifaceSession->console->WriteString (buf,   0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	cpifaceSession->console->WriteNum    (buf,   1, col, n+1, 16, 2, 0);
	cpifaceSession->console->WriteString (buf,   5, col, compoMode?"#":ht->ht_Instruments[n].ins_Name,     58);

	cpifaceSession->console->WriteNum    (buf,  64, col, ht->ht_Instruments[n].ins_Volume,           10, 3, 0);
	cpifaceSession->console->WriteNum    (buf,  71, col, ht->ht_Instruments[n].ins_WaveLength,       10, 3, 0);

	cpifaceSession->console->WriteNum    (buf,  76, col, ht->ht_Instruments[n].ins_FilterLowerLimit, 10, 3, 0);
	cpifaceSession->console->WriteString (buf,  78, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf,  80, col, ht->ht_Instruments[n].ins_FilterUpperLimit, 10, 3, 0);
	cpifaceSession->console->WriteString (buf,  83, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf,  84, col, ht->ht_Instruments[n].ins_FilterSpeed,      10, 3, 0);

	cpifaceSession->console->WriteNum    (buf,  89, col, ht->ht_Instruments[n].ins_SquareLowerLimit, 10, 3, 0);
	cpifaceSession->console->WriteString (buf,  92, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf,  93, col, ht->ht_Instruments[n].ins_SquareUpperLimit, 10, 3, 0);
	cpifaceSession->console->WriteString (buf,  96, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf,  97, col, ht->ht_Instruments[n].ins_SquareSpeed,      10, 3, 0);

	cpifaceSession->console->WriteNum    (buf, 102, col, ht->ht_Instruments[n].ins_VibratoDelay,     10, 3, 0);
	cpifaceSession->console->WriteString (buf, 105, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf, 106, col, ht->ht_Instruments[n].ins_VibratoSpeed,     10, 3, 0);
	cpifaceSession->console->WriteString (buf, 109, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf, 110, col, ht->ht_Instruments[n].ins_VibratoDepth,     10, 3, 0);

	cpifaceSession->console->WriteNum    (buf, 120, col, ht->ht_Instruments[n].ins_PList.pls_Speed,  10, 3, 0);
	cpifaceSession->console->WriteString (buf, 123, 0x07, "/", 1);
	cpifaceSession->console->WriteNum    (buf, 124, col, ht->ht_Instruments[n].ins_PList.pls_Length, 10, 3, 0);
}

static void hvlDisplayIns (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, enum cpiInstWidth width, int n, int plInstMode, int compoMode)
{
	switch (width)
	{
		case cpiInstWidth_33:
			hvlDisplayIns33 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_40:
			hvlDisplayIns40 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_52:
			hvlDisplayIns52 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_80:
			hvlDisplayIns80 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
		case cpiInstWidth_132:
			hvlDisplayIns132 (cpifaceSession, buf, n, plInstMode, compoMode);
			break;
	}
}

static void hvlMark (struct cpifaceSessionAPI_t *cpifaceSession)
{
	/* moved into hvl_statbuffer_callback_from_hvlbuf */
}

static void hvlInstClear (struct cpifaceSessionAPI_t *cpifaceSession)
{
}

static void hvlDone (struct cpifaceSessionAPI_t *cpifaceSession)
{
}

OCP_INTERNAL void hvlInstSetup (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct insdisplaystruct plInsDisplay;

	plInsDisplay.Clear=hvlInstClear;
	plInsDisplay.title80 = " ##   instrument name / song message                  volume length  pls-spd/len";
	plInsDisplay.title132 = " ##   instrument name / song message                          volume length   filter       square       vibrato   pls-speed/length  ";
	plInsDisplay.Mark=hvlMark;
	plInsDisplay.Display=hvlDisplayIns;
	plInsDisplay.Done=hvlDone;
	hvlInstClear (cpifaceSession);
	plInsDisplay.height=ht->ht_InstrumentNr;
	plInsDisplay.bigheight=ht->ht_InstrumentNr;
	cpifaceSession->UseInstruments (cpifaceSession, &plInsDisplay);
}
