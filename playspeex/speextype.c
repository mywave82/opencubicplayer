/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SpeexPlay file type detection routines for fileselector
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
#include "speextype.h"

static int speexReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *buf, size_t len, const struct mdbReadInfoAPI_t *API)
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

	/* https://wiki.xiph.org/OggSpeex
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1| Byte
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | speex_string: Identifier char[8]: 'Speex   '                  | 0-3
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 4-7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | speex_version: char[20]                                       | 8-11
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 12-15
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 16-19
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 20-23
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               | 24-27
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | speex_version_id                                              | 28-31
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | header_size                                                   | 32-35
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | rate                                                          | 36-39
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | mode                                                          | 40-43
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | mode_bitstream_version                                        | 44-47
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | nb_channels                                                   | 48-51
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | bitrate                                                       | 52-55
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | frame_size                                                    | 56-59
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | vbr                                                           | 60-63
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | frames_per_packet                                             | 64-67
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | extra_headers                                                 | 68-71
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | reserved1                                                     | 72-75
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | reserved2                                                     | 76-79
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/

	if (packetsize < 80)
	{
		return 0;
	}
	/* Speex audio ? */
	if ( (*(uint32_t *)(buf + 0) != uint32_little(0x65657053)) || // "Spee"
	     (*(uint32_t *)(buf + 4) != uint32_little(0x20202078)))   // "x   "
	{
		return 0;
	}

	m->modtype.integer.i = MODULETYPE("SPX");
	uint32_t rate        = uint32_little(*((uint32_t *)(buf + 36)));
	uint32_t mode        = uint32_little(*((uint32_t *)(buf + 40)));
	uint32_t channels    = uint32_little(*((uint32_t *)(buf + 48)));
	 int32_t bitrate     =  int32_little(*((uint32_t *)(buf + 52)));
	uint32_t vbr         = uint32_little(*((uint32_t *)(buf + 60)));

	char BR[16];

	if (vbr)
	{
		snprintf (BR, sizeof (BR), ", VBR");
	} else if (bitrate > 0)
	{
		snprintf (BR, sizeof (BR), ", %"PRId32"bps", bitrate);
	} else {
		/* quality encoded, parameter is not stored in the header */
		BR[0] = 0;
	}

	m->channels = channels;
	snprintf (m->comment, sizeof (m->comment), "%s, %"PRIu32"Hz%s, %s", (channels==2) ? "Stereo" : "Mono", rate, BR, (mode==0) ? "NarrowBand" : (mode==1) ? "WideBand" : (mode==2) ? "UltraWideBand" : "Unknown mode");

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

	if ((packetsize < 4) || (len < 4))
	{
		return 1;
	}
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

static const char *SPX_description[] =
{
	//                                                                          |
	"Speex is an codec focused on compression speech. It has been replaced by",
	"the more modern Opus codec and is considered obsolete by its host, Xiph",
	NULL
};

static struct mdbreadinforegstruct speexReadInfoReg = {"SPX", speexReadInfo MDBREADINFOREGSTRUCT_TAIL};

OCP_INTERNAL int speex_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("SPX");

	mt.integer.i = MODULETYPE("SPX");
	API->fsTypeRegister (mt, SPX_description, "plOpenCP", &speexPlayer);

	API->mdbRegisterReadInfo(&speexReadInfoReg);
	return errOk;
}

OCP_INTERNAL void speex_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("SPX");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&speexReadInfoReg);
}
