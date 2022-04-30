/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OPLPlay track/pattern display routines
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
#include "adplug-git/src/adplug.h"
#include "adplug-git/src/fprovide.h"
#include "types.h"
extern "C"
{
#include "cpiface/cpiface.h"
#include "stuff/poutput.h"
}
#include "oplplay.h"
#include "oplptrak.h"
#include "ocpemu.h"

#define COLPTNOTE 0x0A
#define COLNOTE 0x0F
#define COLPITCH 0x02
#define COLSPEED 0x02
#define COLPAN 0x05
#define COLVOL 0x09
#define COLACT 0x04
#define COLINS 0x07
#define COLOPL 0x05

static uint16_t curRow;
static uint16_t curPosition; // order
static int16_t curChannel;   // -1 = Global
static int cacheRows;
static int cacheChannels;

static CPlayer *trkP;

struct pattern_t
{
  CPlayer::TrackedCmds command;
  uint8_t note;
  uint8_t inst;
  uint8_t param;
  uint8_t volume;
};
static struct pattern_t *pattern;
static int patterndim;

static void preparepattern (int rows, int channels)
{
 if (rows * channels > patterndim)
 {
    int i;
    patterndim = rows * channels;
    free (pattern);
    pattern = (struct pattern_t *)malloc (sizeof (pattern[0]) * patterndim);
    if (!pattern)
    {
      patterndim = 0;
      return;
    }
    for (i=0; i < patterndim; i++)
    {
      pattern[i].note = 0;
      pattern[i].inst = 0;
      pattern[i].command = CPlayer::TrackedCmdNone;
      pattern[i].param = 0;
      pattern[i].volume = 255;
    }
  }
}

static int startrow(void)
{
	curRow++;
	if (curRow >= cacheRows)
	{
		return -1;
	}

	return curRow;
}

static void trackdata (void *arg, unsigned char row, unsigned char channel, unsigned char note, CPlayer::TrackedCmds command, unsigned char inst, unsigned char volume, unsigned char param)
{
	pattern[row * cacheChannels + channel].note = note;
	pattern[row * cacheChannels + channel].command = command;
	pattern[row * cacheChannels + channel].inst = inst;
	pattern[row * cacheChannels + channel].volume = volume;
	pattern[row * cacheChannels + channel].param = param;
	return;
}

static void seektrack(int n, int c)
{
	if (curPosition != n)
	{
		cacheRows = trkP->getrows();
		preparepattern (cacheRows, cacheChannels);
		curPosition = n;
		if (pattern)
		{
			trkP->gettrackdata (trkP->getpattern (curPosition), trackdata, 0);
		}
	}
	curRow = -1;
	curChannel = c;
}

static const char *getpatname(int n)
{
	return 0; /* patterns do not have labels */
}

static int getpatlen(int n)
{
	return trkP->getrows();
}

static int getcurpos(void)
{
	return (trkP->getorder() << 8) | trkP->getrow();
}

static int getnote(uint16_t *buf, int small)
{
	uint8_t color;

	uint8_t cacheNote = pattern[curRow * cacheChannels + curChannel].note;
	CPlayer::TrackedCmds cacheCommand = pattern[curRow * cacheChannels + curChannel].command;

	if ((cacheNote == 0x00) || (cacheNote == 127))
	{
		return 0;
	}

#if 0
	Note += 24 - 1;
#endif

	color = ((cacheCommand == CPlayer::TrackedCmdTonePortamento) || (cacheCommand == CPlayer::TrackedCmdTonePortamentoVolumeSlide)) ? COLPTNOTE : COLNOTE;

	switch (small)
	{
		case 0:
			writestring(buf, 0, color, &"CCDDEFFGGAAB"[(cacheNote & 0x7f)%12], 1);
			writestring(buf, 1, color, &"-#-#--#-#-#-"[(cacheNote & 0x7f)%12], 1);
			writestring(buf, 2, color, &"-0123456789" [(cacheNote & 0x7f)/12], 1);
			break;
		case 1:
			writestring(buf, 0, color, &"cCdDefFgGaAb"[(cacheNote & 0x7f)%12], 1);
			writestring(buf, 1, color, &"-0123456789" [(cacheNote & 0x7f)/12], 1);
			break;
		case 2:
			writestring(buf, 0, color, &"cCdDefFgGaAb"[(cacheNote & 0x7f)%12], 1);
			break;
	}
	return 1;
}

static int getvol(uint16_t *buf)
{
	uint8_t cacheVolume = pattern[curRow * cacheChannels + curChannel].volume;

	if (cacheVolume != 0xff)
	{
		writenum(buf, 0, COLVOL, cacheVolume, 16, 2, 0);
		return 1;
	}

	return 0;
}

static int getins(uint16_t *buf)
{
	uint8_t cacheInst = pattern[curRow * cacheChannels + curChannel].inst;

	if (cacheInst)
	{
		writenum(buf, 0, COLINS, cacheInst, 16, 2, 0);
		return 1;
	}
	return 0;
}

static int getpan(uint16_t *buf)
{
	return 0;
}

static void _getgcmd(uint16_t *buf, int *n, uint8_t fx, uint8_t param)
{
	switch (fx)
	{
		default:
			break;

		case CPlayer::TrackedCmdSpeed:
			writestring(buf, 0, COLSPEED, "s", 1);
			writenum(buf, 1, COLSPEED, param, 16, 2, 0);
			*n = *n - 1;
			break;

		case CPlayer::TrackedCmdTempo:
			writestring(buf, 0, COLSPEED, "t", 1);
			writenum(buf, 1, COLSPEED, param, 16, 2, 0);
			*n = *n - 1;
			break;

		case CPlayer::TrackedCmdPatternJumpTo:
			writestring(buf, 0, COLACT, "\x1A", 1);
			writenum(buf, 1, COLACT, param, 16, 2, 0);
			*n = *n - 1;
			break;

		case CPlayer::TrackedCmdPatternBreak:
			writestring(buf, 0, COLACT, "\x19", 1);
			writenum(buf, 1, COLACT, param, 16, 2, 0);
			*n = *n - 1;
			break;

		case CPlayer::TrackedCmdPatternSetLoop:
			writestring(buf, 0, COLACT, (param==1)?"lp1":(param==2)?"lp2":"lp-", 3);
			*n = *n - 1;
			break;

		case CPlayer::TrackedCmdPatternDoLoop:
			writestring(buf, 0, COLACT, "pl", 2);
			writenum(buf, 2, COLACT, param, 16, 1, 0);
			*n = *n - 1;
			break;

		case CPlayer::TrackedCmdPatternDelay:
			writestring(buf, 0, COLACT, "pd", 2);
			writenum(buf, 2, COLACT, param & 15, 16, 1, 0);
			*n = *n - 1;
			break;

		case CPlayer::TrackedCmdGlobalVolume:
			writestring(buf, 0, COLVOL, "v", 1);
			writenum(buf, 2, COLVOL, param, 16, 2, 0);
			*n = *n - 1;
			break;
	}
}


static void getgcmd(uint16_t *buf, int n)
{
	int i;

	for (i=0; i<cacheChannels; i++)
	{
		_getgcmd (buf, &n, pattern[curRow * cacheChannels + i].command,
		                   pattern[curRow * cacheChannels + i].param);
	}
}

static void getfx(uint16_t *buf, int n)
{
	CPlayer::TrackedCmds cacheCommand = pattern[curRow * cacheChannels + curChannel].command;
	uint8_t cacheParam = pattern[curRow * cacheChannels + curChannel].param;

	switch (cacheCommand)
	{
		default:
		case CPlayer::TrackedCmdNone:
		case CPlayer::TrackedCmdSpeed:
		case CPlayer::TrackedCmdTempo:
		case CPlayer::TrackedCmdPatternJumpTo:
		case CPlayer::TrackedCmdPatternBreak:
		case CPlayer::TrackedCmdPatternSetLoop:
		case CPlayer::TrackedCmdPatternDoLoop:
		case CPlayer::TrackedCmdPatternDelay:
		case CPlayer::TrackedCmdGlobalVolume:
			return;

		case CPlayer::TrackedCmdNoteCut:
			writestring(buf, 0, COLINS, "off", 3);
			return;

		case CPlayer::TrackedCmdArpeggio:
			writestring(buf, 0, COLPITCH, "\xf0", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdPitchSlideUp:
			writestring(buf, 0, COLPITCH, "\x18", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdPitchSlideDown:
			writestring(buf, 0, COLPITCH, "\x19", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdPitchSlideUpDown:
			if (!cacheParam)
			{
				writestring(buf, 0, COLVOL, "\x12""00", 3);
			} else {
				if (cacheParam & 0xF0)
				{
					writestring(buf, 0, COLVOL, "\x18", 1);
					writenum(buf, 1, COLVOL, cacheParam>>4, 16, 2, 0);
				} else {
					writestring(buf, 0, COLVOL, "\x19", 1);
					writenum(buf, 1, COLVOL, cacheParam&0xF, 16, 2, 0);
				}
			}
			return;

		case CPlayer::TrackedCmdPitchFineSlideUp:
			writestring(buf, 0, COLPITCH, "+", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdPitchFineSlideDown:
			writestring(buf, 0, COLPITCH, "-", 2);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdTonePortamento:
			writestring(buf, 0, COLPITCH, "\x0D", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdTonePortamentoVolumeSlide:
			writestring(buf, 0, COLACT, "\x0D", 1);
			if ((cacheParam & 0xF0)!=0x00)
			{
				writestring(buf, 1, COLVOL, "\x18", 1);
				writenum(buf, 2, COLVOL, cacheParam >> 4, 16, 1, 0);
			} else if ((cacheParam & 0xF0)!=0x00)
			{
				writestring(buf, 1, COLVOL, "\x19", 1);
				writenum(buf, 2, COLVOL, cacheParam & 0xF, 16, 1, 0);
			} else {
				writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			}
			return;

		case CPlayer::TrackedCmdVibratoFine:
		case CPlayer::TrackedCmdVibrato:
			writestring(buf, 0, COLPITCH, "~", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdVibratoVolumeSlide:
			writestring(buf, 0, COLPITCH, "~", 1);
			if (!cacheParam)
			{
				writestring(buf, 1, COLVOL, "\x12""0", 2);
			} else {
				if (cacheParam&0xF0)
				{
					writestring(buf, 1, COLVOL, "\x18", 1);
					writenum(buf, 2, COLVOL, cacheParam>>4, 16, 1, 0);
				} else {
					writestring(buf, 1, COLVOL, "\x19", 1);
					writenum(buf, 2, COLVOL, cacheParam&0xF, 16, 1, 0);
				}
			}
			return;

		case CPlayer::TrackedCmdReleaseSustainedNotes:
			writestring(buf, 0, COLACT, "^", 1);
			writenum(buf, 1, COLACT, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdVolumeSlideUpDown:
			if ((cacheParam & 0xF0)!=0x00)
			{
				writestring(buf, 0, COLVOL, "\x18", 1);
				writenum(buf, 1, COLVOL, cacheParam >> 4, 16, 2, 0);
			} else if ((cacheParam &0xF0)!=0x00)
			{
				writestring(buf, 1, COLVOL, "\x19", 1);
				writenum(buf, 1, COLVOL, cacheParam & 0xF, 16, 2, 0);
			} else {
				writestring(buf, 1, COLVOL, "\x12", 1);
				writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			}
			return;

		case CPlayer::TrackedCmdVolumeFineSlideUp:
			writestring(buf, 0, COLVOL, "+", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdVolumeFineSlideDown:
			writestring(buf, 0, COLVOL, "-", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdVolumeFadeIn:
			writestring(buf, 0, COLVOL, "\x1a", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLCarrierModulatorVolume:
			writestring(buf, 0, COLOPL, "!", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLCarrierVolume:
			writestring(buf, 0, COLOPL, "c", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLModulatorVolume:
			writestring(buf, 0, COLOPL, "m", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLCarrierModulatorWaveform:
			writestring(buf, 0, COLOPL, "~", 1);
			writenum(buf, 1, COLOPL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLTremoloVibrato:
			writestring(buf, 0, COLOPL, "!", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLTremolo:
			writestring(buf, 0, COLOPL, "~", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLVibrato:
			writestring(buf, 0, COLOPL, "~", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPL3Multiplier:
			writestring(buf, 0, COLOPL, "M", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLFeedback:
			writestring(buf, 0, COLOPL, "f", 1);
			writenum(buf, 1, COLPITCH, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPL3Volume:
			writestring(buf, 0, COLOPL, "v", 1);
			writenum(buf, 1, COLVOL, cacheParam, 16, 2, 0);
			return;

		case CPlayer::TrackedCmdOPLVoiceMode:
			writestring(buf, 0, COLOPL, "voc", 3);
			return;

		case CPlayer::TrackedCmdOPLDrumMode:
			writestring(buf, 0, COLOPL, "drm", 3);
			return;

		case CPlayer::TrackedCmdRetrigger:
			writestring(buf, 0, COLACT, "\x13", 1);
			writenum(buf, 1, COLACT, cacheParam, 16, 2, 0);
			return;
	}
}

static struct cpitrakdisplaystruct oplptrkdisplay=
{
	getcurpos, getpatlen, getpatname, seektrack, startrow, getnote,
	getins, getvol, getpan, getfx, getgcmd
};

void __attribute__ ((visibility ("internal"))) oplTrkSetup(CPlayer *p)
{
	curPosition = 0xffff;
	trkP = p;
	cacheChannels = trkP->getnchans();

	int plOrders=trkP->getorders();
	if (plOrders && trkP->getrows())
	{
		cpiTrkSetup2(&oplptrkdisplay, plOrders, cacheChannels); /* the tracker tracks does not map 1:1 to physical channels, usually 1:2 */
	}
}

void __attribute__ ((visibility ("internal"))) oplTrkDone (void)
{
	free (pattern); pattern = 0;
	patterndim = 0;
	curPosition = 0xffff;
}
