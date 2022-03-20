/* OpenCP Module Player
 * copyright (c) 2019-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for ScreamTracker ][ modules
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
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dev/mcp.h"
#include "filesel/filesystem.h"
#include "gmdplay.h"
#include "stuff/err.h"

/*
  Commands are tested against Scream Tracker v2.3 (stores files as v2.21)
  A Tempo/Speed                     - OK (cmdFineSpeed, cmdSpeed, cmdTempo - OCP has Speed and Tempo in reverse)
  B Break Pattern and Jump to order - OK (Goto, PreConfigures which order next break will jump to)
  C Break Pattern                   - OK (cmdBreak)
  D Volume Slide                    - OK (our wave-device makes ramps, instead of instant volume changes)
  E Portamento down                 - TODO
  F Portamento up                   - TODO
  G Portomento to                   - TODO
  H Vibrato                         - TODO
  I Tremor                          - OK (our wave-device makes ramps, instead of instant volume changes
  J Arpieggo                        - N/A
  K Portamento + volume slide       - N/A
  L Vibrato + volume slide          - N/A
 */

static uint16_t tempo_table[256] =
{
	  10, 1053,  826,  600,  375,  175,    4,    4,    4,    4,    4,    4,    4,    4,    4,    4,
	1256, 1176, 1100, 1025,  951,  876,  801,  726,  625,  550,  475,  400,  325,  250,  175,  100,
	1255, 1231, 1178, 1151, 1105, 1076, 1027, 1001,  952,  901,  875,  826,  802,  751,  727,  676,
	1256, 1256, 1230, 1201, 1178, 1151, 1130, 1101, 1076, 1053, 1027, 1002,  979,  952,  928,  901,
	1255, 1255, 1231, 1231, 1201, 1177, 1178, 1152, 1130, 1130, 1105, 1105, 1076, 1053, 1053, 1027,
	1255, 1255, 1255, 1231, 1231, 1201, 1201, 1178, 1178, 1178, 1151, 1151, 1130, 1130, 1105, 1105,
	1255, 1251, 1251, 1231, 1231, 1225, 1201, 1200, 1178, 1178, 1178, 1151, 1151, 1151, 1130, 1130,
	1255, 1255, 1255, 1255, 1231, 1231, 1231, 1231, 1201, 1201, 1201, 1201, 1178, 1177, 1177, 1178,
	1256, 1255, 1255, 1255, 1256, 1255, 1231, 1231, 1231, 1231, 1231, 1201, 1201, 1201, 1201, 1201,
	1255, 1255, 1255, 1255, 1255, 1255, 1231, 1231, 1231, 1231, 1231, 1201, 1201, 1201, 1201, 1201,
	1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1231, 1231, 1231, 1231, 1231, 1231, 1231, 1231,
	1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1231, 1231, 1231, 1231, 1231, 1231, 1231, 1231,
	1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1231, 1231, 1231, 1231, 1231, 1231, 1231, 1231,
	1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1231, 1231, 1231, 1231, 1231, 1231, 1231, 1231,
	1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255,
	1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255, 1255,
};

struct STMPattern
{
	uint8_t note;
	uint8_t instrument;
	uint8_t volume;
	uint8_t command;
	uint8_t parameters;
};

static inline void putcmd(uint8_t **p, uint8_t c, uint8_t d)
{
	*(*p)++=c;
	*(*p)++=d;
}

static int _mpLoadSTM(struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	unsigned int t,t2,i;
	uint8_t orders[128];
	int ordersn;
	uint32_t smppara[31];
	struct gmdpattern *pp;
	uint8_t temptrack[2000];

	struct __attribute__((packed))
	{
		char name[20];
		char tracker[8];
		uint8_t sig, type, maj, min, tempo, pats, gv;
		uint8_t reserved[13];
	} hdr;

	mpReset(m);

#ifdef STM_LOAD_DEBUG
	fprintf(stderr, "Reading header: %d bytes\n", (int)sizeof(hdr));
#endif

	if (file->read (file, &hdr, sizeof (hdr)) != sizeof (hdr))
	{
		fprintf (stderr, "STM: reading header failed: %s\n", strerror (errno));
		return errFormStruc;
	}

	if (hdr.type != 2)
	{
		fprintf (stderr, "STM: Not a module\n");
		return errFormStruc;
	}

	if (hdr.min <= 10)
	{
		hdr.gv = 64;
	} else if (hdr.gv > 64)
	{
		hdr.gv = 64;
	}

	if (hdr.pats>99)
	{
		fprintf(stderr, "STM: too many patterns\n");
		return errFormStruc;
	}

	if (hdr.pats==0)
	{
		fprintf(stderr, "STM: no patterns\n");
		return errFormStruc;
	}

	m->patnum=hdr.pats;

	memcpy(m->name, hdr.name, 20);
	m->name[20]=0;

	m->channum = 4;

	m->modsampnum=m->sampnum=m->instnum=31;
	if (hdr.min < 21)
	{
		ordersn = 64;
	} else {
		ordersn = 128;
	}
	m->tracknum=hdr.pats*(4+1)+1;
	m->options=MOD_S3M; /* we have Tremor with 00 parameters etc. */
	m->loopord=0;

	/* m->options|=MOD_MODPAN; */

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum))
	{
		fprintf(stderr, "STM: Out of mem?\n");
		return errAllocMem;
	}

	for (i=0; i<31; i++)
	{
		struct gmdinstrument *ip;
		struct gmdsample *sp;
		struct sampleinfo *sip;
		unsigned int j;

		struct __attribute__((packed))
		{
			char dosname[12];
			uint8_t  type;
			uint8_t  disk;
			uint16_t sampptr;
			uint16_t length;
			uint16_t loopstart;
			uint16_t loopend;
			uint8_t volume;
			char d1[1];
			uint16_t c3spd;
			char d2[6];
		} sins;

#ifdef STM_LOAD_DEBUG
		fprintf(stderr, "Reading %d bytes of instrument %02d header (@0x%08x)\n", (int)sizeof(sins), i + 1, (int)file->getpos (file));
#endif
		if (file->read (file, &sins, sizeof (sins)) != sizeof (sins))
		{
			fprintf (stderr, "STM: reading instrument %02d header failed: %s\n", i+1, strerror (errno));
			return errFileRead;
		}

		sins.sampptr   = uint16_little (sins.sampptr);
		sins.length    = uint16_little (sins.length);
		sins.loopstart = uint16_little (sins.loopstart);
		sins.loopend   = uint16_little (sins.loopend);
		sins.c3spd     = uint16_little (sins.c3spd);

		smppara[i]=sins.sampptr<<4;

		ip=&m->instruments[i];
		sp=&m->modsamples[i];
		sip=&m->samples[i];

		ip->name[0]=0;
		if (!sins.length)
			continue;
		if (sins.length > 64000)
		{ /* limitiation of Scream Tracker series */
			sins.length = 64000;
		}
		if (sins.type!=0)
		{
			fprintf (stderr, "STM: warning, non-PCM type sample\n");
		}

		sp->handle=i;
		for (j=0; j<128; j++)
			ip->samples[j]=i;
		memcpy(sp->name, sins.dosname, 12);
		sp->name[12]=0;
		sp->normnote=-mcpGetNote8363(sins.c3spd); // TODO, this seems off
		sp->stdvol=(sins.volume>0x3F)?0xFF:(sins.volume<<2);
		sp->stdpan=-1;
		sp->opt=0;

		if ((sins.loopstart >= sins.loopend) || (sins.loopstart >= sins.length))
		{
			sins.loopstart = 0;
			sins.loopend = 65535;
		} else if ((sins.loopend > sins.length) && (sins.loopend != 65535))
		{
			sins.loopend = sins.length;
		}

		sip->length=sins.length;
		sip->loopstart=sins.loopstart;
		sip->loopend=sins.loopend;
		sip->samprate=8363; // TODO, this seems off
		sip->type=0; //mcpSampUnsigned;

		if (sins.loopstart || (sins.loopend != 65535))
		{
			sip->type |= mcpSampLoop;
		}
	}


#ifdef STM_LOAD_DEBUG
	fprintf(stderr, "Reading pattern orders, %d bytes\n", (int)m->patnum);
#endif
	if (file->read (file, orders, ordersn) != ordersn)
	{
		fprintf (stderr, "STM: reading orders failed: %s\n", strerror (errno));
		return errFileRead;
	}

#if 0
	for (t=m->patnum-1; (signed)t>=0; t--)
	{
		if (orders[t]<254)
			break;
		m->patnum--;
	}
#endif

	t2=0;
	for (t=0; t<ordersn; t++)
	{
		if (orders[t] == 255) /* End of song */
		{
			break;
		}
		if (orders[t]<hdr.pats) /* 99 = Ignore this entry */
		{
			t2 = t+1;
		}
	}

	m->endord = m->ordnum = t2;
	if (!m->ordnum)
	{
		fprintf(stderr, "STM: No orders\n");
		return errFormMiss;
	}

	if (!mpAllocTracks(m, m->tracknum)||!mpAllocPatterns(m, m->patnum)||!mpAllocOrders(m, m->ordnum))
	{
		fprintf(stderr, "STM: Out of mem?\n");
		return errAllocMem;
	}

	for (t=0; t<t2; t++)
	{
		if (orders[t] == 255) /* End of song */
		{
			break;
		}
		if (orders[t]<hdr.pats) /* 99 = Ignore this entry */
		{
			/* does jump hit correct, if we have a hole in the series now? */
			m->orders[t]=orders[t];
		} else {
			m->orders[t]=0xffff;
		}
	}

	for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
	{
		pp->patlen=64;
		for (i=0; i<m->channum; i++)
		{
			pp->tracks[i]=t*(m->channum+1)+i;
		}
		pp->gtrack=t*(m->channum+1)+m->channum;
	}

	for (t=0; t<hdr.pats; t++)
	{
		unsigned int row;
		unsigned int j;
		/* we need to load the full-pattern, since we need to have data sorted by [channel][row], not [row][channel] */
		struct STMPattern pat[64][4];

		for (row=0; row < 64; row++)
		{
			for (j=0; j<4; j++)
			{
				if (ocpfilehandle_read_uint8 (file, &pat[row][j].note))
				{
					fprintf (stderr, "STM: reading pattern data failed: %s\n", strerror (errno));
					return errFileRead;
				}
				switch (pat[row][j].note)
				{
					case 0xfb:
						pat[row][j].note = 0;    pat[row][j].instrument =  0; pat[row][j].volume =  0; pat[row][j].command = 0; pat[row][j].parameters = 0; break;
					case 0xfc:
						pat[row][j].note = 255;  pat[row][j].instrument = 32; pat[row][j].volume = 65; pat[row][j].command = 0; pat[row][j].parameters = 0; break;
					case 0xfd:
						pat[row][j].note = 0xfe; pat[row][j].instrument = 32; pat[row][j].volume = 65; pat[row][j].command = 0; pat[row][j].parameters = 0; break;
					default:
					{
						uint8_t insvol, volcmd;
						if (ocpfilehandle_read_uint8 (file, &insvol))
						{
							fprintf (stderr, "STM: reading pattern data failed: %s\n", strerror (errno));
							return errFileRead;
						}

						if (ocpfilehandle_read_uint8 (file, &volcmd))
						{
							fprintf (stderr, "STM: reading pattern data failed: %s\n", strerror (errno));
							return errFileRead;
						}

						if (ocpfilehandle_read_uint8 (file, &pat[row][j].parameters))
						{
							fprintf (stderr, "STM: reading pattern data failed: %s\n", strerror (errno));
							return errFileRead;
						}

						pat[row][j].instrument = insvol >> 3;
						pat[row][j].volume = (insvol & 0x07) | ((volcmd & 0xf0) >> 1);
						pat[row][j].command = volcmd & 0x0f;
					}
				}
			}
		}

		for (j=0; j<m->channum; j++)
		{
			//char setorgpan=t==orders[0];
			struct gmdtrack *trk;
			uint16_t len;
			uint8_t *tp, *cp;

			tp=temptrack;
			cp=tp+2;

			for (row = 0; row < 64; row++)
			{
				int nte=-1, vol=-1, ins=-1;

				if (!row&&(t==orders[0]))
				{
					putcmd(&cp, cmdVolVibratoSetWave, 0x10);
					putcmd(&cp, cmdPitchVibratoSetWave, 0x10);
				}

				if (pat[row][j].note < 254)
				{
					// TonePortemento behaves differently on STM....
					//if (pat[row][j].command != 7)
					nte=(pat[row][j].note>>4)*12+(pat[row][j].note&0x0F)+36;
				} else if (pat[row][j].note == 254)
				{
					putcmd(&cp, cmdNoteCut, 0);
				}

				if (pat[row][j].volume <= 64)
				{
					vol = (pat[row][j].volume > 0x3f)?0xFF:(pat[row][j].volume<<2);
				}
				if (pat[row][j].instrument)
				{
					ins = pat[row][j].instrument - 1;
				}

				if (pat[row][j].command == 7) /* cmdPitchSlideToNote, so flag this note as a portamento-target instead of a note-on event */
				{
					nte |= 0x80;
				}

				if ((ins!=-1)||(nte!=-1)||(vol!=-1))
				{
					uint8_t *act=cp;
					*cp++=cmdPlayNote;
					if (ins!=-1)
					{
						*act|=cmdPlayIns;
						*cp++=ins;
					}
					if (nte!=-1)
					{
						*act|=cmdPlayNte;
						*cp++=nte;
					}
					if (vol!=-1)
					{
						*act|=cmdPlayVol;
						*cp++=vol;
					}
				}

				switch (pat[row][j].command)
				{
					case 0: /* . */
					case 1: /* A - Speed, global */
					case 2: /* B - Position Jump, global */
					case 3: /* C - Pattern Break, global */
						break;
					case 4: /* D - VolumeSlide */
						if (pat[row][j].parameters & 0x0f)
						{
							putcmd(&cp, cmdVolSlideDown, (pat[row][j].parameters&0x0f)<<2);
						} else {
							putcmd(&cp, cmdVolSlideUp, (pat[row][j].parameters>>4)<<2);
						}
						break;
					case 5: /* E - PortamentoDown */
						putcmd(&cp, cmdRowPitchSlideDown, pat[row][j].parameters);
						break;
					case 6: /* F - PortamentoUp */
						putcmd(&cp, cmdRowPitchSlideUp, pat[row][j].parameters);
						break;
					case 7: /* G - TonePortamento */
						putcmd(&cp, cmdPitchSlideToNote, pat[row][j].parameters);
						break;
					case 8: /* H - Vibrato */
						putcmd(&cp, cmdPitchVibrato, pat[row][j].parameters);
						break;
					case 9: /* I - Tremor */
						putcmd(&cp, cmdTremor, pat[row][j].parameters);
						break;
					case 10: /* J - Arpeggio, not implemented in Scream Tracker 2 */
					case 11: /* K - Vibra, VolumeSlide, not implemented in Scream Tracker 2 */
					case 12: /* L - */
					case 13: /* M - */
					case 14: /* N - */
					case 15: /* O - Tone, VolumeSlide, not implemented in Scream Tracker 2*/
						break;
				}

				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
					cp=tp+2;
				}
			}

			trk=&m->tracks[t*(m->channum+1)+j];
			len=tp-temptrack;

			if (!len)
			{
				trk->ptr=trk->end=0;
			} else {
				trk->ptr=malloc(sizeof(uint8_t)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
				{
					fprintf(stderr, "STM: Out of mem (4) ?\n");
					return errAllocMem;
				}
				memcpy(trk->ptr, temptrack, len);
			}
		}

		do {
			uint8_t *cp, *tp;
			struct gmdtrack *trk;
			uint16_t len;

			tp=temptrack;
			cp=tp+2;

			if (t==orders[0])
			{
				int p = hdr.tempo;

				if (hdr.min < 21)
				{
					p = ((p/10)<<4) | (p % 10);
				}

				if (!p)
				{
					p = 0x60;
				}

				putcmd(&cp, cmdTempo, p>>4);

				putcmd(&cp, cmdSpeed, tempo_table[p] / 10);
				putcmd(&cp, cmdFineSpeed, tempo_table[p]%10);

				if (hdr.gv!=0x40)
				{
					putcmd(&cp, cmdGlobVol, hdr.gv<<2);
				}
			}

			for (row=0; row < 64; row++)
			{
				for (j=0; j<m->channum; j++)
				{
					switch (pat[row][j].command)
					{
						case 0: /* . */
							break;
						case 1: /* A - Speed, global */
							{
								int p = pat[row][j].parameters;
								if (hdr.min < 21)
								{
									p = ((p/10)<<4) | (p % 10);
								}

								if (p)
								{
									putcmd(&cp, cmdTempo, p>>4);

									putcmd(&cp, cmdSpeed, tempo_table[p] / 10);
									putcmd(&cp, cmdFineSpeed, tempo_table[p]%10);
								}
							}
							break;
						case 2: /* B - Position Jump, global */
							putcmd(&cp, cmdGoto, pat[row][j].parameters);
							break;
						case 3: /* C - Pattern Break, global */
							putcmd(&cp, cmdBreak, pat[row][j].parameters);
							break;
						case 4: /* D - VolumeSlide */
						case 5: /* E - PortamentoDown */
						case 6: /* F - PortamentoUp */
						case 7: /* G - TonePortamento */
						case 8: /* H - Vibrato */
						case 9: /* I - Tremor */
						case 10: /* J - Arpeggio, not implemented in Scream Tracker 2 */
						case 11: /* K - Vibra, VolumeSlide, not implemented in Scream Tracker 2 */
						case 12: /* L - */
						case 13: /* M - */
						case 14: /* N - */
						case 15: /* O - Tone, VolumeSlide, not implemented in Scream Tracker 2*/
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

			trk=&m->tracks[t*(m->channum+1)+m->channum];
			len=tp-temptrack;

			if (!len)
			{
				trk->ptr=trk->end=0;
			} else {
				trk->ptr=malloc(sizeof(uint8_t)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
				{
					fprintf(stderr, "STM: Out of mem (6)?\n");
					return errAllocMem;
				}
				memcpy(trk->ptr, temptrack, len);
			}
		} while (0);
	}

	for (i=0; i<m->instnum; i++)
	{
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];
		int l;
		if (sp->handle==0xFFFF)
			continue;

		l=sip->length;
#ifdef STM_LOAD_DEBUG
		fprintf (stderr, "Instrument %d sample, seeking to 0x%08x\n", i+1, smppara[i]);
#endif
		file->seek_set (file, smppara[i]);

		sip->ptr=malloc(sizeof(int8_t)*(l+16));
		if (!sip->ptr)
		{
			fprintf(stderr, "STM: Out of mem (7)?\n");
			return errAllocMem;
		}
		if (file->read (file, sip->ptr, l) != l)
		{
			fprintf(stderr, __FILE__ ": warning, read failed #8\n");
		}
	}

	return errOk;
}

struct gmdloadstruct mpLoadSTM = { _mpLoadSTM };

struct linkinfostruct dllextinfo = {.name = "gmdlstm", .desc = "OpenCP Module Loader: *.STM (c) 2019-'22, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
