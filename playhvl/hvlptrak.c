/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVLPlay track/pattern display routines
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
#include "types.h"
#include "cpiface/cpiface.h"
#include "stuff/poutput.h"
#include "hvlplay.h"
#include "hvlptrak.h"
#include "player.h"

#define COLPTNOTE 0x0A
#define COLNOTE 0x0F
#define COLPITCH 0x02
#define COLSPEED 0x02
#define COLPAN 0x05
#define COLVOL 0x09
#define COLACT 0x04
#define COLINS 0x07

static uint16_t curRow;
static uint16_t curPosition; // order
static int16_t curChannel;   // -1 = Global

static int startrow(void)
{
	curRow++;
	if (curRow >= ht->ht_TrackLength)
	{
		return -1;
	}
	return curRow;
}

static void seektrack(int n, int c)
{
	curRow = -1;
	curPosition = n;
	curChannel = c;
}

static const char *getpatname(int n)
{
	return 0; /* patterns do not have labels - we could reference the source pattern here, since HVL/AHX reuses patterns across orders */
}

static int getpatlen(int n)
{
	return ht->ht_TrackLength;
}

static int getcurpos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int     row,     rows;
	int   order,   orders;
	int subsong, subsongs;
	int tempo;
	int speedmult;

	hvlGetStats (&row, &rows, &order, &orders, &subsong, &subsongs, &tempo, &speedmult);

	return (order<<8) | row;
}

static int getnote(uint16_t *bp, int small)
{
	struct hvl_step *Step;
	int16_t Note;
	uint8_t color;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;
	Note = Step->stp_Note;

	if (!Note)
	{
		return 0;
	}
	Note += ht->ht_Positions [ curPosition ] .pos_Transpose [curChannel];
	Note += 24 - 1;
	if (Note < 0)
	{
		Note = 0;
	} else if (Note >= 10*12)
	{
		Note = 10*12-1;
	}

	if ((Step->stp_FX == 0x03) || (Step->stp_FXb == 0x3))
	{
		color = COLPTNOTE;
	} else {
		color = COLNOTE;
	}

	switch (small)
	{
		case 0:
			writestring(bp, 0, color, &"CCDDEFFGGAAB"[(Note & 0x7f)%12], 1);
			writestring(bp, 1, color, &"-#-#--#-#-#-"[(Note & 0x7f)%12], 1);
			writestring(bp, 2, color, &"-0123456789" [(Note & 0x7f)/12], 1);
			break;
		case 1:
			writestring(bp, 0, color, &"cCdDefFgGaAb"[(Note & 0x7f)%12], 1);
			writestring(bp, 1, color, &"-0123456789" [(Note & 0x7f)/12], 1);
			break;
		case 2:
			writestring(bp, 0, color, &"cCdDefFgGaAb"[(Note & 0x7f)%12], 1);
			break;
	}
	return 1;
}

static int getvol(uint16_t *bp)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;

	if ((Step->stp_FX == 0x0c) && (Step->stp_FXParam < 0x40))
	{
		writenum(bp, 0, COLVOL, Step->stp_FXParam, 16, 2, 0);
		return 1;
	}
	if ((Step->stp_FXb == 0x0c) && (Step->stp_FXbParam < 0x40))
	{
		writenum(bp, 0, COLVOL, Step->stp_FXbParam, 16, 2, 0);
		return 1;
	}

	return 0;
}

static int getins(uint16_t *bp)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;

	if (Step->stp_Instrument)
	{
		writenum(bp, 0, COLINS, Step->stp_Instrument, 16, 2, 0);
		return 1;
	}
	return 0;
}

static int getpan(uint16_t *bp)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;

	if (Step->stp_FX == 0x07)
	{
		writenum(bp, 0, COLPAN, Step->stp_FXParam, 16, 2, 0);
		return 1;
	}
	if (Step->stp_FXb == 0x07)
	{
		writenum(bp, 0, COLPAN, Step->stp_FXbParam, 16, 2, 0);
		return 1;
	}
	return 0;
}

static void _getgcmd(uint16_t *buf, int *n, uint8_t fx, uint8_t param)
{
	if (fx == 0x0) /* Position Jump Hi */
	{
		if (param != 0) /* avoid flooding */
		{
			writestring(buf, 0, COLACT, "H", 1);
			writenum(buf, 1, COLACT, param, 16, 2, 0);
			*n = *n - 1;
		}
	} else if (fx == 0xb) /* Position Jump Lo */
	{
		writestring(buf, 0, COLACT, "\x1A", 1);
		writenum(buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0xC) /* Volume */
	{
		if ((param >= 0xA0) && (param < 0xE0))
		{
			writestring(buf, 0, COLVOL, "v", 1);
			writenum(buf, 1, COLVOL, param - 0xA0, 16, 2, 0);
			*n = *n - 1;
		}
	} else if (fx == 0xd) /* Break */
	{
		writestring(buf, 0, COLACT, "\x19", 1);
		writenum(buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0xf) /* Tempo */
	{
		writestring(buf, 0, COLSPEED, "t", 1);
		writenum(buf, 1, COLSPEED, param, 16, 2, 0);
		*n = *n - 1;
	}
}


static void getgcmd(uint16_t *buf, int n)
{
	int i;
	struct hvl_step *Step;

	for (i=0; i < MAX_CHANNELS; i++)
	{
		Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ i ]  ] + curRow;
		_getgcmd (buf, &n, Step->stp_FX, Step->stp_FXParam);
		if (!n) return;
		_getgcmd (buf, &n, Step->stp_FXb, Step->stp_FXbParam);
		if (!n) return;
	}
}

static void _getfx(uint16_t *buf, int *n, uint8_t fx, uint8_t param)
{
	if (fx == 0x1) /* Porta Up */
	{
		writestring(buf, 0, COLPITCH, "\x18", 1);
		writenum(buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x2) /* Porta Down */
	{
		writestring(buf, 0, COLPITCH, "\x19", 1);
		writenum(buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x2) /* Porta Down */
	{
		writestring(buf, 0, COLPITCH, "\x19", 1);
		writenum(buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x3) /* Porta to note */
	{
		writestring(buf, 0, COLPITCH, "\x0D", 1);
		writenum(buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x4) /* Filter */
	{
		writestring(buf, 0, COLACT, "F", 1);
		writenum(buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x5) /* PortaTo+VolumeSlide */
	{
		writestring(buf, 0, COLACT, "\x0D", 1);
		if ((param & 0xF0)!=0x00)
		{
			writestring(buf, 1, COLVOL, "\x18", 1);
			writenum(buf, 2, COLVOL, param >> 4, 16, 1, 0);
		} else if ((param & 0xF0)!=0x00)
		{
			writestring(buf, 1, COLVOL, "\x19", 1);
			writenum(buf, 2, COLVOL, param & 0xF, 16, 1, 0);
		} else {
			writenum(buf, 1, COLVOL, param, 16, 2, 0);
		}
		*n = *n - 1;
#if 0
	// done by getpan()
	} else if (fx == 0x7) /* Pan */
	{
		writestring(buf, 0, COLPAN, ((int8_t)param>0)?"\x1A":((int8_t)param<0)?"\x1B":"\x1D", 1);
		writenum(buf, 1, COLPAN, param, 16, 2, 0);
		*n = *n - 1;
#endif
	} else if (fx == 0x9) /* Square-Relation */
	{
		writestring(buf, 0, COLACT, "S", 1);
		writenum(buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0xA) /* VolumeSlide */
	{
		if ((param & 0xF0)!=0x00)
		{
			writestring(buf, 0, COLVOL, "\x18", 1);
			writenum(buf, 1, COLVOL, param >> 4, 16, 2, 0);
		} else if ((param &0xF0)!=0x00)
		{
			writestring(buf, 1, COLVOL, "\x19", 1);
			writenum(buf, 1, COLVOL, param & 0xF, 16, 2, 0);
		} else {
			writestring(buf, 1, COLVOL, "v", 1);
			writenum(buf, 1, COLVOL, param, 16, 2, 0);
		}
		*n = *n - 1;
	} else if (fx == 0xC) /* Volume */
	{
		if ((param >= 0x50) && (param < 0x90))
		{
			writestring(buf, 0, COLVOL, "v", 1);
			writenum(buf, 1, COLVOL, param - 0x50, 16, 2, 0);
			*n = *n - 1;
		}
	} else if (fx == 0xE) // Multiple commands
	{
		if ((param & 0xF0) == 0x10) /* FinePortUp */
		{
			writestring(buf, 0, COLPITCH, "+", 1);
			writenum(buf, 1, COLPITCH, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0x20) /* FinePortDown */
		{
			writestring(buf, 0, COLPITCH, "-", 1);
			writenum(buf, 1, COLPITCH, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0x40) /* Vibrato Control */
		{
			writestring(buf, 0, COLPITCH, "~=", 2);
			writenum(buf, 2, COLPITCH, param & 0xf, 16, 1, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xA0) /* FineVolumeUp */
		{
			writestring(buf, 0, COLVOL, "+", 1);
			writenum(buf, 1, COLVOL, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xB0) /* FineVolumeDown */
		{
			writestring(buf, 0, COLVOL, "-", 1);
			writenum(buf, 1, COLVOL, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xC0) /* NoteCut */
		{
			writestring(buf, 0, COLACT, "^", 1);
			writenum(buf, 1, COLACT, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xD0) /* NoteDelay */
		{
			writestring(buf, 0, COLACT, "d", 1);
			writenum(buf, 1, COLACT, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xF0) /* Preserve Transpose */
		{
			writestring(buf, 0, COLACT, "pre", 3);
			*n = *n - 1;
		}
	}
}

static void getfx(uint16_t *buf, int n)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;
	_getfx (buf, &n, Step->stp_FX, Step->stp_FXParam);
	if (!n) return;
	_getfx (buf, &n, Step->stp_FXb, Step->stp_FXbParam);
}

static struct cpitrakdisplaystruct hvlptrkdisplay=
{
	getcurpos, getpatlen, getpatname, seektrack, startrow, getnote,
	getins, getvol, getpan, getfx, getgcmd
};

void __attribute__ ((visibility ("internal"))) hvlTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->TrackSetup(cpifaceSession, &hvlptrkdisplay, ht->ht_PositionNr);
}
