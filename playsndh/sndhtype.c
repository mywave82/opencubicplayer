/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * .sndh file type detection routines for the file selector
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
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "sndhtype.h"
#include "boot/plinkman.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"

#include "psgplay-git/lib/toslibc/include/toslibc/unicode/atari.h"
#ifdef SNDH_MUSTPROVIDE_ICE
#include "psgplay-git/include/ice/ice.h"
#endif


#define SNDH_SUPPORT_v1x 1

/* Atari ST is not included by default in the GNU iconv, but it must be configured to include extended codepages */

#ifdef PLAYSNDH_DEBUG
static void fput_unicode (uint32_t CodePoint, FILE *f)
{
	unsigned char buf[8];
	memset (buf, 0, sizeof (buf));
	if (utf32_to_utf8_length (CodePoint) >= sizeof (buf) - 1)
		return;
	utf32_to_utf8 (CodePoint, buf, sizeof (buf) - 1);
	fputs ((char *)buf, f);
}

static void fputs_unicode (const unsigned char *str, FILE *f)
{
	while (*str)
	{
		fput_unicode (charset_atari_st_to_utf32 (*str, 0), f);
		str++;
	}
}
#endif

static int AtariST_to_UTF8_Dynamic (char **target, const unsigned char *src)
{
	int targetlength = 1;
	const unsigned char *iter;
	char *targetnext;
	int retval = 0;

	for (iter = src; *iter; iter++)
	{
		uint32_t CodePoint = charset_atari_st_to_utf32 (*src, 0);
		targetlength += utf32_to_utf8_length (CodePoint);
	}
	*target = malloc (targetlength);
	if (!*target)
	{
		fprintf (stderr, "AtariST_to_UTF8_Dynamic: malloc() failed\n");
		return 0;
	}
	targetnext = *target;
	while (*src)
	{
		uint32_t CodePoint = charset_atari_st_to_utf32 (*src, 0);
		unsigned int len = utf32_to_utf8_length (CodePoint);
		utf32_to_utf8 (CodePoint, (unsigned char *)targetnext, 8);
		targetnext += len;
		retval += len;
		src++;
	}
	*targetnext = 0;
	return retval;
}

static int DetectStringDynamic (const char *Field, const unsigned char **ptr, int *remaining, int even, const int taglength, int subtunes, void *_target)
{
	char **target = _target;
	unsigned char *termination;
#ifdef PLAYSNDH_DEBUG
	if (even) fprintf (stderr, "[SNDH] DetectStringDynamic: Not needed even padding\n");
#endif
	termination = memchr ((*ptr) + taglength, 0, (*remaining) - taglength);
	if (!termination)
	{
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] DetectStringDynamic: %.*s %s NOT TERMINATED\n", taglength, *ptr, Field);
#endif
		return -1;
	}
#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "[SNDH] %s: \"", Field);
	fputs_unicode ((*ptr) + taglength, stderr);
	fputs ("\"\n", stderr);
#endif
	if (target)
	{
		if (*target)
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectStringDynamic: %.*s %s Already present\n", taglength, *ptr, Field);
#endif
		} else {
			AtariST_to_UTF8_Dynamic (target, (*ptr) + taglength);
		}
	}
	termination++;
	*remaining -= termination - (*ptr);
	*ptr = termination;
	return 0;
}

static int DetectNumber (const char *Field, const unsigned char **ptr, int *remaining, int even, int taglength, int subtunes, void *_target)
{
	uint16_t *target = _target;
#define NUMBER_MAX_LENGTH 4
	int i;
	int value;

#ifdef PLAYSNDH_DEBUG
	if (even)
	{
		fprintf (stderr, "[SNDH] (DetectNumber: Not needed even padding)\n");
	}
#endif

	if (((*remaining) >= (taglength + 1)) && (!(*ptr)[taglength]))
	{
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] DetectNumber: %s tag contains no data\n", Field);
#endif
		*remaining -= taglength + 1;
		*ptr += taglength + 1;
		return 0;
	}

	for (i=1; i <= NUMBER_MAX_LENGTH; i++)
	{
		if ((*remaining) < (taglength + i + 1))
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectNumber: %s tag with remaining data < taglength + %d\n", Field, i);
#endif
			return -1;
		}
		if (!isdigit (((*ptr)[taglength + i - 1])))
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectNumber: %s tag, text at position %d (%02x %c) is not a digit\n", Field, i, ((*ptr)[taglength + i - 1]), ((*ptr)[taglength + i - 1]));
#endif
			return -1;
		}
		if (!(*ptr)[taglength + i])
		{
			break;
		}
	}
	if (i > NUMBER_MAX_LENGTH)
	{
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] DetectNumber: %s tag did not find termination within the limit of %d digits\n", Field, NUMBER_MAX_LENGTH);
#endif
		return -1;
	}

	value = atoi ((const char *)(*ptr) + taglength);

	if (target)
	{
		if (*target)
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectNumber: %s already present\n", Field);
			return -1;
#endif
		}
		*target = value;
	}

#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "[SNDH] %s: %d\n", Field, value);
#endif
	*remaining -= taglength + i + 1;
	*ptr += taglength + i + 1;
	return 0;
}

static int DetectNumbers16Dynamic (const char *Field, const unsigned char **ptr, int *remaining, int even, int taglength, int subtunes, void *_target)
{
	int i;
	uint16_t **target = _target;

#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "[SNDH] %s:\n", Field);
#endif

	if (target)
	{
		*target = calloc (subtunes, sizeof (**target));
		if (!*target)
		{
			fprintf (stderr, "DetectNumbersDynamic: calloc() failed\n");
			target = 0;
		}
	}

	for (i = 0; i < subtunes; i++)
	{
		if ((*remaining) < (taglength + (i * 2)))
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectNumbersDynamic: Ran out of data\n");
#endif
			return -1;
		}
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "  %2d: %6"PRIu16"\n", i + 1,
			((uint16_t)((*ptr)[taglength + (i << 1) + 0]) <<  8) |
			            (*ptr)[taglength + (i << 1) + 1]);
#endif
		if (target)
		{
			(*target)[i] = (((uint32_t)((*ptr)[taglength + (i << 1) + 0]) <<  8) |
			                            (*ptr)[taglength + (i << 1) + 1]);
		}
	}

	*remaining -= taglength + subtunes * 2;
	*ptr += taglength + subtunes * 2;
	return 0;
}


static int DetectNumbers32Dynamic (const char *Field, const unsigned char **ptr, int *remaining, int even, int taglength, int subtunes, void *_target)
{
	int i;
	uint32_t **target = _target;

#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "[SNDH] %s:\n", Field);
#endif

	if (target)
	{
		*target = calloc (subtunes, sizeof (**target));
		if (!*target)
		{
			fprintf (stderr, "DetectNumbersDynamic: calloc() failed\n");
			target = 0;
		}
	}

	for (i = 0; i < subtunes; i++)
	{
		if ((*remaining) < (taglength + (i * 4)))
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectNumbersDynamic: Ran out of data\n");
#endif
			return -1;
		}
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "  %2d: %6"PRIu32"\n", i + 1,
			((uint32_t)((*ptr)[taglength + (i << 2) + 0]) << 24) |
			((uint32_t)((*ptr)[taglength + (i << 2) + 1]) << 16) |
			((uint32_t)((*ptr)[taglength + (i << 2) + 2]) <<  8) |
			            (*ptr)[taglength + (i << 2) + 3]);
#endif
		if (target)
		{
			(*target)[i] = (((uint32_t)((*ptr)[taglength + (i << 2) + 0]) << 24) |
			                ((uint32_t)((*ptr)[taglength + (i << 2) + 1]) << 16) |
			                ((uint32_t)((*ptr)[taglength + (i << 2) + 2]) <<  8) |
			                            (*ptr)[taglength + (i << 2) + 3]);
		}
	}

	*remaining -= taglength + subtunes * 4;
	*ptr += taglength + subtunes * 4;
	return 0;
}

static int DetectStringsDynamic (const char *Field, const unsigned char **ptr, int *remaining, int even, int taglength, int subtunes, void *_target)
{
	char ***target = _target;
	uint32_t offset = 0;
	unsigned char *termination = 0;
	unsigned char *termination_max = 0;
	int i;

#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "[SNDH] %s:\n", Field);
#endif

	if (!subtunes) {
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] DetectStringsDynamic: No SubTunes detected yet\n");
#endif
		*remaining -= taglength;
		*ptr += taglength;
		return 0;
	}

	if (target)
	{
		if (*target)
		{
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] DetectStringsDynamic: tag already encountered\n");
#endif
		target = 0;
		}
	}
	if (target)
	{
		*target = calloc (subtunes, sizeof (char *));
	}

	for (i = 0; i < subtunes; i++)
	{
		if ((*remaining) < (taglength + (i * 2)))
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectStringsDynamic: Ran out of data (1)\n");
#endif
			return -1;
		}
		offset = ((unsigned int)((*ptr)[taglength + (i << 1) + 0]) << 8) | (*ptr)[taglength + (i << 1) + 1];
		if (offset >= (*remaining))
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectStringsDynamic: Ran out of data (2)\n");
#endif
			return -1;
		}
		termination = memchr ((*ptr) + offset, 0, *remaining - offset);
		if (!termination)
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] DetectStringsDynamic: Ran out of data (3)\n");
#endif
			return -1;
		}
		if (termination > termination_max)
		{
			termination_max = termination;
		}
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "  %2d: %5d \"", i + 1, ((unsigned int)((*ptr)[taglength + (i << 1) + 0]) << 8) | (*ptr)[taglength + (i << 1) + 1]);
		fputs_unicode ((*ptr) + offset, stderr);
		fputs ("\"\n", stderr);
#endif
		if (target)
		{
			AtariST_to_UTF8_Dynamic ((*target) + i, (*ptr) + offset);
		}
	}
	termination_max++;
	*remaining -= termination_max - (*ptr);
	*ptr = termination_max;
	return 0;
}

#define SNDH_VERSION_UNKNOWN 0
#define SNDH_VERSION_v1x     1
#define SNDH_VERSION_v2p0    2
#define SNDH_VERSION_v2p1    3
#define SNDH_VERSION_v2p2    4

static int sndhReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *data, size_t len, const struct mdbReadInfoAPI_t *API)
{
	const unsigned char *ptr = (const unsigned char *)data;
	int remaining = len;

	if (remaining < 20)
	{
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] sndhReadInfo: File to small to contain a complete header\n");
#endif
		return 0;
	}
	if ((ptr[12] != 'S') ||
	    (ptr[13] != 'N') ||
	    (ptr[14] != 'D') ||
	    (ptr[15] != 'H'))
	{
#ifdef SNDH_MUSTPROVIDE_ICE
		if ((ptr[0] == 'I') &&
		    (ptr[1] == 'C') &&
		    (ptr[2] == 'E') &&
		    (ptr[3] == '!') && f /* avoid recursive */)
		{
			char *rawdata = 0;
			uint64_t filesize = f->filesize (f);
			uint64_t fill;

			size_t uncompressedsize;
			ssize_t realuncompressedsize;
			char *uncompresseddata = 0;
			int retval = 0;
			if (filesize < 2*1024*1024)
			{
				rawdata = malloc (filesize);
				if (!rawdata)
				{
					return 0;
				}
				f->seek_set (f, 0);
				fill = f->read (f, rawdata, filesize);
				if (fill != filesize)
				{
					goto iceout;
				}
				uncompressedsize = ice_decrunched_size (rawdata, filesize);
				if ((uncompressedsize < 20) || (uncompressedsize > 2 * 1024 * 1024))
				{
					goto iceout;
				}
				uncompresseddata = malloc (uncompressedsize);
				if (!uncompresseddata)
				{
					goto iceout;
				}
				realuncompressedsize = ice_decrunch (uncompresseddata, rawdata, filesize);
				if ((realuncompressedsize < 20) || (realuncompressedsize > 2 * 1024 * 1024))
				{
					goto iceout;
				}
				retval = sndhReadInfo (m, 0, uncompresseddata, realuncompressedsize, API);
iceout:
				f->seek_set (f, 0);
				free (uncompresseddata);
				free (rawdata);
				return retval;
			}
		}
#endif
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] sndhReadInfo: Signature missing\n");
#endif
		return 0;
	}

	struct sndhMeta_t *meta = sndhReadInfos (ptr, remaining);
	if (!meta)
	{
		return 0;
	}

	m->title[0] = 0;
	m->composer[0] = 0;
	m->artist[0] = 0;
	m->style[0] = 0;
	m->comment[0] = 0;
	m->album[0] = 0;
	m->channels = 1;
	m->date = 0;
	m->playtime = 0;
	m->modtype.integer.i = MODULETYPE("SNDH");

	if (meta->titles)
	{
		int i;
		int fill = 0;
		for (i = 0; (i < meta->subtunes) && (fill < sizeof (m->title)) && meta->titles[i]; i++)
		{
			snprintf (m->title + fill, sizeof (m->title) - fill, "%s%s", i ? ", " : "", meta->titles[i]);
			fill += strlen (m->title + fill);
		}
		if (meta->title && strcmp (meta->title, "n/a"))
		{
			snprintf (m->album, sizeof (m->album), "%s", meta->title);
		}
	} else if (meta->title && strcmp (meta->title, "n/a"))
	{
		snprintf (m->title, sizeof (m->title), "%s", meta->title);
	}

	if (meta->composer)
	{
		snprintf (m->composer, sizeof (m->composer), "%s", meta->composer);
	}

	if (meta->year)
	{
		m->date = (uint_fast32_t)(meta->year) << 16;
	}
	if (meta->frames)
	{
		int i;
		uint64_t frames = 0;
		for (i = 0; i < meta->subtunes; i++)
		{
			frames += meta->frames[i];
		}
		m->playtime = frames / 50;
	} else if (meta->times)
	{
		int i;
		uint64_t times = 0;
		for (i = 0; i < meta->subtunes; i++)
		{
			times += meta->times[i];
		}
		m->playtime = times;
	}

	char SubTunesText[20];
	if (meta->subtunes > 1)
	{
		snprintf (SubTunesText, sizeof (SubTunesText), ", %d sub-tunes", meta->subtunes);
	} else {
		SubTunesText[0] = 0;
	}

	switch (meta->fileversion)
	{
		default:
		case SNDH_VERSION_UNKNOWN:
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] Version: Unknown\n");
#endif
			break;
		case SNDH_VERSION_v1x:
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] Version: 1.x\n");
#endif
			snprintf (m->comment, sizeof (m->comment), "SNDH v1.x%s", SubTunesText);
			break;
		case SNDH_VERSION_v2p0:
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] Version: 2.0\n");
#endif
			snprintf (m->comment, sizeof (m->comment), "SNDH v2.0%s", SubTunesText);
			break;
		case SNDH_VERSION_v2p1:
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] Version: 2.1\n");
#endif
			snprintf (m->comment, sizeof (m->comment), "SNDH v2.1%s", SubTunesText);
			break;
		case SNDH_VERSION_v2p2:
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] Version: 2.2\n");
#endif
			snprintf (m->comment, sizeof (m->comment), "SNDH v2.2%s", SubTunesText);
			break;
	}

	sndhMetaFree (meta);
	return 1;
}

void sndhMetaFree (struct sndhMeta_t *meta)
{
	unsigned int i;
	if (!meta)
	{
		return;
	}
	for (i = 0; i < meta->subtunes; i++)
	{
		if (meta->titles) free (meta->titles[i]);
		if (meta->flags) free (meta->flags[i]);
	}
	free (meta->title);
	free (meta->titles);
	free (meta->ripper);
	free (meta->converter);
	free (meta->flag);
	free (meta->flags);
	free (meta->frames);
	free (meta->times);
	free (meta);
}

struct Element
{
	const char *tag;
	const char *name;
	int offset;
	int (*Detect) (const char *Field, const unsigned char **ptr, int *remaining, int even, int taglength, int subtunes, void *Target);
	int ForceSingleTrackIfNotSet;
	int fileversion;
};

struct sndhMeta_t *sndhReadInfos(const uint8_t *data, size_t len)
{
	const struct Element Elements[] =
	{
		{"TITL",  "Title",            offsetof(struct sndhMeta_t, title),          DetectStringDynamic,  0, SNDH_VERSION_UNKNOWN},
		{"!#SN",  "Sub tune names",   offsetof(struct sndhMeta_t, titles),         DetectStringsDynamic, 0, SNDH_VERSION_v2p1},
		{"COMM",  "Composer",         offsetof(struct sndhMeta_t, composer),       DetectStringDynamic,  0, SNDH_VERSION_UNKNOWN},
		{"RIPP",  "Ripper",           offsetof(struct sndhMeta_t, ripper),         DetectStringDynamic,  0, SNDH_VERSION_UNKNOWN},
		{"CONV",  "Converter",        offsetof(struct sndhMeta_t, converter),      DetectStringDynamic,  0, SNDH_VERSION_UNKNOWN},
		{"##",    "SubTunes",         offsetof(struct sndhMeta_t, subtunes),       DetectNumber,         0, SNDH_VERSION_UNKNOWN},
		{"TA",    "Timer A",          offsetof(struct sndhMeta_t, timer_a),        DetectNumber,         0, SNDH_VERSION_UNKNOWN},
		{"TB",    "Timer B",          offsetof(struct sndhMeta_t, timer_a),        DetectNumber,         0, SNDH_VERSION_UNKNOWN},
		{"TC",    "Timer C",          offsetof(struct sndhMeta_t, timer_a),        DetectNumber,         0, SNDH_VERSION_UNKNOWN},
		{"TD",    "Timer D",          offsetof(struct sndhMeta_t, timer_a),        DetectNumber,         0, SNDH_VERSION_UNKNOWN},
		{"!V",    "VBL",              offsetof(struct sndhMeta_t, vbl),            DetectNumber,         0, SNDH_VERSION_UNKNOWN},
		{"!#",    "Default Sub Tune", offsetof(struct sndhMeta_t, defaultsubtune), DetectNumber,         0, SNDH_VERSION_v2p1},
		{"YEAR",  "Year",             offsetof(struct sndhMeta_t, year),           DetectNumber,         0, SNDH_VERSION_v2p0},
		{"TIME",  "Time",             offsetof(struct sndhMeta_t, times),          DetectNumbers16Dynamic, 1, SNDH_VERSION_v2p0},
		{"FRMS",  "Frames",           offsetof(struct sndhMeta_t, frames),         DetectNumbers32Dynamic, 1, SNDH_VERSION_v2p2},
		{"FLAG~", "Flag",             offsetof(struct sndhMeta_t, flag),           DetectStringDynamic,  0, SNDH_VERSION_v2p1},
		{"FLAG",  "Flags",            offsetof(struct sndhMeta_t, flags),          DetectStringsDynamic, 0, SNDH_VERSION_v2p2},
/*		{"HDNS",  "Termination",      0,                                           DetectHDNS,           0, SNDH_VERSION_v2p0},*/
	};

	const uint8_t *ptr = data;
	int remaining = len;

	int even = 0;
	uint16_t EXIT;

	if (remaining < 20)
	{
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] sndhReadInfo: File to small to contain a complete header\n");
#endif
		return 0;
	}
	if ((ptr[12] != 'S') ||
	    (ptr[13] != 'N') ||
	    (ptr[14] != 'D') ||
	    (ptr[15] != 'H'))
	{
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] sndhReadInfos: Signature missing\n");
#endif
		return 0;
	}

	struct sndhMeta_t *target = calloc (1, sizeof (*target));

#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "[SNDH] INIT=$%02x%02x EXIT=$%02x%02x PLAY=$%02x%02x\n", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
#endif
	EXIT = ((uint16_t)(ptr[2]) << 8) | ptr[3];
	ptr += 16;
	remaining -= 16;
	while (remaining >= 4)
	{
		int e;
		if (ptr[0] == 0)
		{
			if (even)
			{
#ifdef PLAYSNDH_DEBUG
				fprintf (stderr, "[SNDH] sndhReadInfos: Double even marker\n");
#endif
				/* We should give up here..... */
			}
			even++;
			ptr++;
			remaining--;
			continue;
		}
		for (e=0; e < ( sizeof (Elements) / sizeof (Elements[0]) ); e++ )
		{
			int taglength = strlen (Elements[e].tag);
			if (!memcmp (ptr, Elements[e].tag, taglength))
			{
				if (Elements[e].ForceSingleTrackIfNotSet && (target->subtunes == 0))
				{
					target->subtunes = 1;
				}
				if (Elements[e].Detect (Elements[e].name, &ptr, &remaining, even, taglength, target->subtunes, (void *)target + Elements[e].offset))
				{
					goto bailout;
				}
				if (Elements[e].fileversion > target->fileversion)
				{
					target->fileversion = Elements[e].fileversion;
				}
#ifdef SNDH_SUPPORT_v1x
				if ( (ptr == (const unsigned char *)(data + EXIT + 0)) ||
				     (ptr == (const unsigned char *)(data + EXIT + 1)) ||
				     (ptr == (const unsigned char *)(data + EXIT + 2)))
				{
					/* fprintf (stderr, "OLD FILE DETECTED, exit gracefully\n"); */
					target->fileversion = SNDH_VERSION_v1x;
					goto TagDone;
				}
#endif
				even = 0;
				break;
			}
		}
		if (e < ( sizeof (Elements) / sizeof (Elements[0]) ))
		{
			continue;
		}
		if (!memcmp (ptr, "HDNS", 4))
		{
#ifdef PLAYSNDH_DEBUG
			fprintf (stderr, "[SNDH] HDNS terminator\n");
#endif
			if (target->fileversion < SNDH_VERSION_v2p0)
			{
				target->fileversion = SNDH_VERSION_v2p0;
			}
			ptr += 4; remaining -= 4;
			goto TagDone;
		}
#ifdef PLAYSNDH_DEBUG
		fprintf (stderr, "[SNDH] sndhReadInfos: Unknown tag: 0x%02x 0x%02x 0x%02x 0x%02x %c%c%c%c\n", ptr[0], ptr[1], ptr[2], ptr[3], isalnum(ptr[0]) ? ptr[0] : '?', isalnum(ptr[1]) ? ptr[1] : '?', isalnum(ptr[2]) ? ptr[2] : '?', isalnum(ptr[3]) ? ptr[3] : '?');
#endif
		return target;
	}
#ifdef PLAYSNDH_DEBUG
	fprintf (stderr, "[SNDH] sndhReadInfos: Ran out of data\n");
#endif
	return target;
bailout:
TagDone:
	if (((ptr - data) > 20))
	{
		return target;
	}
	sndhMetaFree (target);
	return 0;
}


static struct mdbreadinforegstruct sndhReadInfoReg = {"SNDH", sndhReadInfo MDBREADINFOREGSTRUCT_TAIL};

static const char *SNDH_description[] =
{
	//                                                                          |
	"SNDH files are executable code that runs a virtual M68K machine with a",
	"virtual YM2149 sound IC. This IC a 3 channel programmable sound generator",
	"(PSG) that can generate sawtooth and pulse-wave (square) sounds. Open Cubic",
	"Player internal uses psgplay for playback.",
	NULL
};

OCP_INTERNAL int sndh_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt("sndh");

	mt.integer.i = MODULETYPE("SNDH");
	API->fsTypeRegister (mt, SNDH_description, "plOpenCP", &sndhPlayer);

	API->mdbRegisterReadInfo(&sndhReadInfoReg);

	return errOk;
}

OCP_INTERNAL void sndh_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("SNDH");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&sndhReadInfoReg);
}
