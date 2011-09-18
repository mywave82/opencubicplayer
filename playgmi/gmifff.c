/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay FFF format handler (reads GUS patches etc)
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
 *  -sss050411 Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "gmipat.h"
#include "gmiplay.h"
#include "boot/psetting.h"
#include "dev/mcp.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

/*
 * u-law (mu-law) decoding table (shamelessly stolen from SOX)
 */

static int ulaw_exp_table[] =
{
	-32124,-31100,-30076,-29052,-28028,-27004,-25980,-24956,
	-23932,-22908,-21884,-20860,-19836,-18812,-17788,-16764,
	-15996,-15484,-14972,-14460,-13948,-13436,-12924,-12412,
	-11900,-11388,-10876,-10364, -9852, -9340, -8828, -8316,
	-7932, -7676, -7420, -7164, -6908, -6652, -6396, -6140,
	-5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092,
	-3900, -3772, -3644, -3516, -3388, -3260, -3132, -3004,
	-2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980,
	-1884, -1820, -1756, -1692, -1628, -1564, -1500, -1436,
	-1372, -1308, -1244, -1180, -1116, -1052,  -988,  -924,
	-876,  -844,  -812,  -780,  -748,  -716,  -684,  -652,
	-620,  -588,  -556,  -524,  -492,  -460,  -428,  -396,
	-372,  -356,  -340,  -324,  -308,  -292,  -276,  -260,
	-244,  -228,  -212,  -196,  -180,  -164,  -148,  -132,
	-120,  -112,  -104,   -96,   -88,   -80,   -72,   -64,
	-56,   -48,   -40,   -32,   -24,   -16,    -8,     0,
	32124, 31100, 30076, 29052, 28028, 27004, 25980, 24956,
	23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764,
	15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412,
	11900, 11388, 10876, 10364,  9852,  9340,  8828,  8316,
	7932,  7676,  7420,  7164,  6908,  6652,  6396,  6140,
	5884,  5628,  5372,  5116,  4860,  4604,  4348,  4092,
	3900,  3772,  3644,  3516,  3388,  3260,  3132,  3004,
	2876,  2748,  2620,  2492,  2364,  2236,  2108,  1980,
	1884,  1820,  1756,  1692,  1628,  1564,  1500,  1436,
	1372,  1308,  1244,  1180,  1116,  1052,   988,   924,
	876,   844,   812,   780,   748,   716,   684,   652,
	620,   588,   556,   524,   492,   460,   428,   396,
	372,   356,   340,   324,   308,   292,   276,   260,
	244,   228,   212,   196,   180,   164,   148,   132,
	120,   112,   104,    96,    88,    80,    72,    64,
	56,    48,    40,    32,    24,    16,     8,     0
};


static inline int ulaw2linear(uint8_t u_val)
{
	return ulaw_exp_table[u_val];
}

struct __attribute__((packed)) FFF_CHUNK_HEADER
{
	char                        id[4];
	uint32_t                    size;
};
static inline void FFF_CHUNK_HEADER_endian(struct FFF_CHUNK_HEADER *a) {
	a->size = uint32_little(a->size);
}

union __attribute__((packed)) FFF_ID
{
	struct
	{
		uint16_t            maj_id,
				    min_id;
	} version;

	uint32_t                    id;
	void                       *ptr;
};

struct __attribute__((packed)) FFF_ENVELOPE_POINT
{
	uint16_t                    next;

	union
	{
		uint16_t            rate;
		uint16_t            time;
	} data;
};
static inline void FFF_ENVELOPE_POINT_endian(struct FFF_ENVELOPE_POINT *a, int i) {
	int j;
	for (j=0;j<i;j++)
	{
		a[j].next = uint16_little (a[j].next);
		a[j].data.rate = uint16_little (a[j].data.rate);
	}
}

struct __attribute__((packed)) FFF_LFO
{
	uint16_t                    freq;
	int16_t                     depth;
	int16_t                     sweep;
	uint8_t                     shape;         /* no ryg, a byte is NOT an uint8 ;) (fd)... it is now (sss) */
	uint8_t                     delay;         /* a normal rounding error, nothing more :) (ryg) */
};
static inline void FFF_LFO_endian(struct FFF_LFO *a){
	a->freq = uint16_little (a->freq);
	a->depth = int16_little (a->depth);
	a->sweep = int16_little (a->sweep);
}

struct __attribute__((packed)) FFF_ENVELOPE_RECORD
{
	int16_t                     nattack;
	int16_t                     nrelease;
	uint16_t                    sustain_offset;
	uint16_t                    sustain_rate;
	uint16_t                    release_rate;
	uint8_t                     hirange;
	uint8_t                     d0;
	struct FFF_ENVELOPE_POINT  *attack_points;
	struct FFF_ENVELOPE_POINT  *release_points;
};

struct __attribute__((packed)) FFF_ENVP_CHUNK
{
	union FFF_ID                id;
	uint8_t                     num_envelopes;
	uint8_t                     flags; /* bit0=retrigger, rest unused/unimportant */
	uint8_t                     mode;
	uint8_t                     indtype;
	struct FFF_ENVELOPE_RECORD *records;
};

struct __attribute__((packed)) FFF_PROG_CHUNK
{
	union FFF_ID                id;
	union FFF_ID                version;
};

struct __attribute__((packed)) FFF_WAVE_CHUNK
{
	union FFF_ID                id;
	uint32_t                    size;
	uint32_t                    start;
	uint32_t                    loopstart;
	uint32_t                    loopend;
	uint32_t                    m_start;
	uint32_t                    sample_ratio;
	uint8_t                     attenuation;
	uint8_t                     low_note;
	uint8_t                     high_note;
	uint8_t                     format;
	uint8_t                     m_format;
	union FFF_ID                data_id;
};

struct __attribute__((packed)) FFF_LAYR_CHUNK
{
	union FFF_ID           id;
	uint8_t                nwaves;
	uint8_t                flags;
	uint8_t                high_range;
	uint8_t                low_range;
	uint8_t                pan;
	uint8_t                pan_freq_scale;
	struct FFF_LFO         tremolo;
	struct FFF_LFO         vibrato;
	uint8_t                velocity_mode;
	uint8_t                attenuation;
	int16_t                freq_scale;
	uint8_t                freq_center;
	uint8_t                layer_event;
	union FFF_ID           penv;
	union FFF_ID           venv;
	struct FFF_WAVE_CHUNK *waves;
};

struct __attribute__((packed)) FFF_PTCH_CHUNK
{
	union FFF_ID           id;
	int16_t                nlayers;
	uint8_t                layer_mode;
	uint8_t                excl_mode;
	int16_t                excl_group;
	uint8_t                effect1;
	uint8_t                effect1_depth;
	uint8_t                effect2;
	uint8_t                effect2_depth;
	uint8_t                bank;
	uint8_t                program;
	struct FFF_LAYR_CHUNK *iw_layer;
};

struct __attribute__((packed)) FFF_DATA_CHUNK
{
	union FFF_ID            id;
	char                    filename[256];
};

struct FFF_ENVP_LIST
{
	struct FFF_ENVP_CHUNK      *chunk;
	struct FFF_ENVP_LIST       *next;
};

struct FFF_PTCH_LIST
{
	struct FFF_PTCH_CHUNK      *chunk;
	struct FFF_PTCH_LIST       *next;
};

struct FFF_DATA_LIST
{
	struct FFF_DATA_CHUNK      *chunk;
	struct FFF_DATA_LIST       *next;
};


static struct FFF_ENVP_LIST *envp_list=0;
static struct FFF_PTCH_LIST *ptch_list=0;
static struct FFF_DATA_LIST *data_list=0;

static void *getENVP(uint32_t id)
{
	struct FFF_ENVP_LIST *l=envp_list;

	while (l)
	{
		if (l->chunk->id.id==id)
			return l->chunk;
		l=l->next;
	};

	return 0;
}

static void *getDATA(uint32_t id)
{
	struct FFF_DATA_LIST *l=data_list;

	while (l)
	{
		if (l->chunk->id.id==id)
			return l->chunk;
		l=l->next;
	};

	return 0;
}

static int loadFFF(FILE *file)
{
	struct FFF_CHUNK_HEADER  hd;
	int                      i, j, matched;
	struct FFF_PTCH_LIST *l;

	/* pass one: read the .FFF file */
	if (fread(&hd, sizeof(hd), 1, file)!=1)
	{
		fprintf(stderr, __FILE__ ": fread() failed #1\n");
		return 0;
	}
	FFF_CHUNK_HEADER_endian (&hd);

	if (memcmp(hd.id, "FFFF", 4))
	{
		fprintf(stderr, __FILE__ ": invalid header\n");
		return 0;
	}

	if (fread(&hd, sizeof(hd), 1, file)!=1)
	{
		fprintf(stderr, __FILE__ ": fread() failed #2\n");
		return 0;
	}
	FFF_CHUNK_HEADER_endian (&hd);

	while (1) /*(strncmp(hd.id, "FFFF", 4)) ryg, i don't understand your code... (fd)  me too :) (ryg) */
	{
		matched=0;

#ifdef DEBUG
		fprintf(stderr, "[FFF] got %c%c%c%c-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif

		if (!strncmp(hd.id, "ENVP", 4))
		{
			struct FFF_ENVP_LIST       *l;
			struct FFF_ENVELOPE_RECORD *lcr;

			matched=1;

			l=calloc(sizeof(struct FFF_ENVP_LIST), 1);
			l->chunk=calloc(sizeof(struct FFF_ENVP_CHUNK), 1);
			l->next=envp_list;
			envp_list=l;

			if (fread(&l->chunk->id.id,         sizeof(uint32_t), 1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #3\n"); return 0;}
			l->chunk->id.id = uint32_little (l->chunk->id.id);
			if (fread(&l->chunk->num_envelopes, sizeof(uint8_t),  1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #4\n"); return 0;}
			if (fread(&l->chunk->flags,         sizeof(uint8_t),  1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #5\n"); return 0;}
			if (fread(&l->chunk->mode,          sizeof(uint8_t),  1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #6\n"); return 0;}
			if (fread(&l->chunk->indtype,       sizeof(uint8_t),  1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #7\n"); return 0;}

			l->chunk->records=calloc(sizeof(struct FFF_ENVELOPE_RECORD), l->chunk->num_envelopes);
			lcr=l->chunk->records;

			for (i=0; i<l->chunk->num_envelopes; i++)
			{
				if (fread(&lcr[i].nattack,        sizeof(uint16_t), 1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #8\n"); return 0;}
				lcr[i].nattack = int16_little (lcr[i].nattack);
				if (fread(&lcr[i].nrelease,       sizeof(uint16_t), 1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #9\n"); return 0;}
				lcr[i].nrelease = int16_little (lcr[i].nrelease);
				if (fread(&lcr[i].sustain_offset, sizeof(uint16_t), 1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #10\n"); return 0;}
				lcr[i].sustain_offset = uint16_little (lcr[i].sustain_offset);
				if (fread(&lcr[i].sustain_rate,   sizeof(uint16_t), 1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #11\n"); return 0;}
				lcr[i].sustain_rate = uint16_little (lcr[i].sustain_rate);
				if (fread(&lcr[i].release_rate,   sizeof(uint16_t), 1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #12\n"); return 0;}
				lcr[i].release_rate = uint16_little (lcr[i].release_rate);
				if (fread(&lcr[i].hirange,        sizeof(uint8_t),  1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #13\n"); return 0;}
				if (fread(&lcr[i].d0,             sizeof(uint8_t),  1, file)!=1) {fprintf(stderr, __FILE__ ": fread() failed #14\n"); return 0;}

				if (lcr[i].nattack)
					lcr[i].attack_points=calloc(sizeof(struct FFF_ENVELOPE_POINT), lcr[i].nattack);
				else
					lcr[i].attack_points=0;

				if (lcr[i].nrelease)
					lcr[i].release_points=calloc(sizeof(struct FFF_ENVELOPE_POINT), lcr[i].nrelease);
				else
					lcr[i].release_points=0;
				if (lcr[i].nattack)
				{
					if (fread(lcr[i].attack_points, sizeof(struct FFF_ENVELOPE_POINT)*lcr[i].nattack, 1, file)!=1)
					{
						fprintf(stderr, __FILE__ ": fread() failed #15\n");
						return 0;
					}
					FFF_ENVELOPE_POINT_endian(lcr[i].attack_points, lcr[i].nattack);
				}
				if (lcr[i].nrelease)
				{
					if (fread(lcr[i].release_points, sizeof(struct FFF_ENVELOPE_POINT)*lcr[i].nrelease, 1, file)!=1)
					{
						fprintf(stderr, __FILE__ ": fread() failed #16\n");
						return 0;
					}
					FFF_ENVELOPE_POINT_endian(lcr[i].release_points, lcr[i].nrelease);
				}
			}
		}

		if (!strncmp(hd.id, "PROG", 4))
		{
			uint32_t dummy[2];

			matched=1;

			if (fread(dummy, sizeof(dummy), 1, file) != 1)
			{
				fprintf(stderr, __FILE__ ": fread() failed #17\n");
				return 0;
			}
#ifdef DEBUG
			fprintf(stderr, "[FFF] skipped program\n");
#endif
		}

		if (!strncmp(hd.id, "PTCH", 4))
		{
			struct FFF_PTCH_LIST  *l;
			struct FFF_PTCH_CHUNK *c;
			struct FFF_LAYR_CHUNK *lr;
			uint32_t dummy;

#ifdef DEBUG
			fprintf(stderr, "[FFF] loading patch\n");
#endif

			matched=1;

			l=calloc(sizeof(struct FFF_PTCH_LIST), 1);
			l->chunk=calloc(sizeof(struct FFF_PTCH_CHUNK), 1);
			l->next=ptch_list;
			ptch_list=l;
			c=l->chunk;

			if (fread(&c->id.id,         sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #17\n"); return 0;}
			c->id.id = uint32_little (c->id.id);
			if (fread(&c->nlayers,       sizeof(uint16_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #18\n"); return 0;}
			c->nlayers = int16_little (c->nlayers);
			if (fread(&c->layer_mode,    sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #19\n"); return 0;}
			if (fread(&c->excl_mode,     sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #20\n"); return 0;}
			if (fread(&c->excl_group,    sizeof(uint16_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #21\n"); return 0;}
			c->excl_group = int16_little (c->excl_group);
			if (fread(&c->effect1,       sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #22\n"); return 0;}
			if (fread(&c->effect1_depth, sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #23\n"); return 0;}
			if (fread(&c->effect2,       sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #24\n"); return 0;}
			if (fread(&c->effect2_depth, sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #25\n"); return 0;}
			if (fread(&c->bank,          sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #26\n"); return 0;}
			if (fread(&c->program,       sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #27\n"); return 0;}

			/* actually, iw_layer isn't used on disk, but it's there (fd) yup (ryg) */
			if (fread(&dummy,            sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #28\n"); return 0;}

#ifdef DEBUG
			fprintf(stderr, "[FFF] %d:%d loading, %d layers.\n", c->bank, c->program, c->nlayers);
#endif

			c->iw_layer=calloc(sizeof(struct FFF_LAYR_CHUNK), c->nlayers);

			for (i=0; i<c->nlayers; i++)
			{
				struct FFF_CHUNK_HEADER  lrhd;
				struct FFF_WAVE_CHUNK   *wv;
				uint32_t dummy;

				lr=&c->iw_layer[i];

				if (fread(&lrhd, sizeof(lrhd), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #29\n"); return 0;}
				FFF_CHUNK_HEADER_endian (&lrhd);

				/* sachmal ryg, wielange warste schon auf als du DAS hier gecoded hast? merke:
				 * strcmp==0 means "gleich"! ;) (fd) 17 stunden, wieso? (ryg)
				 */
				if (strncmp(lrhd.id, "LAYR", 4))
				{
					printf("[FFF] non-LAYR chunk in PTCH (malformed FFF file)\n");
					printf("[FFF] (found %c%c%c%c-chunk)\n", lrhd.id[0], lrhd.id[1], lrhd.id[2], lrhd.id[3]);
					return 0;
				}

				if (fread(&lr->id.id,          sizeof(uint32_t),       1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #30\n"); return 0;}
				lr->id.id = uint32_little(lr->id.id);
				if (fread(&lr->nwaves,         sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #31\n"); return 0;}
				if (fread(&lr->flags,          sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #32\n"); return 0;}
				if (fread(&lr->high_range,     sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #33\n"); return 0;}
				if (fread(&lr->low_range,      sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #34\n"); return 0;}
				if (fread(&lr->pan,            sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #35\n"); return 0;}
                                if (fread(&lr->pan_freq_scale, sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #36\n"); return 0;}
				if (fread(&lr->tremolo,        sizeof(struct FFF_LFO), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #37\n"); return 0;}
				FFF_LFO_endian (&lr->tremolo);
				if (fread(&lr->vibrato,        sizeof(struct FFF_LFO), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #38\n"); return 0;}
				FFF_LFO_endian (&lr->vibrato);
				if (fread(&lr->velocity_mode,  sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #39\n"); return 0;}
				if (fread(&lr->attenuation,    sizeof(uint8_t),        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #40\n"); return 0;}
				if (fread(&lr->freq_scale,     sizeof(uint16_t),       1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #41\n"); return 0;}
				lr->freq_scale = int16_little (lr->freq_scale);
				if (fread(&lr->freq_center,    sizeof(uint16_t),       1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #42\n"); return 0;}
				if (fread(&lr->layer_event,    sizeof(uint16_t),       1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #43\n"); return 0;}
				if (fread(&lr->penv.id,        sizeof(uint32_t),       1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #44\n"); return 0;}
				lr->penv.id = uint32_little (lr->penv.id);
				if (fread(&lr->venv.id,        sizeof(uint32_t),       1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #45\n"); return 0;}
				lr->venv.id = uint32_little (lr->venv.id);

				if (fread(&dummy,              sizeof(uint32_t),       1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #46\n"); return 0;}

				lr->waves=calloc(sizeof(struct FFF_WAVE_CHUNK), lr->nwaves);

#ifdef DEBUG
				fprintf(stderr, "[FFF] ... %d waves.\n", lr->nwaves);
#endif

				for (j=0; j<lr->nwaves; j++)
				{
					struct FFF_CHUNK_HEADER  wvhd;

					wv=&lr->waves[j];

					if (fread(&wvhd, sizeof(wvhd), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #47\n"); return 0;}
					FFF_CHUNK_HEADER_endian (&wvhd);

					if (strncmp(wvhd.id, "WAVE", 4))
					{
						fprintf(stderr, "[FFF] non-WAVE chunk in LAYR (malformed FFF file)\n");
						fprintf(stderr, "[FFF] (found %c%c%c%c-chunk)\n", wvhd.id[0], wvhd.id[1], wvhd.id[2], wvhd.id[3]);
						return 0;
					};

					if (fread(&wv->id.id,        sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #48\n"); return 0;}
					wv->id.id = uint32_little (wv->id.id);
					if (fread(&wv->size,         sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #49\n"); return 0;}
					wv->size = uint32_little (wv->size);
					if (fread(&wv->start,        sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #50\n"); return 0;}
					wv->start = uint32_little (wv->start);
					if (fread(&wv->loopstart,    sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #51\n"); return 0;}
					wv->loopstart = uint32_little (wv->loopstart);
					if (fread(&wv->loopend,      sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #52\n"); return 0;}
					wv->loopend = uint32_little (wv->loopend);
					if (fread(&wv->m_start,      sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #53\n"); return 0;}
					wv->m_start = uint32_little (wv->m_start);
					if (fread(&wv->sample_ratio, sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #54\n"); return 0;}
					wv->sample_ratio = uint32_little (wv->sample_ratio);
					if (fread(&wv->attenuation,  sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #55\n"); return 0;}
					if (fread(&wv->low_note,     sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #56\n"); return 0;}
					if (fread(&wv->high_note,    sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #57\n"); return 0;}
					if (fread(&wv->format,       sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #58\n"); return 0;}
					if (fread(&wv->m_format,     sizeof(uint8_t),  1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #59\n"); return 0;}
					if (fread(&wv->data_id.id,   sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #60\n"); return 0;}
					wv->data_id.id = uint32_little (wv->data_id.id);
					fprintf(stderr, "wave %d loaded.. (%d bytes, playable from %x to %x)\n", i, wv->size, wv->low_note, wv->high_note);
				}
			}
		}

		if (!strncmp(hd.id, "DATA", 4))
		{
			struct FFF_DATA_LIST       *l;
#ifdef DEBUG
			fprintf(stderr, "[FFF] read DATA chunk\n");
#endif

			matched=1;

			l=calloc(sizeof(struct FFF_DATA_LIST), 1);
			l->chunk=calloc(sizeof(struct FFF_DATA_CHUNK), 1);
			l->next=data_list;
			data_list=l;
			if (fread(&l->chunk->id.id,       sizeof(uint32_t), 1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #61\n"); return 0;}
			l->chunk->id.id = uint32_little (l->chunk->id.id);
			if (fread(&l->chunk->filename[0], hd.size-4,        1, file)!=1) { fprintf(stderr, __FILE__ ": fread() failed #62\n"); return 0;}
		}

		if (!matched)
		{
			fseek(file, hd.size, SEEK_CUR);
#ifdef DEBUG
			fprintf(stderr, "[FFF] skipped %c%c%c%c-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif
		}

		if (fread(&hd, sizeof(hd), 1, file)!=1) break;
		FFF_CHUNK_HEADER_endian (&hd);
	}

	/* pass 2: convert those IDs to pointers (yeeah) */

	l=ptch_list;

	while (l)
	{
		for (i=0; i<l->chunk->nlayers; i++)
		{
			struct FFF_LAYR_CHUNK *lc;

			lc=&l->chunk->iw_layer[i];

			if (lc->penv.id)
			{
				lc->penv.ptr=getENVP(lc->penv.id);
				if (! lc->penv.ptr )
				{
					fprintf (stderr, "penvelop id %x not found.\n", lc->penv.id);
					return 0;
				}
			}

			if (lc->venv.id)
			{
				lc->venv.ptr=getENVP(lc->venv.id);
				if (! lc->venv.ptr )
				{
					fprintf (stderr, "venvelop id %x not found.\n", lc->venv.id);
					return 0;
				}
			}

			for (j=0; j<lc->nwaves; j++)
			{
				lc->waves[j].data_id.ptr=getDATA(lc->waves[j].data_id.id);
				if (! lc->waves[j].data_id.ptr )
				{
					fprintf (stderr, "wavedata file #%x not found...\n", lc->waves[j].data_id.id);
					return 0;
				}
			}
		}

		l=l->next;
	}

	fprintf(stderr, "done with that loading. be happy.\n");

	return 1;
}

static void closeFFF(void)
{
	struct FFF_ENVP_LIST *el=envp_list;
	struct FFF_PTCH_LIST *pl=ptch_list;
	struct FFF_DATA_LIST *dl=data_list;

	while (el)
	{
		struct FFF_ENVP_LIST *t=el->next;
		int i;
		for (i=0; i<el->chunk->num_envelopes; i++)
		{
			free(el->chunk->records[i].attack_points);
			free(el->chunk->records[i].release_points);
		}
		free(el->chunk->records);
		free(el->chunk);
		free(el);
		el=t;
	}

	while (pl)
	{
		struct FFF_PTCH_LIST *t=pl->next;
		int i;
		for (i=0; i<pl->chunk->nlayers; i++)
			free(pl->chunk->iw_layer[i].waves);

		free(pl->chunk->iw_layer);
		free(pl->chunk);

		free(pl);;
		pl=t;
	}

	while (dl)
	{
		struct FFF_DATA_LIST *t=dl->next;
		free(dl->chunk);
		free(dl);
		dl=t;
	}
}

/*
static int addpatchFFF( struct minstrument *ins,
                        uint8_t             program,
                        uint8_t             sn,
                        uint8_t             sampnum,
                        struct sampleinfo  *sip,
                        uint16_t           *samplenum)
{
	return 0;
}
*/
/*
 * instrument name table for .FFF files (melodic bank, after GM standard)
 */

static uint32_t getfreq(uint16_t note)
{
	int freq=pocttab[note/256/12+1];
	freq=umuldiv(freq, pnotetab[(note/256)%12], 32768);
	freq=umuldiv(freq, pfinetab[(note/16)&0xF], 32768);
	return(umuldiv(freq, pxfinetab[(note)&0xF], 32768));  /* (x-)finetuning not VERY much tested. */
}


static char *gmins_melo[]={
	"Acoustic Piano 1", "Acoustic Piano 2", "Acoustic Piano 3",
	"Honky-tonk", "E-Piano 1", "E-Piano 2", "Harpsichord",
	"Clavinet", "Celesta", "Glockenspiel", "Music Box",
	"Vibraphone", "Marimbaphone", "Xylophone", "Tubular-bell",
	"Santur", "Organ 1", "Organ 2", "Organ 3", "Church Organ",
	"Reed Organ", "Accordion", "Harmonica", "Bandoneon",
	"Nylon-str. Guit.", "Steel-str. Guit.", "Jazz Guitar",
	"Clean Guitar", "Muted Guitar", "Overdrive Guitar",
	"Distortion Guit.", "Guitar Harmonics", "Acoustic Bass",
	"Fingered Bass", "Picked Bass", "Fretless Bass",
	"Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
	"Violin", "Viola", "Cello", "Contrabass", "Tremolo String",
	"Pizzicato String", "Harp", "Timpani", "Strings",
	"Slow Strings", "Synth Strings 1", "Synth Strings 2", "Choir",
	"Voice Oohs", "SynVox", "Orchestra Hit", "Trumpet",
	"Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass 1",
	"Synth Brass 1", "Synth Brass 2", "Soprano Saxophone",
	"Alto Saxophone", "Tenor Saxophone", "Baritone Saxophone",
	"Oboe", "English Horn", "Bassoon", "Clarinet", "Piccolo",
	"Flute", "Recorder", "Pan Flute", "Bottle Blow",
	"Shakuhachi", "Whistle", "Ocarina", "Square Wave", "Saw wave",
	"Synth Calliope", "Chiffer Lead", "Charang", "Solo Vox",
	"5th Saw Wave", "Bass & Lead", "Fantasia", "Warm Pad",
	"Polysynth", "Space Voice", "Bowed Glass", "Metal Pad",
	"Halo Pad", "Sweeo Pad", "Ice Rain", "Soundtrack", "Crystal",
	"Atmosphere", "Brightness", "Goblin", "Echo Drops",
	"Star Theme", "Sitar", "Banjo", "Shamisen", "Koto", "Kalima",
	"Bag Pipe", "Fiddle", "Shannai", "Tinkle Bell", "Agogo",
	"Steel Drums", "Woodblock", "Taiko", "Melodic Tom 1",
	"Synth Drum", "Reverse Cymbal", "Guitar FretNoise",
	"Breath Noise", "Seashore", "Bird", "Telephone 1",
	"Helicopter", "Applause", "Gun Shot"
};

static int loadpatchFFF(struct minstrument *ins,
                        uint8_t             program,
                        uint8_t            *sampused,
                        struct sampleinfo **smps,
                        uint16_t           *samplenum)
{
	struct FFF_PTCH_LIST *pl=ptch_list;
	struct FFF_PTCH_CHUNK *patch;
	struct FFF_LAYR_CHUNK *layer;
	struct FFF_ENVP_CHUNK *envelope[2];

	int i, e, n, smp;

	while (pl)
	{
		if (pl->chunk->program==program) break;
		pl=pl->next;
	}
	if (!pl)
	{
		fprintf(stderr, "[FFF]: program %d not found!\n", program);
		return errGen;
	}

	patch=pl->chunk;
	layer=patch->iw_layer;       /* todo: more layers?!? don't know how.. (fd) */
	envelope[0]=(struct FFF_ENVP_CHUNK*)layer->penv.ptr;
	envelope[1]=(struct FFF_ENVP_CHUNK*)layer->venv.ptr;

/*
	sprintf(ins->name, ".-.-.-", program);
*/

	if (program<128)
		strcpy(ins->name, gmins_melo[program]);
	else
		sprintf(ins->name, "#%d", program);

	ins->prognum=program;
	ins->sampnum=layer->nwaves;
	ins->samples=calloc(sizeof(struct msample), layer->nwaves);

	*smps=calloc(sizeof(struct sampleinfo), layer->nwaves);

	fprintf(stderr, "loading program %d\n", program);

	for(i=0; i<2; i++)
	{
		if(!(i?layer->venv.ptr:layer->penv.ptr)) continue;
		fprintf(stderr, "%s envelope(s):\n", i?"volume":"pitch");
		fprintf(stderr, "   retrigger: %d\n", envelope[i]->flags);
		fprintf(stderr, "   mode     : %d\n", envelope[i]->mode);
		fprintf(stderr, "   indtype  : %d\n", envelope[i]->indtype);
		for(e=0; e<envelope[i]->num_envelopes; e++)
		{
			struct FFF_ENVELOPE_RECORD *rec=&envelope[i]->records[e];
			fprintf(stderr, "  env #%d: (hirange: %d)\n", e, rec->hirange);
			fprintf(stderr, "   sustain_offset:        %d\n", rec->sustain_offset);
			fprintf(stderr, "   sustain_rate  :        %d\n", rec->sustain_rate);
			fprintf(stderr, "   release_rate  :        %d\n", rec->release_rate);
			fprintf(stderr, "   attack_envelope: \n    next:   ");
			for(n=0; n<rec->nattack; n++)
				fprintf(stderr, "%04d ", rec->attack_points[n].next);
			fprintf(stderr, "\n    r/t :   ");
			for(n=0; n<rec->nattack; n++)
				fprintf(stderr, "%04d ", rec->attack_points[n].data.rate);
			fprintf(stderr, "\n   release_envelope:\n    next:   ");
			for(n=0; n<rec->nrelease; n++)
				fprintf(stderr, "%04d ", rec->release_points[n].next);
			fprintf(stderr, "\n    r/t :   ");
			for(n=0; n<rec->nrelease; n++)
				fprintf(stderr, "%04d ", rec->release_points[n].data.rate);
			fprintf(stderr, "\n");
		}
	}

	for (smp=0; smp<layer->nwaves; smp++)
	{
		struct FFF_WAVE_CHUNK *wave=&layer->waves[smp];
		struct msample *s=&ins->samples[smp];
		int freq;
		int n, q;
		char *nf, *nt;
		FILE *dat;
		char filename[PATH_MAX+NAME_MAX];

		s->handle=(*samplenum)++;
		fprintf(stderr, "(3)Loaded sample %x\n", s->handle);
		s->sampnum=smp;

		s->normnote=layer->freq_center<<8;

		freq=(44100.0*1024.0/(float)wave->sample_ratio*(float)getfreq(s->normnote)/(float)1000);

/* ratio = 44100 * (root_freq * 1000) * 1024 / sample_rate / 1000
 * is this really right? (fd)  don't know, but seems to work (but some samples sound mistuned, hmmm...) (ryg)
 */

		for (n=wave->low_note; n<wave->high_note; n++)
			ins->note[n]=smp;

		for (q=0; q<6; q++)
		{
			s->volpos[q]=0;
			s->volrte[q]=0;
		}

		s->volpos[0]=62976;
		s->volrte[0]=250<<8;
		s->volpos[1]=62976;
		s->volrte[1]=0;
		s->end=2;
		s->sustain=1;
		s->sclfac=256;       /* ?!?? */
		s->sclbas=60;

		/* todo: double check them again.
		 * did it (and corrected 'em) (ryg)
		 */
/*
		s->tremswp=(layer->tremolo.sweep-10)*2*64/45;
		s->tremrte=((float)layer->tremolo.freq*7.0/3.0+15.0)*65536.0/(300.0*64.0);
		s->tremdep=layer->tremolo.depth*19*4*256/255/2;

		s->vibswp=(layer->vibrato.sweep-10)*2*64/45;
		s->vibrte=((float)layer->vibrato.freq*7.0/3.0+15.0)*65536.0/(300.0*64.0);
		s->vibdep=layer->vibrato.depth*19*4*256/255/2;

    ryg 990125: seems to be incorrect (vibratos glide over the whole fft
                analyser :)  - trying to fix this by combining beisert's
                formulas for PAT with some from GIPC ("a glorious hack")
*/

		s->tremswp=(layer->tremolo.sweep-10)*2*64/45;
		s->tremrte=(((float)layer->tremolo.freq*7.0/3.0)+15.0)*65536.0/(300.0*64);
		s->tremdep=(layer->tremolo.depth*51*256)/(255*7);

		s->vibswp=(layer->vibrato.sweep-10)*2*64/45;
		s->vibrte=(((float)layer->vibrato.freq*7.0/3.0)+15.0)*65536.0/(300.0*64);
		s->vibdep=(layer->vibrato.depth*12*256)/(255*7);

		fprintf(stderr, "   -> %d %d %d, %d %d %d\n", s->tremswp, s->tremrte, s->tremdep,
		                                              s->vibswp, s->vibrte, s->vibdep);

		(*smps)[smp].type =(wave->format&1)?0:mcpSamp16Bit;
		(*smps)[smp].type|=(wave->format&2)?0:mcpSampUnsigned;
		(*smps)[smp].type|=(wave->format&8)?mcpSampLoop:0;
		(*smps)[smp].type|=(wave->format&16)?mcpSampBiDi:0;
/*
		(*smps)[smp].type|=(wave->format&32)?mcpSampULaw:0;
*/
		(*smps)[smp].type|=(wave->format&32)?mcpSamp16Bit:0;

		(*smps)[smp].ptr=calloc(wave->size*((wave->format&1)?1:2), ((wave->format&32)?2:1));
		(*smps)[smp].length=wave->size;
		(*smps)[smp].samprate=freq;
		(*smps)[smp].loopstart=wave->loopstart>>4;
		(*smps)[smp].loopend=wave->loopend>>4;

		(*smps)[smp].sloopstart=0;
		(*smps)[smp].sloopend=0;

		nf=plNoteStr[wave->low_note];
		nt=plNoteStr[wave->high_note];
		sprintf(s->name, "%c%c%c to %c%c%c", nf[0], nf[1], nf[2],
		                                     nt[0], nt[1], nt[2]);

		snprintf(filename, sizeof(filename), "%s%s", cfGetProfileString("midi", "dir", "./"), ((struct FFF_DATA_CHUNK *)wave->data_id.ptr)->filename);

		if (!(dat=fopen(filename, "r")))
		{
			fprintf(stderr, "[FFF]: '%s': %s\n", filename, strerror(errno));
			free ((*smps)[smp].ptr);
			return errGen;
		}

		fseek(dat, wave->start, SEEK_SET);

		if(wave->format&32) /* ulaw */
		{
			int8_t *temp=calloc(wave->size, 1);
			uint16_t *d=(uint16_t *)(*smps)[smp].ptr;
			unsigned int i;
			if (fread(temp, wave->size, 1, dat)!=1) { fprintf(stderr, __FILE__ ": fread() failed #63\n"); free(temp); return 0;}
			for (i=0; i<wave->size; i++)
				((int16_t*)d)[i]=ulaw2linear(temp[i]);
			free(temp);
		} else
			if (fread((*smps)[smp].ptr, wave->size*((wave->format&1)?1:2), 1, dat) != 1) { fprintf(stderr, __FILE__ ": fread() failed #64\n"); return 0;}

		fclose(dat);
	}

	return errOk;
}

int __attribute__ ((visibility ("internal"))) midInitFFF(void)
{
	FILE *fff;
	const char *fn=cfGetProfileString("midi", "fff", "midi.fff");
	char path[PATH_MAX+NAME_MAX+1];

	_midClose=0;

	if(!(fff=fopen(fn, "r")))
	{
		if (!strchr(fn, '/'))
		{
			snprintf(path, sizeof(path), "%s%s", cfDataDir, fn);
			fff=fopen(fn, "r");
		}
	}
	if (!fff)
	{
		fprintf(stderr, "[FFF]: '%s': %s\n", fn, strerror(errno));
		return 0;
	}

	_midClose=closeFFF;
	if (!loadFFF(fff))
	{
		fprintf(stderr, "Failed to load FFF\n");
		return 0;
	}
	loadpatch = loadpatchFFF;
	addpatch = 0; /*addpatchFFF;*/
	return 1;
}
