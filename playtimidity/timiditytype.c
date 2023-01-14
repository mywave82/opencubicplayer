/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/cp437.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "timiditytype.h"

static int timidityReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *_buf, size_t flen, const struct mdbReadInfoAPI_t *API)
{
	const uint8_t *buf = (const uint8_t *)_buf;
	uint16_t mtype;
	unsigned int i;
	uint32_t chunklen;
	int IsSoftKaraoke = 0;
	int TrackNo = 0;

	if (flen<12)
	{
		return 0;
	}

	/* if RIFF container is present, verify RMID and locate the data, offset "i" and limit "flen" */
	if (!memcmp(buf, "RIFF", 4))
	{
		if (memcmp (buf + 8, "RMID", 4))
		{ /* not a RMID, bail out */
			return 0;
		}
		buf += 12; flen -= 12; /* skip the main RIFF header */

		while (1)
		{ /* and locate the data chunk */
			if (flen < 8)
			{ /* data-chunk not found */
				return 0;
			}
			chunklen = (buf[4]<<24)|(buf[5]<<16)|(buf[6]<<8)|(buf[7]);
			if (!memcmp (buf, "data", 4))
			{
				buf += 8; flen -= 8;
				if (flen > chunklen)
				{
					flen = chunklen;
				}
				break;
			}
			buf += 8; flen -= 8;
			if (flen < chunklen)
			{
				return 0;
			}
			buf += chunklen + (chunklen & 1); /* skip to the next chunk, and chunk datasize should be rounded up to 16bit alignment */
			flen -= chunklen + (chunklen & 1);
		}
	}

	/* verify MThd */
	if ((flen < 22) || memcmp(buf, "MThd", 4))
	{
		return 0;
	}
	chunklen = (buf[4]<<24)|(buf[5]<<16)|(buf[6]<<8)|(buf[7]);
	buf += 8; flen -= 8;

	if (chunklen < 6)
	{
		return 0;
	}
	mtype = (buf[0]<<8)|(buf[1]);
	if (flen < chunklen)
	{
		return 0;
	}
	buf += chunklen; flen -= chunklen;

	/* This is MIDI file */
	m->modtype.integer.i = MODULETYPE("MIDI");
	m->channels=16;

	while (1)
	{
		char CurrentTrackName[256];
		CurrentTrackName[0] = 0;

		if ((TrackNo == 1) && mtype != 1) /* not multitrack format */
		{
			return 1;
		}

		while (1)
		{
			int MTrk;
			if (flen < 8)
			{
				return 1;
			}
			chunklen = (buf[4]<<24)|(buf[5]<<16)|(buf[6]<<8)|(buf[7]);
			MTrk = !memcmp(buf, "MTrk", 4);
			buf += 8; flen -= 8;
			if (chunklen > flen) chunklen = flen;
			if (MTrk)
			{
				TrackNo++;
				break;
			}
			buf += chunklen; flen -= chunklen;
		}

		i = 0;
		while ((i+4) < chunklen)
		{
			uint16_t eventType = (buf[i+1]<<8) | buf[i+0];
			/* We only visit the initial META events */
			if (eventType != 0xff00)
			{
				if (TrackNo != 1)
				{ /* We only care about CurrentTrackName if it is track 1 or a track that is pure META events */
					CurrentTrackName[0] = 0;
				}
				break;
			}

			switch (IsSoftKaraoke)
			{
				case 0: /* Find META event 0x03 Sequence/Track Name, and verify if it is "Soft Karaoke", if we are in Track 2 */
					if (buf[i + 2] == 0x03)
					{
						snprintf (CurrentTrackName, sizeof (CurrentTrackName), "%.*s", buf[i + 3], (char *)buf + i + 4);
						if ((TrackNo == 2) && !strcasecmp (CurrentTrackName, "Soft Karaoke"))
						{
							IsSoftKaraoke = 1;
						}
					}
					break;

				case 1: /* Find META event 0x01 Text, that starts with @T, if we are in Track 3 */
					if ((buf[i + 2] == 0x01) && (TrackNo == 3))
					{
						if ((4 + i + buf[i + 3]) > chunklen)
						{
							return 1;
						}
						if ((buf[i + 3] >= 2) && !memcmp (buf + i + 4, "@T", 2))
						{
							API->cp437_f_to_utf8_z ((char *)buf + i + 4 + 2, buf[i + 3] - 2, m->title, sizeof (m->title));
							IsSoftKaraoke = 2;
						}
					}
					break;
				case 2: /* Find META event 0x01 Text, that starts with @T, if we are in Track 3 */
					if ((buf[i + 2] == 0x01) && (TrackNo == 3))
					{
						if ((4 + i + buf[i + 3]) > chunklen)
						{
							return 1;
						}
						if ((buf[i + 3] >= 2) && !memcmp (buf + i + 4, "@T", 2))
						{
							API->cp437_f_to_utf8_z ((char *)buf + i + 4 + 2, buf[i + 3] - 2, m->artist, sizeof (m->artist));
							IsSoftKaraoke = 3;
						}
					}
					break;
				case 3: /* Find META event 0x01 Text, that starts with @T, if we are in Track 3 */
					if ((buf[i + 2] == 0x01) && (TrackNo == 3))
					{
						if ((4 + i + buf[i + 3]) > chunklen)
						{
							return 1;
						}
						if ((buf[i + 3] >= 2) && !memcmp (buf + i + 4, "@T", 2))
						{
							API->cp437_f_to_utf8_z ((char *)buf + i + 4 + 2, buf[i + 3] - 2, m->comment, sizeof (m->comment));
							return 1; /* IsSoftKaraokee = 4; */
						}
					}
					break;
			}
			i += 4 + buf[i + 3];
		}

		if (!IsSoftKaraoke)
		{
			if (CurrentTrackName[0])
			{
				API->cp437_f_to_utf8_z (CurrentTrackName, strlen (CurrentTrackName), m->title, sizeof (m->title));
				if (TrackNo != 1)
				{
					return 1; /* Track 1 and later, it is a pure META, do not try fetch more */
				}
			}
		} else {
			if (TrackNo >= 3)
			{
				return 1; /* We only scan Track number 3 for text fields that start with @T and then bail out */
			}
		}

		buf += chunklen; flen -= chunklen;
	}

	return 1;
}

static const char *MIDI_description[] =
{
	//                                                                          |
	"MIDI files are music files that only contains note and meta data. MIDI in",
	"itself is also a standard for transferring control signal between",
	"synthesizers and other audio equipment. A MIDI file is record of such a",
	"signal together with timestamps. Open Cubic Player relies on TiMidity++",
	"code to playback and requires that the host has configured an instrument",
	"sound font in order to function (install TiMidity++ usually does the trick).",
	NULL
};

static struct mdbreadinforegstruct timidityReadInfoReg = {"MIDI", timidityReadInfo MDBREADINFOREGSTRUCT_TAIL};

int __attribute__ ((visibility ("internal"))) timidity_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("MID");
	API->fsRegisterExt ("MIDI");
	API->fsRegisterExt ("RMI");
	API->fsRegisterExt ("KAR");

	mt.integer.i = MODULETYPE("MIDI");
	API->fsTypeRegister (mt, MIDI_description, "plOpenCP", &timidityPlayer);

	API->mdbRegisterReadInfo(&timidityReadInfoReg);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) timidity_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("MIDI");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&timidityReadInfoReg);
}
