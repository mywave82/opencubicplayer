/* OpenCP Module Player
 * copyright (c) 2022-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Dumping tool for YM files, used for debugging
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

#include "lzh/lzh.h"

struct __attribute__((packed)) pymHeader
{
	char id[4]; /* YM2!, YM3!, YM3b, //YM4!, YM5!, YM6! */
	char mark[8]; /* LeOnArD!, YM5!, YM6! */
	uint32_t nb_frame; /* YM5!, YM6! */
	uint32_t attributes; /* YM5!, YM6! */
	uint16_t nb_drum; /* YM5!, YM6! */
	uint32_t clock; /* YM5!, YM6! */
	uint16_t rate; /* YM5!, YM6! */
	uint32_t loopframe; /* YM5!, YM6! */
	uint16_t skip; /* YM5!, YM6! */
	/* skip bytes of data if YM5!, YM6!
	nb_drums {
		uint32_t size;
		size bytes of data;
	}
	char *song_name;
	char *song_author;
	char *song_comment;
	data
	uint32_t loopframe at EOF-4 for YM3b */
};

struct __attribute__((packed)) pmixHeader
{
	char id[4]; /* MIX1 */
	char mark[8]; /* LeOnArD! */
	uint32_t attribute;
	uint32_t sample_size;
	uint32_t nb_mix_block;
/*
	nb_mix_block {
		uint32_t samplestart;
		uint32_t samplelength;
		uint16_t nbRepeat;
		uint16_t replayFreq;
	}
	char *song_name;
	char *song_author;
	char *song_comment;
	sample_size data;
*/
};

struct __attribute__((packed)) pymtHeader
{
	char id[4]; /* YMT1, YMT2 */
	char mark[8]; /* LeOnArD! */
	uint16_t nb_voices;
	uint16_t player_rate;
	uint32_t music_length;
	uint32_t music_loop;
	uint16_t nb_digidrum;
	uint32_t flags;
/*
	char *music_name;
	char *music_author;
	char *music_comment;
	data */
};

struct __attribute__((packed)) lzhHeader
{
	uint8_t size;
	uint8_t sum;
	char id[5]; /* -lh5- */
	uint32_t packed;
	uint32_t original;
	char reserved[5];
	uint8_t level;
	uint8_t name_length;
};

static char ex_buf[1024*1024*16];

static int Try_LH5(const char *mem, size_t len)
{
	struct lzhHeader *h= (struct lzhHeader *)mem;

	if (len<sizeof(lzhHeader))
	{
		return 0;
	}

	if (strcmp (h->id, "-lh5-", 5))
	{
		return 0;
	}

	printf ("[%sLHA compression%s]\n", FONT_BRIGHT_CYAN, FONT_RESET);

	DumpPrefix (mem, len, 0x00, 1);
	if (h->size)
	{
		printf ("size: %s%d%s ??\n", FONT_BRIGHT_GREEN, h->size, FONT_RESET);
	} else {
		printf ("size: %s%d # must be ZERO%s\n", FONT_BRIGHT_RED, h->size, FONT_RESET);
	}

	DumpPrefix (mem, len, 0x01, 1);
	printf ("checksum: 0x%02x", h->sum);

	DumpPrefix (mem, len, 0x02, 5);
	printf ("id: %s\"%c%c%c%c%c\"%s\n", FONT_BRIGHT_GREEN, h->id[0], h->id[1], h->id[2], h->id[3], h->id[4], FONT_RESET);

	DumpPrefix (mem, len, 0x07, 4);
	printf ("packed: %u (size after compression)\n", (unsigned int)uint32_little(h->packed));

	DumpPrefix (mem, len, 0x0b, 4);
	printf ("original: %u (size before compression)\n", (unsigned int)uint32_little(h->original));

	DumpPrefix (mem, len, 0x0f, 5);
	printf ("Reserved\n");

	DumpPrefix (mem, len, 0x14, 1);
	if (!h->level)
	{
		printf ("level: %s%d%s\n", FONT_BRIGHT_GREEN, h->level, FONT_RESET);
	} else {
		printf ("level: %s%d # must be zero %s\n", FONT_BRIGHT_RED, h->level, FONT_RESET);
	}

	DumpPrefix (mem, len, 0x15, 1);
	printf ("namelength: %d\n", h->namelength);

	if ((h->size==0)||(h->level!=0))
	{
		return -1;
	}

	{
		uint32_t fileSize = uint32_little(h->original);
		uint32_t packedSize = uint32_little(h->packed)-2;

		if (len < (sizeof(*h) + h->name_length + packedSize + 2))
		{
			printf ("%sFile does not contain complete data as specified by packetSize%s\n", FONT_BRIGHT_RED, FONT_RESET);
			return -1;
		}

		{
			CLzhDepacker *pDepacker = new CLzhDepacker;
			const bool bRet = pDepacker->LzUnpack(mem + sizeof(*h) + h->name_length + 2, packedSize, ex_buf, fileSize);
			delete pDepacker;

			if (!bRet)
			{
				printf ("%sDECOMPRESSION FAILED%s\n", FONT_BRIGHT_RED, FONT_RESET);
				return -1;
			}
		}

		ParseYMFile (ex_buf, fileSize);
	}

	return 1;
}

