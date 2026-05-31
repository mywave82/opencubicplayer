/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WavPack file type detection routines for fileselector
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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "wavpacktype.h"

static int APEParseHeader (const unsigned char *buf, const int ishead, uint32_t *tagsize, uint32_t *tagcount, uint32_t *tagsflags)
{
	int retval = 0;

	uint32_t tagversion = ((uint32_t)(buf[ 8])      ) |
	                      ((uint32_t)(buf[ 9]) << 8 ) |
	                      ((uint32_t)(buf[10]) << 16) |
	                      ((uint32_t)(buf[11]) << 24);

	if (tagversion == 1000)
	{
		if (ishead)
		{
#ifdef PLAYWAVPACK_DEBUG
			fprintf (stderr, "APE Tag %s is a v2.000 feature, not v1.000\n", ishead?"Header":"Footer");
#endif
		}
	} else if (tagversion != 2000)
	{
#ifdef PLAYWAVPACK_DEBUG
		fprintf (stderr, "Unknown APE Tag %s version %u.%03u\n", ishead?"Header":"Footer", (unsigned int)(tagversion/1000), (unsigned int)(tagversion % 1000));
#endif
		retval = -1;
	}

	*tagsize   = ((uint32_t)(buf[12])      ) |
	             ((uint32_t)(buf[13]) << 8 ) |
	             ((uint32_t)(buf[14]) << 16) |
	             ((uint32_t)(buf[15]) << 24);

	*tagcount  = ((uint32_t)(buf[16])      ) |
	             ((uint32_t)(buf[17]) << 8 ) |
	             ((uint32_t)(buf[18]) << 16) |
	             ((uint32_t)(buf[19]) << 24);

	*tagsflags = ((uint32_t)(buf[20])      ) |
	             ((uint32_t)(buf[21]) << 8 ) |
	             ((uint32_t)(buf[22]) << 16) |
	             ((uint32_t)(buf[23]) << 24);

	int i;
	for (i=24; i < 32; i++)
	{
		if (buf[i])
		{
#ifdef PLAYWAVPACK_DEBUG
			fprintf (stderr, "APE TAG %s byte %d is reserved, but found value 0x%02x\n", ishead?"Header":"Footer", i, buf[i]);
#endif
			retval = -1;
		}
	}

	if (tagversion == 0x1000)
	{
		if (*tagsflags)
		{
#ifdef PLAYWAVPACK_DEBUG
			fprintf (stderr, "APE TAG version 1.000 should have flags set to zero, but they are 0x%"PRIx32"\n", *tagsflags);
#endif
		}
	} else if (tagversion == 0x2000)
	{
#ifdef PLAYWAVPACK_DEBUG
		if (ishead)
		{
			if ((*tagsflags & 0x20000000) == 0)
			{
			fprintf (stderr, "APE TAG Header, but flags says Footer\n");
			}
		} else if ((*tagsflags & 0x20000000) != 0)
		{
			fprintf (stderr, "APE TAG Footer, but flags says Header\n");
		}
		if (*tagsflags & 0x1ffffff8)
		{
			fprintf (stderr, "APE TAG %s, reserved bit(s) in flags set: 0x%"PRIx32"\n", ishead?"Header":"Footer", *tagsflags & 0x1ffffff8);
		}
#endif
	}

	if (*tagcount > 10000)
	{
#ifdef PLAYWAVPACK_DEBUG
		fprintf (stderr, "APE TAG %s, insane amount of tags: %"PRIu32"\n", ishead?"Header":"Footer", *tagcount);
#endif
		retval = -1;
	}

	if ((*tagcount * 10 + 32) > *tagsize)
	{
#ifdef PLAYWAVPACK_DEBUG
		fprintf (stderr, "APE TAG %s, tagsize %"PRIu32" is unable to store all the indicated tagcount %"PRIu32"\n", ishead?"Header":"Footer", *tagsize, *tagcount);
#endif
		retval = -1;
	}

	return retval;
}

static int APEParseTags (const unsigned char *buf, uint32_t length, uint32_t count, struct moduleinfostruct *m)
{
	while (count)
	{
		if (length < 10)
		{
#ifdef PLAYWAVPACK_DEBUG
			fprintf (stderr, "APE TAG run out of data, before reaching tag count\n");
#endif
			return 1;
		}

		uint32_t ItemValueSize = ((uint32_t)(buf[0])      ) |
		                         ((uint32_t)(buf[1]) << 8 ) |
		                         ((uint32_t)(buf[2]) << 16) |
		                         ((uint32_t)(buf[3]) << 24);

		uint32_t ItemFlags     = ((uint32_t)(buf[4])      ) |
		                         ((uint32_t)(buf[5]) << 8 ) |
		                         ((uint32_t)(buf[6]) << 16) |
		                         ((uint32_t)(buf[7]) << 24);

		const char *Key = (const char *)buf + 8;
		const char *KeyEnd = memchr (buf + 8, 0, length - 8);
		if (!KeyEnd)
		{
#ifdef PLAYWAVPACK_DEBUG
			fprintf (stderr, "APE TAG, missing terminator for a key\n");
#endif
			return 1;
		}

		if ((ItemValueSize > length) || ((ItemValueSize + 10) > length) || ((ItemValueSize + strlen(Key) + 9) > length) )
		{
#ifdef PLAYWAVPACK_DEBUG
			fprintf (stderr, "APE TAG: Run out of data, can not fit value (ItemValueSize %"PRIu32" + strlen(Key) %u + 9) > length %"PRIu32"\n", ItemValueSize, (unsigned int)strlen(Key), length);
#endif
			return 1;
		}

		if ((ItemFlags & 0x6) == 0)
		{
			const char *Value = KeyEnd + 1;
			if (!strcasecmp (Key, "Artist"))
			{
				size_t s = ItemValueSize;
				if (s > MDB_ARTIST_LEN) s = MDB_ARTIST_LEN;
				/* we expect m->artist to only be NULL terminated if shorter than MDB_ARTIST_LEN */
				strncpy (m->artist, Value, s);
			} else if (!strcasecmp (Key, "Title"))
			{
				size_t s = ItemValueSize;
				if (s > MDB_TITLE_LEN) s = MDB_TITLE_LEN;
				/* we expect m->title to only be NULL terminated if shorter than MDB_TITLE_LEN */
				strncpy (m->title, Value, s);
			} else if (!strcasecmp (Key, "Album"))
			{
				size_t s = ItemValueSize;
				if (s > MDB_ALBUM_LEN) s = MDB_ALBUM_LEN;
				/* we expect m->album to only be NULL terminated if shorter than MDB_ALBUM_LEN */
				strncpy (m->album, Value, s);
			} else if (!strcasecmp (Key, "Year"))
			{
				if ((ItemValueSize == 2) && isdigit(Value[0]) && isdigit(Value[1]))
				{
					m->date = (1900 + (Value[0] - '0') * 10 + (Value[1] - '0')) << 16;
				} else if ((ItemValueSize == 4) && isdigit(Value[0]) && isdigit(Value[1]) && isdigit(Value[2]) && isdigit(Value[3]))
				{
						m->date = ((Value[0] - '0') * 1000 + (Value[1] - '0') * 100 + (Value[2] - '0') * 10 + (Value[3] - '0')) << 16;
				}
			} else if (!strcasecmp (Key, "Genre"))
			{
				size_t s = ItemValueSize;
				if (s > MDB_STYLE_LEN) s = MDB_STYLE_LEN;
				/* we expect m->artist to only be NULL terminated if shorter than MDB_STYLE_LEN */
				strncpy (m->style, Value, s);
			} else if (!strcasecmp (Key, "Comment"))
			{
				size_t s = ItemValueSize;
				if (s > MDB_COMMENT_LEN) s = MDB_COMMENT_LEN;
				/* we expect m->artist to only be NULL terminated if shorter than MDB_COMMENT_LEN */
				strncpy (m->comment, Value, s);
			}
			/* We currently ignore:
			 * "Track"
			 * "Cuesheet"
			 * "Encoder"
			 * "Settings"
			 * "Replaygain_Track_Gain"
			 * "Replaygain_Track_Peak"
			 * "Replaygain_Album_Gain"
			 * "Replaygain_Album_Peak"
			 * "Cover Art (Front)"
			 * "Cover Art (Back)"
			 * "Log"
			 */
		} else {
#ifdef PLAYWAVPACK_DEBUG
			fprintf (stderr, "DEBUG: Tag \"%s\" not stored as text\n", Key); // DEBUG
#endif
}

		const uint8_t *NextTag = (const uint8_t *)KeyEnd + 1 + ItemValueSize;
		length -= NextTag - buf;
		buf = NextTag;

		count--;
	}

	return 0;
}

static void parse_ID3v1x(const char *source, struct moduleinfostruct *m, const struct mdbReadInfoAPI_t *API)
{
	const char *z;
	z = memchr (source +  3, 0, 30); API->latin1_f_to_utf8_z (source +  3, z ? z - (source +  3) : 30, m->title, sizeof (m->title));
	z = memchr (source + 33, 0, 30); API->latin1_f_to_utf8_z (source + 33, z ? z - (source + 33) : 30, m->artist, sizeof (m->artist));
	z = memchr (source + 63, 0, 30); API->latin1_f_to_utf8_z (source + 63, z ? z - (source + 63) : 30, m->album, sizeof (m->album));

	if (isdigit(source[93]) && isdigit(source[94]) && isdigit(source[95]) && isdigit(source[96]))
	{
		m->date = ((source[93] - '0') * 1000 + (source[94] - '0') * 100 + (source[95] - '0') * 10 + (source[96] - '0')) << 16;
	}

  if (source[97])
	{
		z = memchr (source +  3, 0, 97); API->latin1_f_to_utf8_z (source + 97, z ? z - (source + 97) : 30, m->comment, sizeof (m->comment));
	}
#if 0
	if ((!source[125]) && (source[126]))
	{
		//m->track = source[126];
	}

	m->style = genres[(uint8_t)source[127]);
#endif
}

static int wavpackReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *_buf, size_t _len, const struct mdbReadInfoAPI_t *API)
{
	const uint8_t *buf = (const uint8_t *)_buf;
	size_t len = _len;
	int gottag = 0;

	/* Check header for APEv2 tag, they can be at the start of file */
	if (len >= 74) /* APE Tags Header, Item + Footer uses atleast 74 bytes */
	{
		uint32_t tagsize, tagcount, tagsflags;
		if (memcmp (buf, "APETAGEX", 8))
		{
			goto notag;
		}
		if (APEParseHeader(buf, 1, &tagsize, &tagcount, &tagsflags))
		{
			return 0; /* We know already now that there is no wavpack header at buf */
		}
		if ((len - 32) < tagsize)
		{
			return 0;
		}
		if (APEParseTags(buf + 32, tagsize - 32, tagcount, m))
		{
			return 0;
		}
		gottag = 1;
		uint64_t relstart = 32 + tagsize;
		buf += relstart;
		len -= relstart;
	}
	/* Check (new offset) header for wavpack "wvpk", and extract total_samples, block_index, flags */
notag:
	if (len < 30)
	{
		return 0;
	}
	if (memcmp (buf, "wvpk", 4))
	{
		return 0;
	}

	uint64_t blockindex = (((uint64_t)(buf[10])) << 32) |
	                      (((uint64_t)(buf[16]))      ) |
	                      (((uint64_t)(buf[17])) <<  8) |
	                      (((uint64_t)(buf[18])) << 16) |
	                      (((uint64_t)(buf[19])) << 24);

	uint64_t totalsamples = (((uint64_t)(buf[11])) << 32) |
	                        (((uint64_t)(buf[12]))      ) |
	                        (((uint64_t)(buf[13])) <<  8) |
	                        (((uint64_t)(buf[14])) << 16) |
	                        (((uint64_t)(buf[15])) << 24);

	uint32_t flags = (((uint64_t)(buf[24]))      ) |
	                 (((uint64_t)(buf[25])) <<  8) |
	                 (((uint64_t)(buf[26])) << 16) |
	                 (((uint64_t)(buf[27])) << 24);

	if (flags & 0x04)
	{
		m->channels = 2; /* stereo */
	} else {
		m->channels = 1; /* mono */
	}

	if (m->comment[0] == 0)
	{
		snprintf (m->comment, sizeof (m->comment), "%dbit, %s, %s, %s", ( (flags & 0x03) + 1 ) * 8, m->channels==1?"mono":(m->flags&0x10)?"joint-stereo":"stereo", (flags&0x08)?"loss-less":"hybrid", (flags&0x10)?"floating-point":"integer");
	}

#if 0
	switch (flags & 0x03)
	{
		case 0: // 8 bits
		case 1: // 16 bits
		case 2: // 24 bits
		case 3: // 32 bits
	}
	
	if (flags & 0x04)
	{
		// stereo
	} else {
		// mono
	}

	if (flags & 0x08)
	{
		// lossless
	} else {
		// hybrid
	}

	if (flags & 0x10)
	{
		// joint-stereo
	}

	if (flags & 0x80)
	{
		// floating point
	} else {
		// integer
	}
#endif

	m->modtype.integer.i = MODULETYPE("WV");

	const uint32_t sample_rates [] = { 6000, 8000, 9600, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 192000, 0 };
	uint32_t sample_rate = sample_rates[(flags >> 26) & 0xf];
	if (sample_rate)
	{
		m->playtime = totalsamples / sample_rate;
	}

	/* if no header tag, look for ID3v1 and APEv1/2 tag at end of file for extra meta-data */
	if (!gottag)
	{
		uint64_t filesize = f->filesize(f);
#if 0 /* if files are smaller than 4096 bytes, this speed-up is marginal */
		if (_len >= filesize)
		{ /* _buf contains the entire file, so no need to seek/read */
			if ((_len > 32) && (!memcmp (_buf + _len - 32, "APETAGEX", 8)))
			{
				uint32_t tagsize, tagcount, tagsflags;
				if (APEParseHeader((const uint8_t *)_buf + _len - 32, 0, &tagsize, &tagcount, &tagsflags))
				{
					return 1;
				}
				if (_len > tagsize)
				{
					APEParseTags ((const uint8_t *)_buf + _len - tagsize, tagsize - 32, tagcount, m);
				}
				return 1;
			}
			if ((_len > 128) && (!memcmp (_buf + _len - 128, "TAG", 3)))
			{
				parse_ID3v1x (_buf + _len - 128, m, API);
			}
		} else {
#else
		{
#endif
			if (filesize > 32)
			{
				uint8_t buffer[32];
				f->seek_set (f, filesize - 32);
				if ((f->read(f, buffer, 32) == 32) && (!memcmp (buffer, "APETAGEX", 8)))
				{
					uint32_t tagsize, tagcount, tagsflags;
					if (APEParseHeader(buffer, 0, &tagsize, &tagcount, &tagsflags))
					{
						return 1;
					}
					if ((tagsize < 1024*1024) && (tagsize > 32) && (f->filesize(f) > tagsize))
					{
						uint8_t *buffer2 = malloc (tagsize - 32);
						if (buffer2)
						{
							f->seek_set (f, filesize - tagsize);
							if (f->read (f, buffer2, tagsize - 32) == tagsize - 32)
							{
								APEParseTags (buffer2, tagsize - 32, tagcount, m);
							}
							free (buffer2);
						}
					}
					return 1;
				}
			}
			if (filesize > 128)
			{
				char buffer[128];
				f->seek_set (f, filesize - 128);
				if ((f->read(f, buffer, 128) == 128) && (!memcmp (buffer, "TAG", 3)))
				{
					parse_ID3v1x (buffer, m, API);
				}
			}
		}
	}
	f->seek_set (f, 0);
	return 1;
}

static const char *WavPack_description[] =
{
	//                                                                          |
	"WV files are WavPack audio files. This is a loss-less audio compressed file.",
	"Open Cubic Player uses libwavpack for decoding of these.",
	NULL
};

static struct mdbreadinforegstruct wavpackReadInfoReg = {"WV", wavpackReadInfo MDBREADINFOREGSTRUCT_TAIL};

OCP_INTERNAL int wavpack_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("WV");

	mt.integer.i = MODULETYPE("WV");
	API->fsTypeRegister (mt, WavPack_description, "plOpenCP", &wavpackPlayer);

	API->mdbRegisterReadInfo(&wavpackReadInfoReg);

	return errOk;
}

OCP_INTERNAL void wavpack_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("WV");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&wavpackReadInfoReg);
}
