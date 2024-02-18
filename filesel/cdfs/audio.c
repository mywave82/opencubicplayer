/* OpenCP Module Player
 * copyright (c) 2022-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Detection of Audio-Tracks in CDFS image
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
#include <assert.h>
#include <discid/discid.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "cdfs.h"
#include "filesel/musicbrainz.h"

OCP_INTERNAL void Check_Audio (struct cdfs_disc_t *disc)
{
	int first = 0;
	int last = 0;
	int i;
	uint32_t AudioDir;
	for (i=1; i < disc->tracks_count; i++)
	{
		switch (cdfs_get_sector_format (disc, disc->tracks_data[i].start + disc->tracks_data[i].pregap))
		{
			case FORMAT_AUDIO___NONE:
			case FORMAT_AUDIO___RW:
			case FORMAT_AUDIO___RAW_RW:
			case FORMAT_AUDIO_SWAP___NONE:
			case FORMAT_AUDIO_SWAP___RW:
			case FORMAT_AUDIO_SWAP___RAW_RW:
				if (!first)
				{
					first = i;
				}
				last = i;
				break;
			default:
			case FORMAT_RAW___NONE:
			case FORMAT_RAW___RW:
			case FORMAT_RAW___RAW_RW:
			case FORMAT_MODE1_RAW___NONE:
			case FORMAT_MODE1_RAW___RW:
			case FORMAT_MODE1_RAW___RAW_RW:
			case FORMAT_MODE2_RAW___NONE:
			case FORMAT_MODE2_RAW___RW:
			case FORMAT_MODE2_RAW___RAW_RW:
			case FORMAT_XA_MODE2_RAW:
			case FORMAT_XA_MODE2_RAW___RW:
			case FORMAT_XA_MODE2_RAW___RAW_RW:
			case FORMAT_MODE1___NONE:
			case FORMAT_MODE1___RW:
			case FORMAT_MODE1___RAW_RW:
			case FORMAT_XA_MODE2_FORM1___NONE:
			case FORMAT_XA_MODE2_FORM1___RW:
			case FORMAT_XA_MODE2_FORM1___RAW_RW:
			case FORMAT_MODE_1__XA_MODE2_FORM1___NONE:
			case FORMAT_MODE_1__XA_MODE2_FORM1___RW:
			case FORMAT_MODE_1__XA_MODE2_FORM1___RAW_RW:
			case FORMAT_MODE2___NONE:
			case FORMAT_MODE2___RW:
			case FORMAT_MODE2___RAW_RW:
			case FORMAT_XA_MODE2_FORM2___NONE:
			case FORMAT_XA_MODE2_FORM2___RW:
			case FORMAT_XA_MODE2_FORM2___RAW_RW:
			case FORMAT_XA_MODE2_FORM_MIX___NONE:
			case FORMAT_XA_MODE2_FORM_MIX___RW:
			case FORMAT_XA_MODE2_FORM_MIX___RAW_RW:
			case FORMAT_XA1_MODE2_FORM1___NONE:
			case FORMAT_XA1_MODE2_FORM1___RW:
			case FORMAT_XA1_MODE2_FORM1___RW_RAW:
			case FORMAT_ERROR:
				break;
		}
	}

	if (!last)
	{
		return;
	}
	
	{
		DiscId *did = discid_new();
		int offsets[100];
		const char *discid;
		const char *toc;

		if (!did)
		{
			goto postfailout;
		}

		memset (offsets, 0, sizeof (offsets));
		for (i=1; i <= last; i++)
		{
			offsets[i] = disc->tracks_data[i].start + 150;
			offsets[0] = disc->tracks_data[i].start + disc->tracks_data[i].length + 150;
		}

		if (!discid_put (did, first, last, offsets))
		{
			goto failout;
		}

		if (!discid_put (did, first, last, offsets))
		{
			goto failout;
		}

		discid = discid_get_id (did);
		toc = discid_get_toc_string (did);
		if (discid && toc)
		{
			disc->discid = strdup (discid);
			disc->toc = strdup (toc);
			disc->musicbrainzhandle = musicbrainz_lookup_discid_init (disc->discid, disc->toc, &disc->musicbrainzdata);
		}
failout:
		discid_free (did);
	}
postfailout:

	AudioDir = CDFS_Directory_add (disc, 0, "AUDIO");

	{
		char discname_long[64];
		char discname_short[16];

		snprintf (discname_long, 64, "%sDISC.CDA", disc->discid?disc->discid:"");
		snprintf (discname_short, 16, "DISC.CDA");

		CDFS_File_add_audio (disc, AudioDir, discname_short, discname_long, (disc->tracks_data[last].start + disc->tracks_data[last].length) * 2352, 100);
	}

	for (i=1; i < disc->tracks_count; i++)
	{
		char trackname_long[64];
		char trackname_short[16];

		assert (i < 100);

		switch (cdfs_get_sector_format (disc, disc->tracks_data[i].start + disc->tracks_data[i].pregap))
		{
			case FORMAT_AUDIO___NONE:
			case FORMAT_AUDIO___RW:
			case FORMAT_AUDIO___RAW_RW:
			case FORMAT_AUDIO_SWAP___NONE:
			case FORMAT_AUDIO_SWAP___RW:
			case FORMAT_AUDIO_SWAP___RAW_RW:
				snprintf (trackname_long, 64, "%sTRACK%02d.CDA", disc->discid?disc->discid:"", i);
				snprintf (trackname_short, 16, "TRACK%02d.CDA", i);

				CDFS_File_add_audio (disc, AudioDir, trackname_short, trackname_long, disc->tracks_data[i].length * 2352, i);
				break;
			default:
				break;
		}
	}
}
