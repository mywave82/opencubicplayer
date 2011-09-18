/* OpenCP Module Player
 * copyright (c) '05-'10 Stian Skjelstad <stian@nixia.no>
 *
 * Dump all info from a given .PAT file.
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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "types.h"
#include "gmipat.h"

static int i = 0;

#if 0

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
#endif

/* this is more nice, isn't it?
 *   -mw
 */
#if 0
struct __attribute__((packed)) PATCHHEADER
{
	char header[12]; /* "GF1PATCH110" */
	char gravis_id[10];   /* "ID#000002" */
	char description[60];
	uint8_t instruments;
	uint8_t voices;
	uint8_t channels;
	uint16_t wave_forms;
	uint16_t master_volume;
	uint32_t data_size;
	char reserved[36];
};
#endif
void dump_header(struct PATCHHEADER *h)
{
	fprintf(stderr, "header[22]:      %s\n", h->header);
	fprintf(stderr, "description[60]: %s\n", h->description);
	fprintf(stderr, "instruments:     %d\n", h->instruments);
	fprintf(stderr, "voices:          %d\n", h->voices);
	fprintf(stderr, "channels:        %d\n", h->channels);
	fprintf(stderr, "wave_forms:      %d\n", h->wave_forms);
	fprintf(stderr, "master_volume:   0x%04x\n", h->master_volume);
	fprintf(stderr, "data_size:       %d\n", h->data_size);
	fprintf(stderr, "\n");
}

#if 0
struct __attribute__((packed)) INSTRUMENTDATA
{
	uint16_t instrument;
	char instrument_name[16];
	uint32_t instrument_size;
	uint8_t layers;
	char reserved[40];
};
#endif
void dump_instrument(struct INSTRUMENTDATA *i)
{
	fprintf(stderr, "instrument: %d\n", i->instrument);
	fprintf(stderr, "name: %s\n", i->instrument_name);
	fprintf(stderr, "size: %d\n", i->instrument_size);
	fprintf(stderr, "layers: %d\n", i->layers);
	fprintf(stderr, "\n");
}
#if 0
struct __attribute__((packed)) LAYERDATA
{
	uint8_t layer_duplicate;
	uint8_t layer;
	uint32_t layer_size;
	uint8_t samples;
	char reserved[40];
};
#endif
void dump_layer(struct LAYERDATA *l)
{
	fprintf(stderr, "layer_duplicate: %d\n", l->layer_duplicate);
	fprintf(stderr, "layer: %d\n", l->layer);
	fprintf(stderr, "size: %d\n", l->layer_size);
	fprintf(stderr, "samples: %d\n", l->samples);
	fprintf(stderr, "\n");
}

#if 0
struct __attribute__((packed)) PATCHDATA
{
	char wave_name[7];

	uint8_t fractions;
	uint32_t wave_size;
	uint32_t start_loop;
	uint32_t end_loop;

	uint16_t sample_rate;
	uint32_t low_frequency;
	uint32_t high_frequency;
	uint32_t root_frequency;
	uint16_t tune;

	uint8_t balance;

	uint8_t envelope_rate[6];
	uint8_t envelope_offset[6];

	uint8_t tremolo_sweep;
	uint8_t tremolo_rate;
	uint8_t tremolo_depth;

	uint8_t vibrato_sweep;
	uint8_t vibrato_rate;
	uint8_t vibrato_depth;

	/* bit 5 = Turn sustaining on. (Env. pts. 3)*/
	/* bit 6 = Enable envelopes - 1 */
	/* bit 7 = fast release */
	uint8_t modes;

	uint16_t scale_frequency;
	uint16_t scale_factor;    /* from 0 to 2048 or 0 to 2 */

	char reserved[36];
};
#endif
void dump_patch(struct PATCHDATA *p)
{
	fprintf(stderr, "name: %s\n", p->wave_name);
	fprintf(stderr, "fractions:\n");
	fprintf(stderr, "  Loop offset start fractions: %d/16\n", p->fractions&15);
	fprintf(stderr, "  Loop offset end fractions: %d/16\n", (p->fractions>>4)&15);
	fprintf(stderr, "size: %d\n", p->wave_size);
	fprintf(stderr, "start_loop: %d\n", p->start_loop);
	fprintf(stderr, "end_loop:   %d\n", p->end_loop);
	fprintf(stderr, "sample_rate: %d\n", p->sample_rate);
	fprintf(stderr, "low_frequency: %d\n", p->low_frequency);
	fprintf(stderr, "high_frequency: %d\n", p->high_frequency);
	fprintf(stderr, "root_frequency: %d\n", p->root_frequency);
	fprintf(stderr, "tune: %d\n", p->tune);
	fprintf(stderr, "panning: %d\n", p->balance);
	fprintf(stderr, "Envelope rate on Attack:    %d\n", p->envelope_rate[0]);
	fprintf(stderr, "Envelope rate on Decay:     %d\n", p->envelope_rate[1]);
	fprintf(stderr, "Envelope rate on Sustain:   %d\n", p->envelope_rate[2]);
	fprintf(stderr, "Envelope rate off Release:  %d\n", p->envelope_rate[3]);
	fprintf(stderr, "Envelope rate off Release:  %d\n", p->envelope_rate[4]);
	fprintf(stderr, "Envelope rate off Release:  %d\n", p->envelope_rate[5]);
	fprintf(stderr, "Envelope offsets on Attack:    %d\n", p->envelope_rate[0]);
	fprintf(stderr, "Envelope offsets on Decay:     %d\n", p->envelope_rate[1]);
	fprintf(stderr, "Envelope offsets on Sustain:   %d\n", p->envelope_rate[2]);
	fprintf(stderr, "Envelope offsets off Release:  %d\n", p->envelope_rate[3]);
	fprintf(stderr, "Envelope offsets off Release:  %d\n", p->envelope_rate[4]);
	fprintf(stderr, "Envelope offsets off Release:  %d\n", p->envelope_rate[5]);
	fprintf(stderr, "Tremolo sweep: %d\n", p->tremolo_sweep);
	fprintf(stderr, "Tremolo rate: %d\n", p->tremolo_rate);
	fprintf(stderr, "Tremolo depth: %d\n", p->tremolo_depth);
	fprintf(stderr, "Vibrato sweep: %d\n", p->vibrato_sweep);
	fprintf(stderr, "Vibrato rate: %d\n", p->vibrato_rate);
	fprintf(stderr, "Vibrato depth: %d\n", p->vibrato_depth);
	fprintf(stderr, "Modes: (%d)\n", p->modes);
	fprintf(stderr, "  %d-bit\n", (p->modes&1?16:8));
	fprintf(stderr, "  %s\n", (p->modes&2?"Unsigned":"Signed"));
	fprintf(stderr, "  Looping: %d\n", (p->modes&4?1:0));
	fprintf(stderr, "  Pingpong: %d\n", (p->modes&8?1:0));
	fprintf(stderr, "  Reverse: %d\n", (p->modes&16?1:0));
	fprintf(stderr, "  Sustein: %d\n", (p->modes&32?1:0));
	fprintf(stderr, "  Envelope: %d\n", (p->modes&64?1:0));
	fprintf(stderr, "  Clamped release (6th point of envelope): %d\n", (p->modes&128?1:0));
	fprintf(stderr, "Scale frequency: %d\n", p->scale_frequency);
	fprintf(stderr, "Scale factor: %d\n", p->scale_factor);
}

#if 0
static uint32_t pocttab[16]={2044, 4088, 8176, 16352, 32703, 65406, 130813, 261626, 523251, 1046502, 2093005, 4186009, 8372018, 16744036, 33488072, 66976145};
static uint16_t pnotetab[12]={32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858};
static uint16_t pfinetab[16]={32768, 32887, 33005, 33125, 33245, 33365, 33486, 33607, 33728, 33850, 33973, 34095, 34219, 34343, 34467, 34591};
static uint16_t pxfinetab[16]={32768, 32775, 32783, 32790, 32798, 32805, 32812, 32820, 32827, 32835, 32842, 32849, 32857, 32864, 32872, 32879};

static int16_t getnote(uint32_t frq)  /* frq=freq*1000, res=(oct*12+note)*256 (and maybe +fine*16+xfine) */
{
	uint16_t x;
	int i;
	for (i=0; i<15; i++)
		if (pocttab[i+1]>frq)
			break;
	x=(i-1)*12*256;
	frq=umuldiv(frq, 32768, pocttab[i]);
	for (i=0; i<11; i++)
		if (pnotetab[i+1]>frq)
			break;
	x+=i*256;
	frq=umuldiv(frq, 32768, pnotetab[i]);
	for (i=0; i<15; i++)
		if (pfinetab[i+1]>frq)
			break;
	x+=i*16;
	frq=umuldiv(frq, 32768, pfinetab[i]);
	for (i=0; i<15; i++)
		if (pxfinetab[i+1]>frq)
			break;
	return x+i;
}

static uint32_t getfreq(uint16_t note)
{
	int freq=pocttab[note/256/12+1];
	freq=umuldiv(freq, pnotetab[(note/256)%12], 32768);
	freq=umuldiv(freq, pfinetab[(note/16)&0xF], 32768);
	return(umuldiv(freq, pxfinetab[(note)&0xF], 32768));  /* (x-)finetuning not VERY much tested. */
}
#endif

int main(int argc, char *argv[])
{
	FILE *file;
	struct PATCHHEADER    header;
	struct INSTRUMENTDATA ins;
	struct LAYERDATA      layer;
	struct PATCHDATA      patch;
	int i, l, p, d;
	char dummy;

	if (argc!=2)
		return -1;
	if (!(file=fopen(argv[1], "r")))
	{
		perror("fopen()");
		return -1;
	}
	fprintf(stderr, "%s:\n", argv[1]);
	if (fread(&header, sizeof(header), 1, file)!=1)
	{
		perror("fread()");
		return -1;
	}
	PATCHHEADER_endian(&header);
	dump_header(&header);
	for (i=0;i<header.instruments;i++)
	{
		if (fread(&ins, sizeof(ins), 1, file)!=1)
		{
			perror("fread()");
			return -1;
		}
		INSTRUMENTDATA_endian(&ins);
		dump_instrument(&ins);
		for (l=0;l<ins.layers;l++)
		{
			if (fread(&layer, sizeof(layer), 1, file)!=1)
			{
				perror("fread()");
				return -1;
			}
			LAYERDATA_endian(&layer);
			dump_layer(&layer);
			for (p=0;p<layer.samples;p++)
			{
				int fd;
				char t[32];
				snprintf(t, 32, "sample%03d.raw", i++);
				fd = open(t, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR);

				if (fread(&patch, sizeof(patch), 1, file)!=1)
				{
					perror("fread()");
					return -1;
				}
				PATCHDATA_endian(&patch);
				dump_patch(&patch);
				for (d=0;d<patch.wave_size;d++)
				{
					if (fread(&dummy, 1, 1, file)!=1)
					{
						perror("fread()");
						return -1;
					}
					write (fd, &dummy, 1);
				}
				close (fd);
			}
		}
	}
	return 0;
}
