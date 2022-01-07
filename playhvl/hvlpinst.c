/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
//#include "dev/mcp.h"
#include "hvlpinst.h"
#include "hvlplay.h"
#include "cpiface/cpiface.h"
#include "player.h"
#include "stuff/poutput.h"

static void hvlDisplayIns40(unsigned short *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	writestring (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum    (buf, 1, col, n+1, 16, 2, 0);
	writestring (buf, 5, col, ht->ht_Instruments[n].ins_Name, 35);
}

static void hvlDisplayIns33(unsigned short *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];

	writestring (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum    (buf, 1, col, n+1, 16, 2, 0);
	writestring (buf, 5, col, ht->ht_Instruments[n].ins_Name, 28);
}

static void hvlDisplayIns52(unsigned short *buf, int n, int plInstMode)
{
	char col=plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	writestring (buf, 0, col, (!plInstMode&&plInstUsed[n])?"    \xfe##: ":"     ##: ", 9);
	writenum    (buf, 5, col, n+1, 16, 2, 0);
	writestring (buf, 9, col, ht->ht_Instruments[n].ins_Name, 43);
}

static void hvlDisplayIns80(unsigned short *buf, int n, int plInstMode)
{
	char col;

	writestring(buf, 0, 0, "", 80);

	col = plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];
	writestring (buf, 0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum    (buf,  1, col, n+1,                                        16, 2, 0);
	writestring (buf,  5, col, ht->ht_Instruments[n].ins_Name, 50);
	writenum    (buf, 56, col, ht->ht_Instruments[n].ins_Volume,           10, 3, 0);
	writenum    (buf, 63, col, ht->ht_Instruments[n].ins_WaveLength,       10, 3, 0);
	writenum    (buf, 73, col, ht->ht_Instruments[n].ins_PList.pls_Speed,  10, 3, 0);
	writestring (buf, 76, 0x07, "/", 1);
	writenum    (buf, 77, col, ht->ht_Instruments[n].ins_PList.pls_Length, 10, 3, 0);
}

static void hvlDisplayIns132(unsigned short *buf, int n, int plInstMode)
{
	char col;

	writestring(buf, 0, 0, "", 132);

	col = plInstMode?0x07:"\x08\x08\x0B\x0A"[(unsigned)plInstUsed[n]];

	writestring (buf,   0, col, (!plInstMode&&plInstUsed[n])?"\xfe##: ":" ##: ", 5);
	writenum    (buf,   1, col, n+1, 16, 2, 0);
	writestring (buf,   5, col, ht->ht_Instruments[n].ins_Name,             58);

	writenum    (buf,  64, col, ht->ht_Instruments[n].ins_Volume,           10, 3, 0);
	writenum    (buf,  71, col, ht->ht_Instruments[n].ins_WaveLength,       10, 3, 0);

	writenum    (buf,  76, col, ht->ht_Instruments[n].ins_FilterLowerLimit, 10, 3, 0);
	writestring (buf,  78, 0x07, "/", 1);
	writenum    (buf,  80, col, ht->ht_Instruments[n].ins_FilterUpperLimit, 10, 3, 0);
	writestring (buf,  83, 0x07, "/", 1);
	writenum    (buf,  84, col, ht->ht_Instruments[n].ins_FilterSpeed,      10, 3, 0);

	writenum    (buf,  89, col, ht->ht_Instruments[n].ins_SquareLowerLimit, 10, 3, 0);
	writestring (buf,  92, 0x07, "/", 1);
	writenum    (buf,  93, col, ht->ht_Instruments[n].ins_SquareUpperLimit, 10, 3, 0);
	writestring (buf,  96, 0x07, "/", 1);
	writenum    (buf,  97, col, ht->ht_Instruments[n].ins_SquareSpeed,      10, 3, 0);

	writenum    (buf, 102, col, ht->ht_Instruments[n].ins_VibratoDelay,     10, 3, 0);
	writestring (buf, 105, 0x07, "/", 1);
	writenum    (buf, 106, col, ht->ht_Instruments[n].ins_VibratoSpeed,     10, 3, 0);
	writestring (buf, 109, 0x07, "/", 1);
	writenum    (buf, 110, col, ht->ht_Instruments[n].ins_VibratoDepth,     10, 3, 0);

	writenum    (buf, 120, col, ht->ht_Instruments[n].ins_PList.pls_Speed,  10, 3, 0);
	writestring (buf, 123, 0x07, "/", 1);
	writenum    (buf, 124, col, ht->ht_Instruments[n].ins_PList.pls_Length, 10, 3, 0);
}

static void hvlDisplayIns(unsigned short *buf, int len, int n, int plInstMode)
{
	switch (len)
	{
		case 33:
			hvlDisplayIns33(buf, n, plInstMode);
			break;
		case 40:
			hvlDisplayIns40(buf, n, plInstMode);
			break;
		case 52:
			hvlDisplayIns52(buf, n, plInstMode);
			break;
		case 80:
			hvlDisplayIns80(buf, n, plInstMode);
			break;
		case 132:
			hvlDisplayIns132(buf, n, plInstMode);
			break;
	}
}

static void hvlMark(void)
{
	/* moved into hvl_statbuffer_callback_from_hvlbuf */
}

static void hvlInstClear(void)
{
}

static void hvlDone(void)
{
}

void __attribute__ ((visibility ("internal"))) hvlInstSetup (void)
{
	struct insdisplaystruct plInsDisplay;

	plInsDisplay.Clear=hvlInstClear;
/*
	plInsDisplay.n40=ht->ht_InstrumentNr;
	plInsDisplay.n52=ht->ht_InstrumentNr;
	plInsDisplay.n80=ht->ht_InstrumentNr;         TODO TODO TODO TODO TODO TODO
*/
	plInsDisplay.title80 = " ##   instrument name / song message                  volume length  pls-spd/len";

	plInsDisplay.title132 = " ##   instrument name / song message                          volume length   filter       square       vibrato   pls-speed/length  ";

	plInsDisplay.Mark=hvlMark;
	plInsDisplay.Display=hvlDisplayIns;
	plInsDisplay.Done=hvlDone;
	hvlInstClear();
	plInsDisplay.height=ht->ht_InstrumentNr;
	plInsDisplay.bigheight=ht->ht_InstrumentNr;
	plUseInstruments(&plInsDisplay);
}
