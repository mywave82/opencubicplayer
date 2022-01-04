/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '05-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMIPlay file type detection routines for the fileselector
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
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/cp437.h"
#include "stuff/compat.h"

static uint32_t gmiGetModuleType (const uint8_t *buf)
{
	if (*(uint32_t*)buf==uint32_little(0x6468544D)) /* "MThd"  - midi without a RIFF container */
		return MODULETYPE("MIDI");
	if ((*(uint32_t*)buf==uint32_little(0x46464952))&&(*(uint32_t*)(buf+8)==uint32_little(0x44494D52))) /* "RIFF" "RMID" - RIFF container for MIDI */
		return MODULETYPE("MIDI");

	return 0;
}

static int gmiReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *_buf, size_t flen)
{
	const uint8_t *buf = (const uint8_t *)_buf;
	int type;
	unsigned int i;
	uint32_t len;

	if (flen<12)
	{
		return 0;
	}

	if (!(type=gmiGetModuleType(buf)))
	{
		return 0;
	}
	m->modtype.integer.i=type;

	len=0;
	m->channels=16;

	i=0;
	/* if RIFF container is present, locate the MIDI data */
	if (*(uint32_t*)buf==uint32_little(0x46464952)) /* RIFF */
	{
		i=12;
		while ((i+8) < flen)
		{
			i+=8;
			if (*(uint32_t*)(buf+i-8)==uint32_little(0x61746164)) /* data */
				break;
			i+=uint32_little(*(uint32_t*)(buf+i-4));
		}
	}
	while ((i+8) < flen)
	{
		i+=8;
		len=(buf[i-4]<<24)|(buf[i-3]<<16)|(buf[i-2]<<8)|(buf[i-1]);
		if (!memcmp(buf+i-8, "MTrk", 4))
			break;
		i+=len;
	}
	len+=i;

	while ((i+4)<len)
	{
		int datalen;

		/* Is the next even a META event, if not give up */
		if (*(uint16_t*)(buf+i)!=uint16_little(0xFF00))
			break;
		/* the the META event 0x03 Sequence/Track Name, else skip it */
		if (buf[i+2]!=0x03)
		{
			i+=4+buf[i+3];
			continue;
		}
		datalen=buf[i+3];
		if (i + datalen + 4 <= len)
		{
			cp437_f_to_utf8_z ((char *)buf+i+4, datalen, m->title, sizeof (m->title));
		}
		break;
	}
	return 1;
}

const char *MIDI_description[] =
{
	//                                                                          |
	"MIDI files are music files that only contains note and meta data. MIDI in",
	"itself is also a standard for transfering control signal between",
	"synthesizers and other audio equipment. A MIDI file is record of such a",
	"signal together with timestamps. Open Cubic Player relies on TiMidity++",
	"code to playback and requires that the host has configured an instrument",
	"sound font in order to function (install TiMidity++ usually does the trick).",
	NULL
};

struct interfaceparameters MIDI_p =
{
	"playtimidity", "timidityPlayer",
	0, 0
};


struct mdbreadinforegstruct gmiReadInfoReg = {"MIDI", gmiReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
