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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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

#include "gmifff-internals.h"

/* #define DEBUG */

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

struct FFF_ENVP_ENTRY_CHUNK
{
	struct FFF_ENVP_ENTRY   header;
	struct FFF_ENVP_POINT  *attack_points;
	struct FFF_ENVP_POINT  *release_points;
};

struct FFF_ENVP_CHUNK
{
	struct FFF_ENVP_HEADER        header;
	struct FFF_ENVP_ENTRY_CHUNK  *records;
};

struct FFF_PTCH_CHUNK
{
	struct FFF_PTCH_HEADER header;
	struct FFF_LAYR_CHUNK *iw_layer;
};

struct FFF_WAVE_CHUNK
{
	struct FFF_WAVE_HEADER header;
	struct FFF_DATA_HEADER *data;
};

struct FFF_LAYR_CHUNK
{
	struct FFF_LAYR_HEADER  header;
	struct FFF_WAVE_CHUNK  *waves;
	struct FFF_ENVP_CHUNK  *penv;
	struct FFF_ENVP_CHUNK  *venv;
};

/* These 3 are not related for file-format, but used as lookups */

struct FFF_ENVP_LIST
{
	struct FFF_ENVP_CHUNK     *chunk;
	struct FFF_ENVP_LIST       *next;
};

struct FFF_PTCH_LIST
{
	struct FFF_PTCH_CHUNK     *chunk;
	struct FFF_PTCH_LIST       *next;
};

struct FFF_DATA_LIST
{
	struct FFF_DATA_HEADER    *chunk;
	struct FFF_DATA_LIST       *next;
};

struct FFF_Session
{
	int fd;
	unsigned char *data;
	size_t data_len;
	size_t data_mmaped_len;
	
	struct FFF_ENVP_LIST *envp_list;
	struct FFF_PTCH_LIST *ptch_list;
	struct FFF_DATA_LIST *data_list;

	struct FFF_PTCH_CHUNK *ptch_current;
	int                    ptch_layer_n;
};

struct __attribute__((packed)) FFF_CHUNK_HEADER
{
	char                        id[4];
	uint32_t                    size;
};

static inline void FFF_CHUNK_HEADER_endian(struct FFF_CHUNK_HEADER *a) {
	a->size = uint32_little(a->size);
}

static struct FFF_ENVP_CHUNK *getENVP(struct FFF_Session *s, struct FFF_ID *id)
{
	struct FFF_ENVP_LIST *l=s->envp_list;

	while (l)
	{
		if ((l->chunk->header.id.major==id->major) &&
		    (l->chunk->header.id.minor==id->minor))
		{
			return l->chunk;
		}
		l=l->next;
	};

	return 0;
}

static struct FFF_DATA_HEADER *getDATA(struct FFF_Session *s, struct FFF_ID *id)
{
	struct FFF_DATA_LIST *l=s->data_list;

	while (l)
	{
		if ((l->chunk->id.major==id->major) &&
		    (l->chunk->id.minor==id->minor))
		{
			return l->chunk;
		}
		l=l->next;
	};

	return 0;
}

static int parseFFFF_ENVP(struct FFF_Session *s, unsigned char *ptr, size_t ptrlen)
{
	int i, j;

	struct FFF_ENVP_LIST        *l;
	struct FFF_ENVP_ENTRY_CHUNK *lcr;

	l=calloc(sizeof(struct FFF_ENVP_LIST), 1);
	l->chunk=calloc(sizeof(struct FFF_ENVP_CHUNK), 1);
	l->next=s->envp_list;
	s->envp_list=l;

	if (ptrlen < sizeof (l->chunk->header))
	{
		fprintf (stderr, "[FFF]: ENVP chunk truncated\n");
		return 0;
	}
	l->chunk->header = *(struct FFF_ENVP_HEADER *)ptr;
	ptr += sizeof (struct FFF_ENVP_HEADER); ptrlen -= sizeof (struct FFF_ENVP_HEADER);
	FFF_ENVP_HEADER_endian (&l->chunk->header);

	l->chunk->records=calloc(sizeof(struct FFF_ENVP_ENTRY_CHUNK), l->chunk->header.num_envelopes);
	lcr=l->chunk->records; /* short alias */

	for (i=0; i<l->chunk->header.num_envelopes; i++)
	{
		if (ptrlen < sizeof (lcr[0].header))
		{
			fprintf (stderr, "[FFF]: ENVP (record) chunk truncated\n");
			return 0;
		}
		lcr[i].header = *(struct FFF_ENVP_ENTRY *)ptr;
		ptr += sizeof (struct FFF_ENVP_ENTRY); ptrlen -= sizeof (struct FFF_ENVP_ENTRY);
		FFF_ENVP_ENTRY_endian (&lcr[i].header);


		lcr[i].attack_points=calloc(sizeof(struct FFF_ENVP_POINT), lcr[i].header.nattack);

		lcr[i].release_points=calloc(sizeof(struct FFF_ENVP_POINT), lcr[i].header.nrelease);

		for (j=0; j < lcr[i].header.nattack; j++)
		{
			if (ptrlen < sizeof (struct FFF_ENVP_POINT))
			{
				fprintf (stderr, "[FFF]: ENVP (record) chunk truncated\n");
				return 0;
			}
			lcr[i].attack_points[j] = *(struct FFF_ENVP_POINT *)ptr;
			ptr += sizeof (struct FFF_ENVP_POINT); ptrlen -= sizeof (struct FFF_ENVP_POINT);
			FFF_ENVP_POINT_endian (lcr[i].attack_points + j);
		}

		for (j=0; j < lcr[i].header.nrelease; j++)
		{
			if (ptrlen < sizeof (struct FFF_ENVP_POINT))
			{
				fprintf (stderr, "[FFF]: ENVP (record) chunk truncated\n");
				return 0;
			}
			lcr[i].release_points[j] = *(struct FFF_ENVP_POINT *)ptr;
			ptr += sizeof (struct FFF_ENVP_POINT); ptrlen -= sizeof (struct FFF_ENVP_POINT);
			FFF_ENVP_POINT_endian (lcr[i].release_points + j);
		}
	}

	return 1;
}

static int parseFFFF_DATA(struct FFF_Session *s, unsigned char *ptr, size_t ptrlen)
{
	struct FFF_DATA_LIST       *l;

	l=calloc(sizeof(struct FFF_DATA_LIST), 1);
	l->chunk=calloc(1, ptrlen);
	l->next=s->data_list;
	s->data_list=l;

	if (ptrlen < 5)
	{
		fprintf (stderr, "[FFF]: DATA chunk truncated\n");	
	}

	memcpy (l->chunk, ptr, ptrlen);
	l->chunk->id = *(struct FFF_ID *)ptr;
	FFF_ID_endian (&l->chunk->id);
	
	return 1;
}

static struct FFF_PTCH_LIST *parseFFFF_PROG_PTCH(struct FFF_Session *s, unsigned char *ptr, size_t ptrlen)
{
	struct FFF_PTCH_LIST  *l;

	if (ptrlen < sizeof (l->chunk->header))
	{
		fprintf (stderr, "[FFF]: PTCH chunk truncated\n");
		return 0;
	}

	l=calloc(sizeof(struct FFF_PTCH_LIST), 1);
	l->chunk=calloc(sizeof(struct FFF_PTCH_CHUNK), 1);
	l->next=s->ptch_list;
	s->ptch_list=l;

	l->chunk->header = *(struct FFF_PTCH_HEADER *)ptr;
	FFF_PTCH_HEADER_endian (&l->chunk->header);

	l->chunk->iw_layer=calloc(sizeof(struct FFF_LAYR_CHUNK), l->chunk->header.nlayers);

	return l;
}

static int parseFFFF_PROG_LAYR(struct FFF_Session *s, unsigned char *ptr, size_t ptrlen, struct FFF_LAYR_CHUNK *chunk)
{
	if (ptrlen < sizeof (chunk->header))
	{
		fprintf (stderr, "[FFF]: LAYR chunk truncated\n");
		return 0;
	}

	chunk->header = *(struct FFF_LAYR_HEADER *)ptr;
	FFF_LAYR_HEADER_endian (&chunk->header);

	chunk->waves=calloc(sizeof(struct FFF_WAVE_CHUNK), chunk->header.nwaves);

	return 1;
}

static int parseFFFF_PROG_WAVE(struct FFF_Session *s, unsigned char *ptr, size_t ptrlen, struct FFF_WAVE_CHUNK *chunk)
{
	if (ptrlen < sizeof (chunk->header))
	{
		fprintf (stderr, "[FFF]: WAVE chunk truncated\n");
		return 0;
	}

	chunk->header = *(struct FFF_WAVE_HEADER *)ptr;
	FFF_WAVE_HEADER_endian (&chunk->header);

	return 1;
}

static int parseFFFF_PROG(struct FFF_Session *s, unsigned char *ptr, size_t ptrlen)
{
	struct FFF_CHUNK_HEADER hd;
	unsigned char *data;
	size_t len;

	if (ptrlen < sizeof (struct FFF_PROG_HEADER))
	{
		fprintf (stderr, "[FFF]: PROG chunk truncated\n");
		return 0;
	}
	/* currently we ignore ALL content of the PROG header */

	data = ptr + sizeof (struct FFF_PROG_HEADER);
	len = ptrlen - sizeof (struct FFF_PROG_HEADER);

	while (len)
	{
		int i, j;

		if (len < sizeof (hd))
		{
			fprintf (stderr, "[FFF]: no space for PROG sub-chunk header\n");
			return 0;
		}
		hd = *(struct FFF_CHUNK_HEADER *)data; data += sizeof (hd); len -= sizeof (hd);
		FFF_CHUNK_HEADER_endian (&hd);
#ifdef DEBUG
		fprintf(stderr, "[FFF] got %c%c%c%c-PROG-sub-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif
		if (hd.size > len)
		{
			fprintf (stderr, "[FFF]: chunk is truncated\n");
			return 0;
		}

		if (!memcmp(hd.id, "PTCH", 4))
		{
			struct FFF_PTCH_LIST *l;
			struct FFF_PTCH_CHUNK *c;

#ifdef DEBUG
			fprintf(stderr, "[FFF] loading patch\n");
#endif

			if (!(l = parseFFFF_PROG_PTCH(s, data, hd.size)))
				return 0;

			c=l->chunk;

			for (i=0; i<c->header.nlayers; i++)
			{
				struct FFF_LAYR_CHUNK *lr;

				lr=&c->iw_layer[i];

				/* now we cheat, we forward to the next chunk, which we expect to be an LAYR */
				data+=hd.size;
				len-=hd.size;

				fprintf (stderr, "skipping %d bytes\n", hd.size);

				/* cheat loop to top */
				if (len < sizeof (hd))
				{
					fprintf (stderr, "[FFF]: no space for PROG sub-chunk header\n");
					return 0;
				}
				hd = *(struct FFF_CHUNK_HEADER *)data; data += sizeof (hd); len -= sizeof (hd);
				FFF_CHUNK_HEADER_endian (&hd);
#ifdef DEBUG
				fprintf(stderr, "[FFF] got %c%c%c%c-PROG-sub-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif
				if (hd.size > len)
				{
					fprintf (stderr, "[FFF]: chunk is truncated\n");
					return 0;
				}
				/* cheat finished */


				/* sachmal ryg, wielange warste schon auf als du DAS hier gecoded hast? merke:
				 * strcmp==0 means "gleich"! ;) (fd) 17 stunden, wieso? (ryg)
				 */
				if (memcmp(hd.id, "LAYR", 4))
				{
					printf("[FFF] non-LAYR chunk in PTCH (malformed FFF file)\n");
					printf("[FFF] (found %c%c%c%c-chunk)\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
					return 0;
				}

				if (!parseFFFF_PROG_LAYR(s, data, hd.size, lr))
					return 0;

				for (j=0; j<lr->header.nwaves; j++)
				{
					struct FFF_WAVE_CHUNK *wv;

					wv=&lr->waves[j];

					/* now we cheat, we forward to the next chunk, which we expect to be an LAYR */
					data+=hd.size;
					len-=hd.size;
					/* cheat loop to top */
					if (len < sizeof (hd))
					{
						fprintf (stderr, "[FFF]: no space for PROG sub-chunk header\n");
						return 0;
					}
					hd = *(struct FFF_CHUNK_HEADER *)data; data += sizeof (hd); len -= sizeof (hd);
					FFF_CHUNK_HEADER_endian (&hd);
#ifdef DEBUG
					fprintf(stderr, "[FFF] got %c%c%c%c-PROG-sub-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif
					if (hd.size > len)
					{
						fprintf (stderr, "[FFF]: chunk is truncated\n");
						return 0;
					}
					/* cheat finished */

					if (memcmp(hd.id, "WAVE", 4))
					{
						fprintf(stderr, "[FFF] non-WAVE chunk in LAYR (malformed FFF file)\n");
						fprintf(stderr, "[FFF] (found %c%c%c%c-chunk)\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
						return 0;
					};

					if (!parseFFFF_PROG_WAVE(s, data, hd.size, wv))
						return 0;
					//fprintf(stderr, "wave %d loaded.. (%d bytes, playable from %x to %x)\n", i, wv->size, wv->low_note, wv->high_note);
				}
			}
		} else {
#ifdef DEBUG
			fprintf(stderr, "[FFF] skipped %c%c%c%c-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif
		}
		data+=hd.size;
		len-=hd.size;
	}

	return 1;
}

static int parseFFF(struct FFF_Session *s)
{
	struct FFF_CHUNK_HEADER hd;
	struct FFF_PTCH_LIST *l;

	unsigned char *data = s->data;
	size_t len = s->data_len;

	if (len < sizeof (hd))
	{
		fprintf (stderr, "[FFF]: no space for FFFF header\n");
		return 0;
	}
	hd = *(struct FFF_CHUNK_HEADER *)data; data += sizeof (hd); len -= sizeof (hd);
	FFF_CHUNK_HEADER_endian (&hd);

	if (memcmp(hd.id, "FFFF", 4))
	{
		fprintf(stderr, "[FFF]: not FFFF header\n");
		return 0;
	}

	while (len)
	{
		if (len < sizeof (hd))
		{
			fprintf (stderr, "[FFF]: no space for chunk header\n");
			return 0;
		}
		hd = *(struct FFF_CHUNK_HEADER *)data; data += sizeof (hd); len -= sizeof (hd);
		FFF_CHUNK_HEADER_endian (&hd);
#ifdef DEBUG
		fprintf(stderr, "[FFF] got %c%c%c%c-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif
		if (hd.size > len)
		{
			fprintf (stderr, "[FFF]: chunk is truncated\n");
			return 0;
		}


		if (!memcmp(hd.id, "ENVP", 4))
		{
			if (!parseFFFF_ENVP(s, data, hd.size))
				return 0;
		} else if (!memcmp(hd.id, "PROG", 4))
		{
			if (!parseFFFF_PROG(s, data, hd.size))
				return 0;
		} else if (!memcmp(hd.id, "DATA", 4))
		{
			if (!parseFFFF_DATA(s, data, hd.size))
				return 0;
		} else {
#ifdef DEBUG
			fprintf(stderr, "[FFF] skipped %c%c%c%c-chunk\n", hd.id[0], hd.id[1], hd.id[2], hd.id[3]);
#endif
		}
		data+=hd.size;
		len-=hd.size;
	}

	/* pass 2: convert those IDs to pointers (yeeah) */

	for (l = s->ptch_list; l; l = l->next)
	{
		int i;
		for (i=0; i<l->chunk->header.nlayers; i++)
		{
			int j;
			struct FFF_LAYR_CHUNK *lc;

			lc=&l->chunk->iw_layer[i];

			if ((lc->header.penv.major)||(lc->header.penv.minor))
			{
				lc->penv=getENVP(s, &lc->header.penv);
				if (! lc->penv )
				{
					fprintf (stderr, "pitch envelop id 0x%04x:0x%04x not found.\n", lc->header.penv.major, lc->header.penv.minor);
					return 0;
				}
			}

			if ((lc->header.venv.major)||(lc->header.venv.minor))
			{
				lc->venv=getENVP(s, &lc->header.venv);
				if (! lc->venv )
				{
					fprintf (stderr, "velocity envelop id 0x%04x:0x%04x not found.\n", lc->header.venv.major, lc->header.venv.minor);
					return 0;
				}
			}

			for (j=0; j<lc->header.nwaves; j++)
			{
				lc->waves[j].data=getDATA(s, &lc->waves[j].header.data);
				if (! lc->waves[j].data )
				{
					fprintf (stderr, "wavedata file 0x%04x:0x%04x not found...\n", lc->waves[j].header.data.major, lc->waves[j].header.data.minor);
					return 0;
				}
			}
		}
	}

	return 1;
}

static struct FFF_Session _FFF_Session;
#define roundup(x,y) (((x) + (y) - 1) & ~((y) - 1))
static int loadFFF(int fd)
{
	struct stat st;
	size_t ps = sysconf(_SC_PAGE_SIZE);

	bzero (&_FFF_Session, sizeof (_FFF_Session));
	_FFF_Session.fd = fd;

	if (fstat(_FFF_Session.fd, &st))
	{
		perror("[FFF]: fstat()");
		return -1;
	}
	if (!st.st_size)
	{
		fprintf (stderr, "[FFF] Zero-size file\n");
		return -1;
	}

	_FFF_Session.data_len = st.st_size;
	_FFF_Session.data_mmaped_len = roundup (_FFF_Session.data_len, ps);
	_FFF_Session.data = mmap (0, _FFF_Session.data_mmaped_len, PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE, _FFF_Session.fd, 0);

	if (_FFF_Session.data == MAP_FAILED)
	{
		perror ("[FFF]: mmap() failed");
		return -1;
	}

	if (parseFFF(&_FFF_Session) == 0)
	{
		munmap (_FFF_Session.data, _FFF_Session.data_mmaped_len);
		return -1;
	}
	
	munmap (_FFF_Session.data, _FFF_Session.data_mmaped_len);
	return 0;
}

static void closeFFF(void)
{
	struct FFF_ENVP_LIST *el=_FFF_Session.envp_list;
	struct FFF_PTCH_LIST *pl=_FFF_Session.ptch_list;
	struct FFF_DATA_LIST *dl=_FFF_Session.data_list;

	while (el)
	{
		struct FFF_ENVP_LIST *t=el->next;
		int i;
		for (i=0; i<el->chunk->header.num_envelopes; i++)
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
		for (i=0; i<pl->chunk->header.nlayers; i++)
		{
			free(pl->chunk->iw_layer[i].waves);
		}

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

	_FFF_Session.envp_list = 0;
	_FFF_Session.ptch_list = 0;
	_FFF_Session.data_list = 0;
}

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

static int loadpatchFFF(struct minstrument *ins, /* bank MSB, LSB and program is pre-filled into instrument */
	                uint8_t            *notesused,
	                struct sampleinfo **smps,
	                uint16_t           *samplenum)
{
	struct FFF_PTCH_LIST *pl;
	struct FFF_PTCH_CHUNK *patch;
	struct FFF_LAYR_CHUNK *layer;
	struct FFF_ENVP_CHUNK *envelope[2];

	int i, e, n, smp;
	int smp_count, smp_index;
	struct sampleinfo *sample;

	if (ins->bankmsb == 120) /* drums */
	{
		int drum;
		int drum_sample = 0;

		strcpy(ins->name, "Drums");

		/* first we count/predict the number of samples */
		for (drum=0; drum<128; drum++)
		{
			if (notesused[drum>>3]&(1<<(drum&7)))
			{
				drum_sample++;
			}
		}
		/* allocate */
		ins->sampnum=drum_sample;
		ins->samples=calloc(sizeof(struct msample), drum_sample);
#warning fetch alloc failed here
		*smps=realloc(*smps, sizeof(struct sampleinfo) * ((*samplenum)+drum_sample));
		/* put the counter back to zero, and do the actual loading */
		drum_sample = 0;

		for (drum=0; drum<128; drum++)
		{
			if (notesused[drum>>3]&(1<<(drum&7)))
			{
				struct FFF_WAVE_CHUNK *wave;
				struct msample *s=&ins->samples[drum_sample];

				int freq;
				int q;
				char *nf, *nt;
				FILE *dat;
				char filename[PATH_MAX+NAME_MAX];

				for (pl=_FFF_Session.ptch_list; pl; pl=pl->next)
				{
					if (pl->chunk->header.program==(drum+128))
					{
						break;
					}
				}
				if (!pl)
				{
					fprintf(stderr, "[FFF]: drum %d not found!\n", drum);
					return errGen;
				}

				fprintf(stderr, "loading drum %d\n", drum);

				patch=pl->chunk;
				layer=patch->iw_layer;       /* todo: more layers?!? don't know how.. (fd) */
				if (!layer)
				{
					fprintf (stderr, "No layer found\n");
					return errGen;
				}
				envelope[0]=layer->penv;
				envelope[1]=layer->venv;
				ins->note[drum]=drum_sample; /* link the drum into the virtual midi-note */

				#warning Envelope as not used yet!

				/* Try to find the correct wave, else force the first one */
				for (smp=0; smp<layer->header.nwaves; smp++)
				{
					wave = layer->waves + smp;

					if ((wave->header.low_note >= drum) && (wave->header.high_note >= drum))
					{
						break;
					}
				}
				if (!wave)
				{
					wave = layer->waves;
				}
				if (!wave)
				{
					fprintf (stderr, "No Wave found\n");
					return errGen;
				}

				s->handle=(*samplenum);
				sample = *smps + (*samplenum);
				(*samplenum)++;

				s->sampnum=drum_sample;
				s->normnote=layer->header.freq_center<<8;

				freq=(44100.0*1024.0/(float)wave->header.sample_ratio*(float)getfreq(s->normnote)/(float)1000);

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

				s->tremswp=(layer->header.tremolo.sweep-10)*2*64/45;
				s->tremrte=(((float)layer->header.tremolo.freq*7.0/3.0)+15.0)*65536.0/(300.0*64);
				s->tremdep=(layer->header.tremolo.depth*51*256)/(255*7);

				s->vibswp=(layer->header.vibrato.sweep-10)*2*64/45;
				s->vibrte=(((float)layer->header.vibrato.freq*7.0/3.0)+15.0)*65536.0/(300.0*64);
				s->vibdep=(layer->header.vibrato.depth*12*256)/(255*7);

				sample->type =(wave->header.format&1)?0:mcpSamp16Bit;
				sample->type|=(wave->header.format&2)?0:mcpSampUnsigned;
				sample->type|=(wave->header.format&8)?mcpSampLoop:0;
				sample->type|=(wave->header.format&16)?mcpSampBiDi:0;
/*
				sample->type|=(wave->header.format&32)?mcpSampULaw:0;
*/
				sample->type|=(wave->header.format&32)?mcpSamp16Bit:0;

				sample->ptr=calloc(wave->header.size*((wave->header.format&1)?1:2), ((wave->header.format&32)?2:1));
				sample->length=wave->header.size;
				sample->samprate=freq;
				sample->loopstart=wave->header.loopstart>>4;
				sample->loopend=wave->header.loopend>>4;

				sample->sloopstart=0;
				sample->sloopend=0;

				nf=plNoteStr[wave->header.low_note];
				nt=plNoteStr[wave->header.high_note];
				sprintf(s->name, "%c%c%c to %c%c%c", nf[0], nf[1], nf[2],
				                                     nt[0], nt[1], nt[2]);

				snprintf(filename, sizeof(filename), "%s%s", cfGetProfileString("midi", "dir", "./"), wave->data->filename);

				if (!(dat=fopen(filename, "r")))
				{
					fprintf(stderr, "[FFF]: '%s': %s\n", filename, strerror(errno));
					return errGen;
				}

				fseek(dat, wave->header.start, SEEK_SET);

				if(wave->header.format&32) /* ulaw */
				{
					int8_t *temp=calloc(wave->header.size, 1);
					uint16_t *d=(uint16_t *)sample->ptr;
					unsigned int i;
					if (fread(temp, wave->header.size, 1, dat)!=1) { fprintf(stderr, __FILE__ ": fread() failed #63\n"); free(temp); return 0;}
					for (i=0; i<wave->header.size; i++)
						((int16_t*)d)[i]=ulaw2linear(temp[i]);
					free(temp);
				} else
					if (fread(sample->ptr, wave->header.size*((wave->header.format&1)?1:2), 1, dat) != 1) { fprintf(stderr, __FILE__ ": fread() failed #64\n"); return 0;}

				fclose(dat);

				drum_sample++;
			}
		}
		return errOk;
	}

	for (pl=_FFF_Session.ptch_list; pl; pl=pl->next)
	{
		if (pl->chunk->header.program==ins->prognum)
		{
			break;
		}
	}
	if (!pl)
	{
		fprintf(stderr, "[FFF]: program %d not found!\n", ins->prognum);
		return errGen;
	}

	patch=pl->chunk;
	layer=patch->iw_layer;       /* todo: more layers?!? don't know how.. (fd) */
	if (!layer)
	{
		fprintf (stderr, "No layer found\n");
		return errGen;
	}
	envelope[0]=layer->penv;
	envelope[1]=layer->venv;

	if (ins->prognum<128)
		strcpy(ins->name, gmins_melo[ins->prognum]);
	else
		sprintf(ins->name, "#%d", ins->prognum); /* should not be possible */

#warning Envelope as not used yet!
#ifdef DEBUG
	for(i=0; i<2; i++)
	{
		if(!(i?layer->venv:layer->penv)) continue;
		fprintf(stderr, "%s envelope(s):\n", i?"volume":"pitch");
		fprintf(stderr, "   retrigger: %d\n", envelope[i]->header.flags);
		fprintf(stderr, "   mode     : %d\n", envelope[i]->header.mode);
		fprintf(stderr, "   indtype  : %d\n", envelope[i]->header.index_type);
		for(e=0; e<envelope[i]->header.num_envelopes; e++)
		{
			struct FFF_ENVP_ENTRY_CHUNK *rec=&envelope[i]->records[e];
			fprintf(stderr, "  env #%d: (hirange: %d)\n", e, rec->header.hirange);
			fprintf(stderr, "   sustain_offset:        %d\n", rec->header.sustain_offset);
			fprintf(stderr, "   sustain_rate  :        %d\n", rec->header.sustain_rate);
			fprintf(stderr, "   release_rate  :        %d\n", rec->header.release_rate);
			fprintf(stderr, "   attack_envelope: \n    next:   ");
			for(n=0; n<rec->header.nattack; n++)
				fprintf(stderr, "%04d ", rec->attack_points[n].next);
			fprintf(stderr, "\n    r/t :   ");
			for(n=0; n<rec->header.nattack; n++)
				fprintf(stderr, "%04d ", rec->attack_points[n].data.rate);
			fprintf(stderr, "\n   release_envelope:\n    next:   ");
			for(n=0; n<rec->header.nrelease; n++)
				fprintf(stderr, "%04d ", rec->release_points[n].next);
			fprintf(stderr, "\n    r/t :   ");
			for(n=0; n<rec->header.nrelease; n++)
				fprintf(stderr, "%04d ", rec->release_points[n].data.rate);
			fprintf(stderr, "\n");
		}
	}
#endif

	smp_count = 0;
	for (smp=0; smp<layer->header.nwaves; smp++)
	{
		struct FFF_WAVE_CHUNK *wave=&layer->waves[smp];

		/* Check if this WAVE is needed (a LAYR is devided up into WAVE, were each WAVE has a note range) */
		for (n=wave->header.low_note; n<wave->header.high_note; n++)
		{
			if (notesused[n>>3]&(1<<(n&7)))
			{
				break;
			}
		}
		if (n==wave->header.high_note)
		{
			/* wave/range not used, skip it */
			continue;
		}
		smp_count++;
	}
	ins->sampnum=smp_count?smp_count:1;
	ins->samples=calloc(sizeof(struct msample), ins->sampnum);
#warning fetch alloc failed here
	*smps=realloc(*smps, sizeof(struct sampleinfo) * ((*samplenum)+ins->sampnum)); /* make room for atleast one sample.. we are going to hope for the best further down */

	fprintf(stderr, "loading program %d\n", ins->prognum);

	smp_index = 0;
	for (smp=0; smp<layer->header.nwaves; smp++)
	{
		struct FFF_WAVE_CHUNK *wave=&layer->waves[smp];
		struct msample *s=&ins->samples[smp_index];
		int freq;
		int n, q;
		char *nf, *nt;
		FILE *dat;
		char filename[PATH_MAX+NAME_MAX];

		/* Check if this WAVE is needed (a LAYR is devided up into WAVE, were each WAVE has a note range) */
		for (n=wave->header.low_note; n<wave->header.high_note; n++)
		{
			if (notesused[n>>3]&(1<<(n&7)))
			{
				break;
			}
		}
		if (n==wave->header.high_note)
		{
			/* wave/range not used, skip it... but if we detected no matching samples at all, we just load the first one and hope for the best */
			if (smp_count || smp)
			{
				continue;
			}
		}

		s->handle= (*samplenum);
		sample = *smps + (*samplenum);
		(*samplenum)++;

		s->sampnum=smp_index;

		s->normnote=layer->header.freq_center<<8;

		freq=(45158.4 * (float)getfreq(s->normnote) / (float)wave->header.sample_ratio);

/* ratio = 44100 * (root_freq * 1000) * 1024 / sample_rate / 1000
 * is this really right? (fd)  don't know, but seems to work (but some samples sound mistuned, hmmm...) (ryg)
 */
		for (n=wave->header.low_note; n<wave->header.high_note; n++)
			ins->note[n]=smp_index;

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

		s->tremswp=(layer->header.tremolo.sweep-10)*2*64/45;
		s->tremrte=(((float)layer->header.tremolo.freq*7.0/3.0)+15.0)*65536.0/(300.0*64);
		s->tremdep=(layer->header.tremolo.depth*51*256)/(255*7);

		s->vibswp=(layer->header.vibrato.sweep-10)*2*64/45;
		s->vibrte=(((float)layer->header.vibrato.freq*7.0/3.0)+15.0)*65536.0/(300.0*64);
		s->vibdep=(int16_t)((layer->header.vibrato.depth*12*256)/(255*7));

		sample->type =(wave->header.format&1)?0:mcpSamp16Bit;
		sample->type|=(wave->header.format&2)?0:mcpSampUnsigned;
		sample->type|=(wave->header.format&8)?mcpSampLoop:0;
		sample->type|=(wave->header.format&16)?mcpSampBiDi:0;
/*
		sample->type|=(wave->header.format&32)?mcpSampULaw:0;
*/
		sample->type|=(wave->header.format&32)?mcpSamp16Bit:0;

		sample->ptr=calloc(wave->header.size*((wave->header.format&1)?1:2), ((wave->header.format&32)?2:1));

		sample->length=wave->header.size;
		sample->samprate=freq;
		sample->loopstart=wave->header.loopstart>>4;
		sample->loopend=wave->header.loopend>>4;

		sample->sloopstart=0;
		sample->sloopend=0;

		nf=plNoteStr[wave->header.low_note];
		nt=plNoteStr[wave->header.high_note];
		sprintf(s->name, "%c%c%c to %c%c%c", nf[0], nf[1], nf[2],
		                                     nt[0], nt[1], nt[2]);

		snprintf(filename, sizeof(filename), "%s%s", cfGetProfileString("midi", "dir", "./"), wave->data->filename);

		if (!(dat=fopen(filename, "r")))
		{
			fprintf(stderr, "[FFF]: '%s': %s\n", filename, strerror(errno));
			return errGen;
		}

		fseek(dat, wave->header.start, SEEK_SET);

		if(wave->header.format&32) /* ulaw */
		{
			int8_t *temp=calloc(wave->header.size, 1);
			uint16_t *d=(uint16_t *)sample->ptr;
			unsigned int i;
			if (fread(temp, wave->header.size, 1, dat)!=1) { fprintf(stderr, __FILE__ ": fread() failed #63\n"); free(temp); return 0;}
			for (i=0; i<wave->header.size; i++)
				((int16_t*)d)[i]=ulaw2linear(temp[i]);
			free(temp);
		} else
			if (fread(sample->ptr, wave->header.size*((wave->header.format&1)?1:2), 1, dat) != 1) { fprintf(stderr, __FILE__ ": fread() failed #64\n"); return 0;}

		fclose(dat);

		smp_index++;
	}

	return errOk;
}

int __attribute__ ((visibility ("internal"))) midInitFFF(void)
{
	int fd;
	const char *fn=cfGetProfileString("midi", "fff", "midi.fff");
	char path[PATH_MAX+NAME_MAX+1];

	_midClose=0;

	if (fn[0]!='/')
	{
		snprintf(path, sizeof(path), "%s%s", cfDataDir, fn);
		fd=open(fn, O_RDONLY);
	} else {
		fd=open(fn, O_RDONLY);
	}

	if (fd < 0)
	{
		fprintf(stderr, "[FFF]: '%s': %s\n", fn, strerror(errno));
		return 0;
	}

	if (loadFFF(fd))
	{
		close (fd);
		fprintf(stderr, "Failed to load FFF\n");
		closeFFF();
		return 0;
	}
	close (fd);
	_midClose=closeFFF;
	loadpatch = loadpatchFFF;
	return 1;
}
