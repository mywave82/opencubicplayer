/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Opus file type detection routines for fileselector
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
 *  -ss040911  Stian Skjelstad <stian@nixia.no>
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
#include "stuff/err.h"
#include "opustype.h"

static int opusReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	unsigned int i;

	if (len<27)
	{
		return 0;
	}
	/* OggS   Ogg data header */
	if (*(uint32_t*)buf!=uint32_little(0x5367674f)) // Oggs
	{
		return 0;
	}

	uint8_t lacings = ((uint8_t *)buf)[26];
	if (!lacings)
	{
		return 0;
	}
	if (len < (27 + lacings))
	{
		return 0;
	}
	uint32_t packetsize = 0;
	for (i=0; i < lacings; i++)
	{
		packetsize += ((uint8_t *)buf)[0x1b + i];
	}
	buf += 27 + lacings;
	len -= 27 + lacings;

	if (len < packetsize)
	{
		return 0;
	}

	/* https://wiki.xiph.org/OggOpus
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1| Byte
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | opus_string: Identifier char[8]: 'OpusHead'                   | 0-3
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 4-7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | version      | channels       | pre-skip [2]                  | 8-11
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | original input sample rate [4]                                | 12-15
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | output gain Q7.8              | channel map   | optional      | 16-19
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | variable length channel-layout map                            | 20-23
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 24-27
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 28-31
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 ................
	*/

	if (packetsize < 19)
	{
		return 0;
	}
	/* Opus audio ? */
	if ( (*(uint32_t *)(buf + 0) != uint32_little(0x7375704f)) || // "Opus"
	     (*(uint32_t *)(buf + 4) != uint32_little(0x64616548)))   // "Head"
	{
		return 0;
	}

	m->modtype.integer.i = MODULETYPE("OPUS");
	uint8_t  version      =              (*((uint8_t  *)(buf + 8)));
	uint8_t  channels     =              (*((uint8_t  *)(buf + 9)));
//	uint16_t preskip      = uint16_little(*((uint16_t *)(buf + 10)));
	uint32_t originalrate = uint32_little(*((uint32_t *)(buf + 12)));
//	uint16_t outputgain   = uint16_little(*((uint32_t *)(buf + 16)));
	uint8_t  channelmap   =              (*((uint8_t  *)(buf + 18)));

	char versionstring[24];

	if (version != 1)
	{
		snprintf(versionstring, sizeof (versionstring), "Invalid version %d, ", version);
	} else {
		versionstring[0] = 0;
	}

	char channelstring[48];

	if (channelmap == 0)
	{
		if (channels == 1)
		{
			strcpy (channelstring, "mono");
		} else if (channels == 2)
		{
			strcpy (channelstring, "stereo");
		} else {
			snprintf (channelstring, sizeof (channelstring), "%d? channels (invalid for channel map)", channels);
		}
	} else if (channelmap == 1)
	{
		switch (channels)
		{
			default:
			case 0: snprintf (channelstring, sizeof (channelstring), "%d? channels (invalid for channel map)", channels); break;
			case 1: strcpy (channelstring, "Mono"); break;
			case 2: strcpy (channelstring, "Stereo"); break;
			case 3: strcpy (channelstring, "3.0 (3 channels)"); break;
			case 4: strcpy (channelstring, "Quadrophonic (4 channels)"); break;
			case 5: strcpy (channelstring, "5.0 (5 channels)"); break;
			case 6: strcpy (channelstring, "5.1 (6 channels)"); break;
			case 7: strcpy (channelstring, "6.1 (7 channels)"); break;
			case 8: strcpy (channelstring, "7.1 (8 channels)"); break;
		}
	} else {
		snprintf (channelstring, sizeof (channelstring), "%d? channels (invalid channel map)", channels);
	}

	snprintf (m->comment, sizeof (m->comment), "%s%s, originally %dHz",
		versionstring,
		channelstring,
		originalrate);

	buf += packetsize;
	len -= packetsize;

	if (len<27)
	{
		return 1;
	}
	/* OggS   Ogg data header */
	if (*(uint32_t*)buf!=int32_little(0x5367674f)) // Oggs
	{
		return 1;
	}

	lacings = ((uint8_t *)buf)[26];
	if (!lacings)
	{
		return 1;
	}
	if (len < (27 + lacings))
	{
		return 1;
	}
	packetsize = 0;
	for (i=0; i < lacings; i++)
	{
		packetsize += ((uint8_t *)buf)[0x1b + i];
	}
	buf += 27 + lacings;
	len -= 27 + lacings;

#if 0  /* if it contains pictures, it might be huuuuge, so allow partial capture */
	if (len < packetsize) // did we capture the entire packet?
	{
		return 1;
	}
#endif

	if ((packetsize < 12) || (len < 12))
	{
		return 1;
	}

	if (memcmp (buf, "OpusTags", 8))
	{
		return 1;
	}

	packetsize -= 8;
	len -= 8;
	buf += 8;

	uint32_t encoder_length = uint32_little(*((uint32_t *)buf));
	if (encoder_length > 1024) // likely, this value is 0x0d
	{
		return 1;
	}
	if ((packetsize < (encoder_length + 4)) ||
	    (len        < (encoder_length + 4)))
	{
		return 1;
	}
	// skip encoder
	buf += encoder_length + 4;
	len -= encoder_length + 4;
	packetsize -= encoder_length + 4;

	if ((packetsize < 4) || (len < 4))
	{
		return 1;
	}

	uint32_t count = uint32_little(((uint32_t *)buf)[0]);
	buf += 4;
	len -= 4;
	packetsize -= 4;
	for (i=0;i<count;i++)
	{
		if ((packetsize < 4) ||
		    (len        < 4))
		{
			return 1;
		}
		uint32_t length=uint32_little(*(uint32_t *)buf);
		if ((length > 16*1024*1024))
		{
			return 1;
		}
		buf += 4;
		len -= 4;
		packetsize -= 4;
		if ((packetsize < length) ||
		    (len        < length))
		{
			return 1;
		}
		if ((length >= 7) && (!strncasecmp(buf, "artist=", 7)))
		{
			int copy = length - 7;
			if (copy >= sizeof (m->artist))
			{
				copy = sizeof (m->artist)-1;
			}
			memset (m->artist, 0, sizeof (m->artist));
			memcpy (m->artist, buf + 7, copy);
		} else if ((length >= 6) && (!strncasecmp(buf, "title=", 6)))
		{
			int copy = length - 6;
			if (copy >= sizeof (m->title))
			{
				copy = sizeof (m->title)-1;
			}
			memset (m->title, 0, sizeof (m->title));
			memcpy (m->title, buf + 6, copy);
		} else if ((length >= 6) && (!strncasecmp(buf, "album=", 6)))
		{
			int copy = length - 6;
			if (copy >= sizeof (m->album))
			{
				copy = sizeof (m->album)-1;
			}
			memset (m->album, 0, sizeof (m->album));
			memcpy (m->album, buf + 6, copy);
		} else if ((length >= 6) &&(!strncasecmp(buf, "genre=", 6)))
		{
			int copy = length - 6;
			if (copy >= sizeof (m->style))
			{
				copy = sizeof (m->style)-1;
			}
			memset (m->style, 0, sizeof (m->style));
			memcpy (m->style, buf + 6, copy);
		} else if ((length >= 9) &&(!strncasecmp(buf, "composer=", 9)))
		{
			int copy = length - 9;
			if (copy >= sizeof (m->composer))
			{
				copy = sizeof (m->composer)-1;
			}
			memset (m->composer, 0, sizeof (m->composer));
			memcpy (m->composer, buf + 9, copy);
		}

		buf += length;
		len -= length;
		packetsize -= length;
	}
	return 1;
}

static const char *OPUS_description[] =
{
	//                                                                          |
	"Opus is an codec focused on compression speech and music. It is a",
	"replacement for Speex and partially the Vorbis audio codecs. The main usage"
	"is audio streaming over the Internet. It is developed by Xiph."
};

static struct mdbreadinforegstruct opusReadInfoReg = {"OPUS", opusReadInfo MDBREADINFOREGSTRUCT_TAIL};

OCP_INTERNAL int opus_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("OPUS");

	mt.integer.i = MODULETYPE("OPUS");
	API->fsTypeRegister (mt, OPUS_description, "plOpenCP", &opusPlayer);

	API->mdbRegisterReadInfo(&opusReadInfoReg);
	return errOk;
}

OCP_INTERNAL void opus_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("OPUS");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&opusReadInfoReg);
}
