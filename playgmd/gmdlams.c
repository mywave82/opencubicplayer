/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for AMS (Extreme Tracker / Velvet Studio)
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
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *
 * ENVELOPES & SUSTAIN!!!
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "filesel/filesystem.h"
#include "gmdplay.h"
#include "stuff/err.h"

#ifndef AMS_DEBUG
static void DEBUG_PRINTF (const char *fmt, ...)
{
}
#else
#define DEBUG_PRINTF(...) cpifaceSession->cpiDebug(cpifaceSession, __VA_ARGS__)
#endif

static inline void putcmd(unsigned char **p, unsigned char c, unsigned char d)
{
	*(*p)++=c;
	*(*p)++=d;
}

static const unsigned char envsin[513]=
{

	  0,  1,  2,  2,  3,  4,  5,  5,  6,  7,  8,  8,  9, 10, 11, 12, 12, 13, 14, 15, 16, 16, 17, 18, 19, 19, 20,
	 21, 22, 22, 23, 24, 25, 26, 26, 27, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 39, 39, 40, 41,
	 42, 43, 43, 44, 45, 46, 46, 47, 48, 49, 49, 50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 58, 58, 59, 60, 61, 61,
	 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 72, 73, 74, 74, 75, 76, 76, 77, 78, 79, 79, 80, 81, 82,
	 82, 83, 84, 84, 85, 86, 87, 87, 88, 89, 90, 90, 91, 92, 93, 93, 94, 95, 96, 96, 97, 98, 98, 99,100,100,101,
	102,103,103,104,105,106,106,107,108,108,109,110,110,111,112,113,113,114,115,115,116,117,117,118,119,119,120,
	121,121,122,123,123,124,125,126,126,127,127,128,129,130,130,131,132,132,133,134,134,135,136,136,137,138,138,
	139,139,140,141,141,142,143,143,144,145,145,146,146,147,148,148,149,150,150,151,152,152,153,153,154,155,155,
	156,156,157,158,158,159,160,160,161,161,162,163,163,164,164,165,166,166,167,167,168,168,169,170,170,171,171,
	172,173,173,174,174,175,175,176,177,177,178,178,179,179,180,180,181,181,182,182,183,184,184,185,185,186,186,
	187,187,188,188,189,190,190,191,191,192,192,193,193,194,194,195,195,196,196,197,197,198,198,199,199,200,200,
	201,201,201,202,202,203,203,204,204,205,205,206,206,207,207,207,208,208,209,209,210,210,211,211,211,212,212,
	213,213,214,214,214,215,215,216,216,216,217,217,218,218,219,219,219,220,220,221,221,221,222,222,223,223,223,
	224,224,224,225,225,225,226,226,227,227,227,228,228,228,229,229,229,230,230,230,231,231,231,232,232,232,233,
	233,233,234,234,234,234,235,235,235,236,236,236,237,237,237,237,238,238,238,239,239,239,239,240,240,240,240,
	241,241,241,241,242,242,242,242,243,243,243,243,244,244,244,244,244,245,245,245,245,246,246,246,246,246,247,
	247,247,247,247,248,248,248,248,248,248,249,249,249,249,249,249,250,250,250,250,250,250,250,251,251,251,251,
	251,251,251,252,252,252,252,252,252,252,252,253,253,253,253,253,253,253,253,253,253,253,254,254,254,254,254,
	254,254,254,254,254,254,254,254,254,254,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

struct AMSPatternE
{
	uint8_t fill;
#define FILL_NOTE 1
#define FILL_INSTRUMENT 2
#define FILL_VOLUME 4
#define FILL_PAN 8
#define FILL_DELAYNOTE 16
#define FILL_KEYOFF 32
#define MAX_EFFECTS 4
	uint8_t note;
	uint8_t instrument;
	uint8_t volume;
	uint8_t pan;
	uint8_t delaynote;
	uint8_t effects;
	uint8_t effect[MAX_EFFECTS];    /* how many could the real trackers support? */
	uint8_t parameter[MAX_EFFECTS];
};
struct AMSPatternC
{
	struct AMSPatternE row[256];
};
struct AMSPattern
{
	struct AMSPatternC channel[32];
	unsigned int rowcount;
};
static int readPascalString (struct cpifaceSessionAPI_t *cpifaceSession, struct ocpfilehandle_t *file, char *target, int targetsize, const char *name)
{
	uint8_t stringlen;
	char buffer[256];

	target[0] = 0;
	if (ocpfilehandle_read_uint8 (file, &stringlen))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] reading length of %s failed\n", target);
		return -1;
	}
	if (!stringlen)
	{
		return 0;
	}
	if (stringlen >= targetsize)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] (warning, string length of %s is too long: %d >= %d)\n", name, stringlen, targetsize);
		if (file->read (file, buffer, stringlen) != stringlen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] reading data of %s failed\n", name);
			return -1;
		}
		memcpy (target, buffer, targetsize - 1);
		target[targetsize-1] = 0;
		return 0;
	} else {
		if (file->read (file, target, stringlen) != stringlen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] reading data of %s failed\n", name);
			target[0] = 0;
			return -1;
		}
		target[stringlen] = 0;
		return 0;
	}
}

static int _mpLoadAMS_InstrumentSample (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file, int i, unsigned int *instsampnum, int *sampnum)
{
	int j;
	DEBUG_PRINTF ("[GMD/AMS] Instrument=%d/%d\n", i + 1, m->instnum);

	for (j=0; j<instsampnum[i]; j++)
	{
		struct sampleinfo *sip=&m->samples[(*sampnum)++];
		uint8_t *smpp;

		DEBUG_PRINTF ("[GMD/AMS] Instrument[%d].Sample[%d].Length=0x%08"PRIx32"\n", i, j, sip->length);

		if (!sip->length)
			continue;

		if (!(sip->type & mcpSampRedStereo))
		{ /* not compressed */
			uint32_t length = sip->length << (!!(sip->type & mcpSamp16Bit));
			smpp=malloc(sizeof(unsigned char)*(length+16));
			if (!smpp)
			{
				return errAllocMem;
			}
			if (file->read (file, smpp, length) != length)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning instrument %d/%d sample %d/%d data failed\n", i + 1, m->instnum, j + 1, instsampnum[i]);
				break;
			}

			sip->ptr=smpp;
			m->modsamples[(*sampnum)-1].handle=(*sampnum)-1;
		} else {
			uint32_t packlena,
				 packlenb;
			uint8_t packbyte;
			uint8_t *packb;

			int8_t cursmp;

			unsigned int p1,p2;

			uint8_t bitsel;

			sip->type &= ~mcpSampRedStereo;

			if (ocpfilehandle_read_uint32_le (file, &packlena))
			{
				packlena = 0;
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning, instrument %d/%d sample %d/%d compressedsize failed\n", i + 1, m->instnum, j + 1, instsampnum[i]);
				break;
			}
			if (ocpfilehandle_read_uint32_le (file, &packlenb))
			{
				packlenb = 0;
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning, instrument %d/%d sample %d/%d decompressedsize failed\n", i + 1, m->instnum, j + 1, instsampnum[i]);
				break;
			}
			if (ocpfilehandle_read_uint8 (file, &packbyte))
			{
				packbyte = 0;
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning, instrument %d/%d sample %d/%d packbyte failed\n", i + 1, m->instnum, j + 1, instsampnum[i]);
				break;
			}

			DEBUG_PRINTF ("[GMD/AMS] Instrument[%d].Sample[%d].LengthA=0x%08"PRIx32"\n", i, j, packlena);
			DEBUG_PRINTF ("[GMD/AMS] Instrument[%d].Sample[%d].LengthB=0x%08"PRIx32"\n", i, j, packlenb);
			DEBUG_PRINTF ("[GMD/AMS] Instrument[%d].Sample[%d].PackByt=0x%02"PRIx8"\n", i, j, packbyte);

			if (packlena != sip->length)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning instrument %d/%d sample %d/%d has unexpected length of 0x%04"PRIx32" vs 0x%04"PRIx32"\n", i + 1, m->instnum, j + 1, instsampnum[i], packlena, sip->length);

				packlena = sip->length;
			}
			if ((uint64_t)packlenb > (file->filesize(file) - file->getpos(file)))
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning, instrument %d/%d sample %d/%d has more compressed length than data available, shrinking\n", i + 1, m->instnum, j + 1, instsampnum[i]);
				packlenb = file->filesize(file) - file->getpos(file);
			}

			if (((uint64_t)packlenb * 85) < packlena)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning, instrument %d/%d sample %d/%d has more compressed length smalled than maximum compression-rate, shrinking\n", i + 1, m->instnum, j + 1, instsampnum[i]);
				packlenb = (packlena / 85) + 1;
			}

			packb=malloc(sizeof(unsigned char)*(((packlenb>packlena)?packlenb:packlena)+16));
			smpp=malloc(sizeof(unsigned char)*(packlena+16));
			if (!smpp||!packb)
			{
				return errAllocMem;
			}
			if (file->read (file, packb, packlenb) != packlenb)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning, instrument %d/%d sample %d/%d data failed\n", i + 1, m->instnum, j + 1, instsampnum[i]);

				break;
			}

			p1=p2=0;

			while (p2<packlena)
			{
				if (p1 >= packlenb)
				{
overflowa:
					cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning instrument %d/%d sample %d/%d, ran out of compressed data\n", i + 1, m->instnum, j + 1, instsampnum[i]);
					memset (smpp + p2, 0, packlena - p2);
					break;
				}
				if (packb[p1]!=packbyte)
					smpp[p2++]=packb[p1++];
				else {
					if ((p1 + 1) >= packlenb)
					{
						goto overflowa;
					}
					if (!packb[++p1])
						smpp[p2++]=packb[p1++-1];
					else {
						if (((uint32_t)p2 + packb[p1] > packlena))
						{
							cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning instrument %d/%d sample %d/%d, ran out of target data\n", i + 1, m->instnum, j + 1, instsampnum[i]);

							memset(smpp+p2, packb[p1+1], packlena - p2);
							p2 = packlena;
							p1+=2;
						} else {
							memset(smpp+p2, packb[p1+1], packb[p1]);
							p2+=packb[p1];
							p1+=2;
						}
					}
				}
			}
			if (p1 < packlenb)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] warning, instrument %d/%d sample %d/%d, compressed data left, rewinding buffer (%"PRId32" vs %"PRId32")\n", i + 1, m->instnum, j + 1, instsampnum[i], p1, packlenb);
				file->seek_cur (file, (int64_t)p1 - (int64_t) packlenb);
			}
			memset (packb, 0, packlena);

			p1=0;
			bitsel=0x80;

			for (p2=0; p2<packlena; p2++)
			{
				uint8_t cur=smpp[p2];
				int t;

				for (t=0; t<8; t++)
				{
					packb[p1++]|=cur&bitsel;
					cur=(cur<<1)|((cur&0x80)>>7);
					if (p1==packlena)
					{
						p1=0;
						cur=(cur>>1)|((cur&1)<<7);
						bitsel>>=1;
					}
				}
			}

			cursmp=0;
			p1=0;
			for (p2=0; p2<packlena; p2++)
			{
				uint8_t cur=packb[p2];
				cursmp+=-((cur&0x80)?((-cur)|0x80):cur);
				smpp[p1++]=cursmp;
			}

			free(packb);
		}

		sip->ptr=smpp;
		m->modsamples[(*sampnum)-1].handle=(*sampnum)-1;
	}

	return errOk;
}

static int _mpLoadAMS_ConvertPattern (struct gmdmodule *m, struct ocpfilehandle_t *file, struct AMSPattern *pattern, int patternindex)
{
	struct gmdpattern *pp = &m->patterns[patternindex];
	int curchan;
	unsigned char temptrack[4000]; /* way too big compared to 2+32*2 + 256*(6+2*4) */

	/* configure default panning */
	if (m->orders[0] == patternindex)
	{
		for (curchan=0; curchan < m->channum; curchan++)
		{
			if (!(pattern->channel[curchan].row[0].fill & FILL_PAN))
			{
				pattern->channel[curchan].row[0].pan = (curchan & 1) ? 0xc0 : 0x40;
				pattern->channel[curchan].row[0].fill |= FILL_PAN;
			}
		}
	}

	pp->patlen=pattern->rowcount;
	for (curchan=0; curchan < m->channum; curchan++)
	{
		struct gmdtrack *trk;
		uint16_t len;
		uint8_t *tp, *cp;
		int row;

		tp=temptrack;
		cp=tp+2;

		for (row=0; row<pattern->rowcount; row++)
		{
			int e;
			if (pattern->channel[curchan].row[row].fill & (FILL_NOTE | FILL_INSTRUMENT | FILL_VOLUME | FILL_PAN | FILL_DELAYNOTE))
			{
				uint8_t *act = cp;
				*cp++=cmdPlayNote;
				if (pattern->channel[curchan].row[row].fill & FILL_INSTRUMENT)
				{
					*act|=cmdPlayIns;
					*cp++=pattern->channel[curchan].row[row].instrument;
				}
				if (pattern->channel[curchan].row[row].fill & FILL_NOTE)
				{
					*act|=cmdPlayNte;
					*cp++=pattern->channel[curchan].row[row].note;
				}
				if (pattern->channel[curchan].row[row].fill & FILL_VOLUME)
				{
					*act|=cmdPlayVol;
					*cp++=pattern->channel[curchan].row[row].volume;
				}
				if (pattern->channel[curchan].row[row].fill & FILL_PAN)
				{
					*act|=cmdPlayPan;
					*cp++=pattern->channel[curchan].row[row].pan;
				}
				if (pattern->channel[curchan].row[row].fill & FILL_DELAYNOTE)
				{
					*act|=cmdPlayDelay;
					*cp++=pattern->channel[curchan].row[row].delaynote;
				}
			}
			if (pattern->channel[curchan].row[row].fill & FILL_KEYOFF)
			{
				putcmd(&cp, cmdKeyOff, 0);
			}

			for (e=0; e < pattern->channel[curchan].row[row].effects; e++)
			{
				uint8_t cmd = pattern->channel[curchan].row[row].effect[e];
				uint8_t data = pattern->channel[curchan].row[row].parameter[e];
				switch (cmd)
				{
					case 0x00:
						if (data)
							putcmd(&cp, cmdArpeggio, data);
						break;

					case 0x01:
						putcmd(&cp, cmdPitchSlideUp, data);
						break;

					case 0x02:
						putcmd(&cp, cmdPitchSlideDown, data);
						break;

					case 0x03:
						putcmd(&cp, cmdPitchSlideToNote, data);
						break;

					case 0x04:
						putcmd(&cp, cmdPitchVibrato, data);
						break;

					case 0x05:
						putcmd(&cp, cmdPitchSlideToNote, 0);
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						else if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;

					case 0x06:
						putcmd(&cp, cmdPitchVibrato, 0);
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						else if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;

					case 0x07:
						putcmd(&cp, cmdVolVibrato, data);
						break;
#if 0 /* handled above */
					case 0x08: /* 08 0x - PanPosition (0-f) */
						if (!(data & 0xf0))
						{
							putcmd(&cp, cmdPlayNote|cmdPlayPan, data);
						}
						break;
#endif
					case 0x09:
						putcmd(&cp, cmdOffset, data);
						break;

					case 0x0a:
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContMixVolSlide);
						else if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;

					case 0x0e:
						cmd = data >> 4;
						data = data & 0x0f;
						switch (cmd)
						{
							case 0x1:
								putcmd(&cp, cmdRowPitchSlideUp, data<<4);
								break;
							case 0x2:
								putcmd(&cp, cmdRowPitchSlideDown, data<<4);
								break;
							case 0x3:
								switch (data)
								{
									case 0x0:
										putcmd(&cp, cmdGlissOff, 0);
										break;
									case 0x1:
										putcmd(&cp, cmdGlissOn, 0);
										break;
								}
								break;
							case 0x4:
								if (data < 4)
								{
									putcmd(&cp, cmdPitchVibratoSetWave, data);
								}
								/* FIXME 0x44 Don't retrig WaveForm ???? */
								/* FIXME 0x45 Don't retrig WaveForm ???? */
								/* FIXME 0x46 Don't retrig WaveForm ???? */
								break;
							case 0x5:
								/* FIXME FineTune */
								break;
							case 0x7: /* FIXME Tremolo WaveForm ??? */
								if (data < 4)
								{
									putcmd(&cp, cmdVolVibratoSetWave, data);
								}
								break;
							case 0x8: /* 0E 80 Break Sample Loop */
								if (data==0x0)
									putcmd(&cp, cmdSetLoop, 0);
								break;
							case 0x9:
								if (data)
									putcmd(&cp, cmdRetrig, data);
								break;
							case 0xa:
								putcmd(&cp, cmdRowVolSlideUp, data<<2);
								break;
							case 0xb:
								putcmd(&cp, cmdRowVolSlideDown, data<<2);
								break;
							case 0xc:
								putcmd(&cp, cmdNoteCut, data);
								break;
#if 0 /* handled above */
							case 0xd:
								putcmd(&cp, cmdPlayNote | cmdPlayDelay, data);
								break;
#endif
						}
						break;

					case 0x10:
						switch (data)
						{
							case 0x00: /* 10 00 Play Sample Forward */
								putcmd(&cp, cmdSetDir, 0);
								break;
							case 0x01: /* 10 01 Play Sample BackWards */
								putcmd(&cp, cmdSetDir, 1);
								break;
							case 0x02: /* 10 02 Enable BiDirectional Loop (only on looped samples) */
								#warning We do not have a cmd yet to make a looped sample into a bidir looped
								break;
						}
						break;

					case 0x11: /* 11 xx Extra Fine Slide Up (4x precision) */
						putcmd(&cp, cmdRowPitchSlideUp, data<<2);
						break;

					case 0x12: /* 12 xx Extra Fine Slide Down (4x precision) */
						putcmd(&cp, cmdRowPitchSlideDown, data<<2);
						break;

					case 0x13: /* Retrig with volslide */
#if 0
						putcmd(&cp, cmdRetrig, data & 0x0f);
						switch (data & 0xf0)
						{
							case 0x10: putcmd(&cp, cmdVolSlideDown, 1); break;
							case 0x20: putcmd(&cp, cmdVolSlideDown, 2); break;
							case 0x30: putcmd(&cp, cmdVolSlideDown, 4); break;
							case 0x40: putcmd(&cp, cmdVolSlideDown, 8); break;
							case 0x50: putcmd(&cp, cmdVolSlideDown, 16); break;
#warning We do not have a cmd yet to set volume to 2/3 of the original volume
							//case 0x60: putcmd(&cp, cmdVol2_3, 0); break;
#warning We do not have a cmd yet to set volume to 1/2 of the original volume
							//case 0x70: putcmd(&cp, cmdVol1_2, 0); break;

							case 0x90: putcmd(&cp, cmdVolSlideUp, 1); break;
							case 0xa0: putcmd(&cp, cmdVolSlideUp, 2); break;
							case 0xb0: putcmd(&cp, cmdVolSlideUp, 3); break;
							case 0xc0: putcmd(&cp, cmdVolSlideUp, 8); break;
							case 0xd0: putcmd(&cp, cmdVolSlideUp, 16); break;
#warning We do not have a cmd yet to set volume to 3/2 of the original volume
							//case 0xe0: putcmd(&cp, cmdVol3_2, 1); break;
#warning We do not have a cmd yet to set volume to 2/1 of the original volume
							//case 0xf0: putcmd(&cp, cmdVol2, 1); break;
						}
#else
						putcmd(&cp, cmdRetrig, data);
#endif
						break;

					case 0x15: /* 15 xx - Just Like 05, but with 2 times finer volslide. */
						putcmd(&cp, cmdPitchSlideToNote, 0);
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						else if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
						else
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
						break;

					case 0x16: /* 16 xx - Just Like 06, but with 2 times finer volslide. */
						putcmd(&cp, cmdPitchVibrato, 0);
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						else if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
						else
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
						break;

					case 0x18: /* 18 xy - Slide PanPosition either left (x) or right (y) */
						if (!data)
							putcmd(&cp, cmdPanSlide, 0);
						else if (data&0xF0)
							putcmd(&cp, cmdPanSlide, data>>4);
						else
							putcmd(&cp, cmdPanSlide, -(data&0xF));
						/*
						if ((data&0x0F)&&(data&0xF0))
							break;
						if (data&0xF0)
							data=-(data>>4)*4;
						else
							data=data*4;
						putcmd(cp, cmdPanSlide, data);
						*/
						break;

					case 0x1a: /* 1a xx - 2 times finer volslide than A. */
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						else
							if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
						break;

					case 0x1c: /* 1c xx - Channel Master Volume */
						putcmd(&cp, cmdChannelVol, (data<=0x7F)?(data<<1):0xFF);
						break;

					case 0x1e:
						cmd = data >> 4;
						data = data & 0x0f;
						switch (data)
						{
							case 0x1: /* 1E 1x - Just like E1, but this uses all octaves. */
								putcmd(&cp, cmdRowPitchSlideUp, data<<4);
								break;
							case 0x2: /* 1E 2x - Just like E1, but this uses all octaves. */
								putcmd(&cp, cmdRowPitchSlideDown, data<<4);
								break;
							case 0xa: /* 1E Ax - 2 times Finer volslide than EA. */
								putcmd(&cp, cmdRowVolSlideUp, data<<1);
								break;
							case 0xb: /* 1E Bx   2 times Finer volslide than EB. */
								putcmd(&cp, cmdRowVolSlideDown, data<<1);
								break;
						}
						break;

					case 0x20: /* 20 xx - KeyOff at Tick */
						putcmd(&cp, cmdKeyOff, data);
						break;

					case 0x21: /* 21 xx - Just like 1, but this uses all octaves. */
#warning We do not have a cmd yet to do wider scale
						putcmd(&cp, cmdPitchSlideUp, data);
						break;

					case 0x22: /* 22 xx - Just like 2, but this uses all octaves. */
#warning We do not have a cmd yet to do wider scale
						putcmd(&cp, cmdPitchSlideDown, data);
						break;
				}
			}

			if (cp!=(tp+2))
			{
				tp[0]=row;
				tp[1]=cp-tp-2;
				tp=cp;
				cp=tp+2;
			}
		}

		pp->tracks[curchan] = patternindex * (m->channum + 1) + curchan;
		trk=&m->tracks[pp->tracks[curchan]];
		len=tp-temptrack;

		if (!len)
		{
			trk->ptr=trk->end=0;
		} else {
			trk->ptr=malloc(sizeof(uint8_t)*len);
			trk->end=trk->ptr+len;
			if (!trk->ptr)
			{
				return errAllocMem;
			}
			memcpy(trk->ptr, temptrack, len);
		}
	}

	do {
		struct gmdtrack *trk;
		uint16_t len;
		uint8_t *tp, *cp;
		int row;

		tp=temptrack;
		cp=tp+2;

		for (row=0; row<pattern->rowcount; row++)
		{
			for (curchan=0; curchan<m->channum; curchan++)
			{
				int e;
				for (e=0; e < pattern->channel[curchan].row[row].effects; e++)
				{
					uint8_t cmd = pattern->channel[curchan].row[row].effect[e];
					uint8_t data = pattern->channel[curchan].row[row].parameter[e];
					switch (cmd)
					{
						case 0x0b:
							putcmd(&cp, cmdGoto, data); break;
						case 0x0d:
							putcmd(&cp, cmdBreak, (data&0x0F)+(data>>4)*10); break;
						case 0x0e:
							cmd = data >> 4;
							data &= 0x0f;
							switch (cmd)
							{
								case 0x6:
									putcmd(&cp, cmdSetChan, curchan);
									putcmd(&cp, cmdPatLoop, data);
									break;
								case 0xe:
									putcmd(&cp, cmdPatDelay, data);
									break;
							}
							break;
						case 0x0f:
							if (data)
							{
								if (data<0x20)
								{
									putcmd(&cp, cmdTempo, data);
								} else {
									putcmd(&cp, cmdSpeed, data);
								}
							} else {
								putcmd(&cp, cmdGoto, 0);
							}
							break;
						case 0x1d:
							putcmd(&cp, cmdBreak, data); break;
						case 0x1f:
							if (data<10)
							{
								putcmd(&cp, cmdFineSpeed, data);
							}
							break;
						case 0x2a: /* 2a xy - Global Volumeslide (X up, Y down) */
							if (!((data&0x0f)&&(data&0xf0)))
							{
								putcmd(&cp, cmdSetChan, curchan);
								if (data&0xF0)
								{
									putcmd(&cp, cmdGlobVolSlide, (data>>4)<<2);
								} else {
									putcmd(&cp, cmdGlobVolSlide, -(data<<2));
								}
							}
							break;
						case 0x2c: /* 2c xx - Set global volume */
							putcmd(&cp, cmdGlobVol, data*2);
							break;
					}
				}
			}

			if (cp!=(tp+2))
			{
				tp[0]=row;
				tp[1]=cp-tp-2;
				tp=cp;
				cp=tp+2;
			}
		}

		pp->gtrack = patternindex * (m->channum + 1) + m->channum;
		trk=&m->tracks[pp->gtrack];
		len=tp-temptrack;

		if (!len)
		{
			trk->ptr=trk->end=0;
		} else {
			trk->ptr=malloc(sizeof(uint8_t)*len);
			trk->end=trk->ptr+len;
			if (!trk->ptr)
			{
				return errAllocMem;
			}
			memcpy(trk->ptr, temptrack, len);
		}
	} while (0);

	return errOk;
}

#include "gmdlams-v1.c"
#include "gmdlams-v2.c"

OCP_INTERNAL int LoadAMS (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	unsigned char sig[7];

	if (file->read (file, &sig, 7) != 7)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS] read failed #1 (signature)\n");
	}
	if (!memcmp(sig, "Extreme", 7))
	{
		return _mpLoadAMS_v1 (cpifaceSession, m, file);
	}
	if (!memcmp(sig, "AMShdr\x1A", 7))
	{
		return _mpLoadAMS_v2 (cpifaceSession, m, file);
	}

	return errFormSig;
}
