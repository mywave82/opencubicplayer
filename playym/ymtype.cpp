/* OpenCP Module Player
 * copyright (c) '10-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SIDPlay file type detection routines for the fileselector
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
 *  -kb980717  Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 */

#include "config.h"
#include <string.h>
#include "types.h"
extern "C" {
#include "boot/plinkman.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
}
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

static void ym_strcpy(char *target, int targetsize, const char **source, int *lenleft)
{
	int length;
	int copy;

	if (*lenleft<=0)
		return;

	for (length=0;;length++)
	{
		if (length>=*lenleft)
		{
			if (length<targetsize)
				return;
			break;
		}
		if (!(*source)[length])
		{
			length++;
			break;
		}
	}
	(*lenleft)+=length;
	if (length>targetsize)
		copy=targetsize;
	else
		copy=length;
	strncpy(target, *source, copy); /* This does NOT NUL terminate the string.. expected behaviour */
	(*source)+=length;
}

static int ymReadMemInfo2(struct moduleinfostruct *m, const char *buf, size_t len)
{
	struct pymHeader *YM = (struct pymHeader *)buf;
	struct pymtHeader *YMT = (struct pymtHeader *)buf;
	struct pmixHeader *MIX = (struct pmixHeader *)buf;

	if (len<4)
		return 0;

	if (!strncmp(YM->id, "YM2!", 4))
	{
		m->modtype.integer.i=MODULETYPE("YM");
		m->channels=3;
		strcpy(m->title, "Unknown");
		strcpy(m->composer, "Unknown");
		strcpy(m->comment, "Converted by Leonard.");
		strcpy(m->style, "YM 2 (MADMAX specific)");
		m->playtime=0;
//#warning Need file size to calculate playtime
		return 1;
	}
	if (!strncmp(YM->id, "YM3!", 4))
	{
		m->modtype.integer.i=MODULETYPE("YM");
		m->channels=3;
		strcpy(m->title, "Unknown");
		strcpy(m->composer, "Unknown");
		strcpy(m->comment, "");
		strcpy(m->style, "YM 3 (Standard YM-Atari format)");
		m->playtime=0;
//#warning Need file size to calculate playtime
		return 1;
	}
	if (!strncmp(YM->id, "YM3b", 4))
	{
		m->modtype.integer.i=MODULETYPE("YM");
		m->channels=3;
		strcpy(m->title, "Unknown");
		strcpy(m->composer, "Unknown");
		strcpy(m->comment, "");
		strcpy(m->style, "YM 3b (Standard YM-Atari format + loop information)");
		m->playtime=0;
//#warning Need file size to calculate playtime
		return 1;
	}
	if (!strncmp(YM->id, "YM4!", 4))
	{
		m->modtype.integer.i=MODULETYPE("YM");
		m->channels=3;
		strcpy(m->style, "YM 4 not supported (Extended Atari format)");
		return 0;
	}

	if (len<12)
		return 0;
	if (strncmp(YM->mark, "LeOnArD!", 8))
		return 0;

	if ((!strncmp(YM->id, "YM5!", 4))||(!strncmp(YM->id, "YM6!", 4)))
	{
		m->modtype.integer.i=MODULETYPE("YM");
		m->channels=3;

		strcpy(m->title, "Unknown");
		strcpy(m->composer, "Unknown");
		strcpy(m->comment, "");
		m->playtime=0;
		if (!strncmp(YM->id, "YM5!", 4))
			strcpy(m->style, "YM 5 (Extended YM2149 format, all machines)");
		else
			strcpy(m->style, "YM 6 (Extended YM2149 format, all machines)");

		if (len >= sizeof(*YM))
		{
			uint_fast32_t drumskip = 0;
			int i;
			uint32_t nbFrame = uint32_big(YM->nb_frame);
			uint16_t nbDrum = uint16_big(YM->nb_drum);
			/*uint32_t clock = uint32_big(YM->clock);*/
			uint16_t rate = uint16_big(YM->rate);
			uint16_t skip = uint16_big(YM->skip);
			// TODO, use clock, rate and nbFrame to calculate song length */
			m->playtime = nbFrame/rate;

			for (i=0;i<nbDrum;i++)
			{
				if (len >= (sizeof(*YM) + skip + drumskip + 4))
				{
					uint32_t drumsize = uint32_big(*(uint32_t *)(buf+skip+drumskip));
					if (drumsize>=0x1000000) /* don't overflow on big files */
						drumsize=0xffffff;
					drumskip+=drumsize+4;
				} else {
					drumskip+=4;
					break;
				}
			}
			const char *ptr = buf+skip+drumskip+sizeof(*YM);
			int lenleft = len-skip-drumskip-sizeof(*YM);

			ym_strcpy(m->title, sizeof(m->title), &ptr, &lenleft);
			ym_strcpy(m->composer, sizeof(m->composer), &ptr, &lenleft);
			ym_strcpy(m->comment, sizeof(m->comment), &ptr, &lenleft);

			if (lenleft>=0)
				return 1;
		}
		return 0; /* we wanted more data */
	}

	if (!strncmp(MIX->id, "MIX1", 4))
	{
		m->modtype.integer.i=MODULETYPE("YM");
		m->channels=3;

		strcpy(m->title, "Unknown");
		strcpy(m->composer, "Unknown");
		strcpy(m->comment, "");
		m->playtime=0;
		strcpy(m->style, "MIX1 (Atari Remix digit format)");

		if (len >= sizeof(*MIX))
		{
			uint32_t nbMixBlock = uint32_big(MIX->nb_mix_block);
			if (nbMixBlock>=0x1000000) /* don't overflow on big files */
			    nbMixBlock=0xffffff;
			uint32_t skip = nbMixBlock * 12;
			const char *ptr = buf+skip+sizeof(*MIX);
			int lenleft = len-skip-sizeof(*MIX);

			ym_strcpy(m->title, sizeof(m->title), &ptr, &lenleft);
			ym_strcpy(m->composer, sizeof(m->composer), &ptr, &lenleft);
			ym_strcpy(m->comment, sizeof(m->comment), &ptr, &lenleft);

			if (lenleft>=0)
				return 1;
		}
		return 0;
	}

	if ((!strncmp(YMT->id, "YMT1", 4))||(!strncmp(YMT->id, "YMT2", 4)))
	{
		m->modtype.integer.i=MODULETYPE("YM");
		m->channels=3;

		strcpy(m->title, "Unknown");
		strcpy(m->composer, "Unknown");
		strcpy(m->comment, "");
		m->playtime=0;
		if (!strncmp(YMT->id, "YMT1", 4))
			strcpy(m->style, "YM-T1 (YM-Tracker)");
		else
			strcpy(m->style, "YM-T2 (YM-Tracker)");

		if (len >= sizeof(*YMT))
		{
		/* TODO, time
			uint16_t Rate = uint16_big(YMT->rate);
		*/
			const char *ptr = buf+sizeof(*YMT);
			int lenleft = len-sizeof(*YMT);

			ym_strcpy(m->title, sizeof(m->title), &ptr, &lenleft);
			ym_strcpy(m->composer, sizeof(m->composer), &ptr, &lenleft);
			ym_strcpy(m->comment, sizeof(m->comment), &ptr, &lenleft);

			if (lenleft>=0)
				return 1;
		}
		return 0;
	}

	return 0;
}


static int ymReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len)
{
#ifdef HAVE_LZH
	uint32_t fileSize;
	uint32_t packedSize;
	char ex_buf[8192];
	struct lzhHeader *h= (struct lzhHeader *)buf;

	if (len<sizeof(lzhHeader))
		return 0; /* no point testing for valid formats at all if we can't even fit this header in */

	if ((h->size==0)||(strncmp(h->id, "-lh5-", 5))||(h->level!=0))
		return ymReadMemInfo2(m, buf, len);

	fileSize = uint32_little(h->original);
	if (fileSize>sizeof(ex_buf))
		fileSize=sizeof(ex_buf);

	packedSize = uint32_little(h->packed)-2;
	if (packedSize > (len+sizeof(*h)+h->name_length+2))
		packedSize = len+sizeof(*h)+h->name_length+2;

	memset(ex_buf, 0, fileSize);
	{
		CLzhDepacker *pDepacker = new CLzhDepacker;
		pDepacker->LzUnpack(buf+sizeof(*h)+h->name_length+2,packedSize,ex_buf,fileSize);
		delete pDepacker;
	}
	return ymReadMemInfo2(m, ex_buf, fileSize);
#else
	return ymReadMemInfo2(m, buf, len);
#endif
}

const char *YM_description[] =
{
	//                                                                          |
	"YM files as music, primary from Atari systems. Atari machines features a",
	"three channel synthesizer IC YM2149 (and very similiar to the wider used",
	"AY-3-8912). YM files contains register value recorded at the screen refresh",
	"rate, so only the IC needs to be simulated. Open Cubic Player uses",
	"stsoundlib for playback.",
	NULL
};

struct interfaceparameters YM_p =
{
	"playym", "ymPlayer",
	0, 0
};

static void ymEvent(int event)
{
	switch (event)
	{
		case mdbEvInit:
		{
			struct moduletype mt;

			fsRegisterExt("YM");
			mt.integer.i = MODULETYPE("YM");
			fsTypeRegister (mt, YM_description, "OpenCP", &YM_p);
		}
	}
}
static struct mdbreadinforegstruct ymReadInfoReg = {"YM", ymReadInfo, ymEvent MDBREADINFOREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	mdbRegisterReadInfo(&ymReadInfoReg);
}

static void __attribute__((destructor))done(void)
{
	mdbUnregisterReadInfo(&ymReadInfoReg);
}

extern "C" {
	const char *dllinfo = "";
	struct linkinfostruct dllextinfo =
	{
		"ymtype" /* name */,
		"OpenCP YM Detection (c) 2010-2021 Stian Skjelstad" /* desc */,
		DLLVERSION /* ver */
	};
}
