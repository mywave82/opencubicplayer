/* OpenCP Module Player
 * copyright (c) 2019-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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

static int hvl_startrow (struct cpifaceSessionAPI_t *cpifaceSession)
{
	curRow++;
	if (curRow >= ht->ht_TrackLength)
	{
		return -1;
	}
	return curRow;
}

static void hvl_seektrack (struct cpifaceSessionAPI_t *cpifaceSession, int n, int c)
{
	curRow = -1;
	curPosition = n;
	curChannel = c;
}

static const char *hvl_getpatname (struct cpifaceSessionAPI_t *cpifaceSession, int n)
{
	return 0; /* patterns do not have labels - we could reference the source pattern here, since HVL/AHX reuses patterns across orders */
}

static int hvl_getpatlen (struct cpifaceSessionAPI_t *cpifaceSession, int n)
{
	return ht->ht_TrackLength;
}

static int hvl_getcurpos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int     row,     rows;
	int   order,   orders;
	int subsong, subsongs;
	int tempo;
	int speedmult;

	hvlGetStats (&row, &rows, &order, &orders, &subsong, &subsongs, &tempo, &speedmult);

	return (order<<8) | row;
}

static int hvl_getnote (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp, int small)
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
			cpifaceSession->console->WriteString (bp, 0, color, &"CCDDEFFGGAAB"[(Note & 0x7f)%12], 1);
			cpifaceSession->console->WriteString (bp, 1, color, &"-#-#--#-#-#-"[(Note & 0x7f)%12], 1);
			cpifaceSession->console->WriteString (bp, 2, color, &"-0123456789" [(Note & 0x7f)/12], 1);
			break;
		case 1:
			cpifaceSession->console->WriteString (bp, 0, color, &"cCdDefFgGaAb"[(Note & 0x7f)%12], 1);
			cpifaceSession->console->WriteString (bp, 1, color, &"-0123456789" [(Note & 0x7f)/12], 1);
			break;
		case 2:
			cpifaceSession->console->WriteString (bp, 0, color, &"cCdDefFgGaAb"[(Note & 0x7f)%12], 1);
			break;
	}
	return 1;
}

static int hvl_getvol (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;

	if ((Step->stp_FX == 0x0c) && (Step->stp_FXParam < 0x40))
	{
		cpifaceSession->console->WriteNum (bp, 0, COLVOL, Step->stp_FXParam, 16, 2, 0);
		return 1;
	}
	if ((Step->stp_FXb == 0x0c) && (Step->stp_FXbParam < 0x40))
	{
		cpifaceSession->console->WriteNum (bp, 0, COLVOL, Step->stp_FXbParam, 16, 2, 0);
		return 1;
	}

	return 0;
}

static int hvl_getins (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;

	if (Step->stp_Instrument)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLINS, Step->stp_Instrument, 16, 2, 0);
		return 1;
	}
	return 0;
}

static int hvl_getpan (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *bp)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;

	if (Step->stp_FX == 0x07)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, Step->stp_FXParam, 16, 2, 0);
		return 1;
	}
	if (Step->stp_FXb == 0x07)
	{
		cpifaceSession->console->WriteNum (bp, 0, COLPAN, Step->stp_FXbParam, 16, 2, 0);
		return 1;
	}
	return 0;
}

static void _hvl_getgcmd (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int *n, uint8_t fx, uint8_t param)
{
	if (fx == 0x0) /* Position Jump Hi */
	{
		if (param != 0) /* avoid flooding */
		{
			cpifaceSession->console->WriteString (buf, 0, COLACT, "H", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLACT, param, 16, 2, 0);
			*n = *n - 1;
		}
	} else if (fx == 0xb) /* Position Jump Lo */
	{
		cpifaceSession->console->WriteString (buf, 0, COLACT, "\x1A", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0xC) /* Volume */
	{
		if ((param >= 0xA0) && (param < 0xE0))
		{
			cpifaceSession->console->WriteString (buf, 0, COLVOL, "v", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param - 0xA0, 16, 2, 0);
			*n = *n - 1;
		}
	} else if (fx == 0xd) /* Break */
	{
		cpifaceSession->console->WriteString (buf, 0, COLACT, "\x19", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0xf) /* Tempo */
	{
		cpifaceSession->console->WriteString (buf, 0, COLSPEED, "t", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLSPEED, param, 16, 2, 0);
		*n = *n - 1;
	}
}


static void hvl_getgcmd (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n)
{
	int i;
	struct hvl_step *Step;

	for (i=0; i < ht->ht_Channels; i++)
	{
		Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ i ]  ] + curRow;
		_hvl_getgcmd (cpifaceSession, buf, &n, Step->stp_FX, Step->stp_FXParam);
		if (!n) return;
		_hvl_getgcmd (cpifaceSession, buf, &n, Step->stp_FXb, Step->stp_FXbParam);
		if (!n) return;
	}
}

static void _hvl_getfx (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int *n, uint8_t fx, uint8_t param)
{
	if (fx == 0x1) /* Porta Up */
	{
		cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x18", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x2) /* Porta Down */
	{
		cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x19", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x2) /* Porta Down */
	{
		cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x19", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x3) /* Porta to note */
	{
		cpifaceSession->console->WriteString (buf, 0, COLPITCH, "\x0D", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x4) /* Filter */
	{
		cpifaceSession->console->WriteString (buf, 0, COLACT, "F", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0x5) /* PortaTo+VolumeSlide */
	{
		cpifaceSession->console->WriteString (buf, 0, COLACT, "\x0D", 1);
		if ((param & 0xF0)!=0x00)
		{
			cpifaceSession->console->WriteString (buf, 1, COLVOL, "\x18", 1);
			cpifaceSession->console->WriteNum    (buf, 2, COLVOL, param >> 4, 16, 1, 0);
		} else if ((param & 0xF0)!=0x00)
		{
			cpifaceSession->console->WriteString (buf, 1, COLVOL, "\x19", 1);
			cpifaceSession->console->WriteNum    (buf, 2, COLVOL, param & 0xF, 16, 1, 0);
		} else {
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param, 16, 2, 0);
		}
		*n = *n - 1;
#if 0
	// done by getpan()
	} else if (fx == 0x7) /* Pan */
	{
		cpifaceSession->console->WriteString (buf, 0, COLPAN, ((int8_t)param>0)?"\x1A":((int8_t)param<0)?"\x1B":"\x1D", 1);
		cpifaceSession->console->WriteNum (buf, 1, COLPAN, param, 16, 2, 0);
		*n = *n - 1;
#endif
	} else if (fx == 0x9) /* Square-Relation */
	{
		cpifaceSession->console->WriteString (buf, 0, COLACT, "S", 1);
		cpifaceSession->console->WriteNum    (buf, 1, COLACT, param, 16, 2, 0);
		*n = *n - 1;
	} else if (fx == 0xA) /* VolumeSlide */
	{
		if ((param & 0xF0)!=0x00)
		{
			cpifaceSession->console->WriteString (buf, 0, COLVOL, "\x18", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param >> 4, 16, 2, 0);
		} else if ((param &0xF0)!=0x00)
		{
			cpifaceSession->console->WriteString (buf, 1, COLVOL, "\x19", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param & 0xF, 16, 2, 0);
		} else {
			cpifaceSession->console->WriteString (buf, 1, COLVOL, "v", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param, 16, 2, 0);
		}
		*n = *n - 1;
	} else if (fx == 0xC) /* Volume */
	{
		if ((param >= 0x50) && (param < 0x90))
		{
			cpifaceSession->console->WriteString (buf, 0, COLVOL, "v", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param - 0x50, 16, 2, 0);
			*n = *n - 1;
		}
	} else if (fx == 0xE) // Multiple commands
	{
		if ((param & 0xF0) == 0x10) /* FinePortUp */
		{
			cpifaceSession->console->WriteString (buf, 0, COLPITCH, "+", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0x20) /* FinePortDown */
		{
			cpifaceSession->console->WriteString (buf, 0, COLPITCH, "-", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLPITCH, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0x40) /* Vibrato Control */
		{
			cpifaceSession->console->WriteString (buf, 0, COLPITCH, "~=", 2);
			cpifaceSession->console->WriteNum    (buf, 2, COLPITCH, param & 0xf, 16, 1, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xA0) /* FineVolumeUp */
		{
			cpifaceSession->console->WriteString (buf, 0, COLVOL, "+", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xB0) /* FineVolumeDown */
		{
			cpifaceSession->console->WriteString (buf, 0, COLVOL, "-", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLVOL, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xC0) /* NoteCut */
		{
			cpifaceSession->console->WriteString (buf, 0, COLACT, "^", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLACT, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xD0) /* NoteDelay */
		{
			cpifaceSession->console->WriteString (buf, 0, COLACT, "d", 1);
			cpifaceSession->console->WriteNum    (buf, 1, COLACT, param & 0xf, 16, 2, 0);
			*n = *n - 1;
		} else if ((param & 0xF0) == 0xF0) /* Preserve Transpose */
		{
			cpifaceSession->console->WriteString (buf, 0, COLACT, "pre", 3);
			*n = *n - 1;
		}
	}
}

static void hvl_getfx (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t *buf, int n)
{
	struct hvl_step *Step;

	Step = ht->ht_Tracks [ ht->ht_Positions [ curPosition ] .pos_Track [ curChannel ]  ] + curRow;
	_hvl_getfx (cpifaceSession, buf, &n, Step->stp_FX, Step->stp_FXParam);
	if (!n) return;
	_hvl_getfx (cpifaceSession, buf, &n, Step->stp_FXb, Step->stp_FXbParam);
}

static struct cpitrakdisplaystruct hvlptrkdisplay=
{
	hvl_getcurpos, hvl_getpatlen, hvl_getpatname, hvl_seektrack, hvl_startrow, hvl_getnote,
	hvl_getins, hvl_getvol, hvl_getpan, hvl_getfx, hvl_getgcmd
};

OCP_INTERNAL void hvlTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->TrackSetup(cpifaceSession, &hvlptrkdisplay, ht->ht_PositionNr);
}
