/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay PAT format handler (reads GUS patches etc)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "gmipat.h"
#include "gmiplay.h"
#include "dev/mcp.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

__attribute__ ((visibility ("internal"))) uint32_t pocttab[16]={2044, 4088, 8176, 16352, 32703, 65406, 130813, 261626, 523251, 1046502, 2093005, 4186009, 8372018, 16744036, 33488072, 66976145};
__attribute__ ((visibility ("internal"))) uint16_t pnotetab[12]={32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858};
__attribute__ ((visibility ("internal"))) uint16_t pfinetab[16]={32768, 32887, 33005, 33125, 33245, 33365, 33486, 33607, 33728, 33850, 33973, 34095, 34219, 34343, 34467, 34591};
__attribute__ ((visibility ("internal"))) uint16_t pxfinetab[16]={32768, 32775, 32783, 32790, 32798, 32805, 32812, 32820, 32827, 32835, 32842, 32849, 32857, 32864, 32872, 32879};

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

static
int loadsamplePAT( FILE               *file,
                   struct minstrument *ins,
                   uint8_t             j,
                   uint8_t             vox,
                   int                 setnote, /* local boolean */
                   uint8_t             sampnum,
                   uint8_t            *sampused,
                   struct sampleinfo  *sip,
                   uint16_t           *samplenum)
{
	struct msample *s=&ins->samples[j];
	int bit16; /* local boolean */
	struct PATCHDATA sh;
	int q;
	uint8_t *smpp;

	if (fread(&sh, sizeof(sh), 1, file)!=1)
	{
		fprintf(stderr, "[*.PAT loader] fread failed #1\n");
		return errFileRead;
	}
	PATCHDATA_endian(&sh);
	bit16=!!(sh.modes&1);
	if (bit16)
	{
		sh.wave_size>>=1;
		sh.start_loop>>=1;
		sh.end_loop>>=1;
	}

	if (setnote)
	{
		uint8_t lownote,highnote;
		int i;

		lownote=(getnote(sh.low_frequency)+0x80)>>8;
		highnote=(getnote(sh.high_frequency)+0x80)>>8;
		if (highnote>=sizeof(ins->note))
		{
			fprintf(stderr, "[*.PAT loader] highnote to high (sh.high_frequency=%d highnote=%d sizeof(ins->note)=%d\n", sh.high_frequency, highnote, (int)sizeof(ins->note));
			highnote=sizeof(ins->note)-1;
		}
		if (lownote>=sizeof(ins->note))
		{
			fprintf(stderr, "[*.PAT loader] lownote to high (sh.low_requency=%d highnote=%d sizeof(ins->note)=%d\n", sh.low_frequency, highnote, (int)sizeof(ins->note));
			lownote=sizeof(ins->note)-1;
		}
		if (highnote<lownote)
		{
			fprintf(stderr, "[*.PAT loader] highnote is smaller than lownote\n");
			highnote=lownote;
		}
		for (i=lownote; i<highnote; i++)
			if (sampused[i>>3]&(1<<(i&7)))
				break;
		if (i==highnote)
		{
			/* sample not used */
			fseek(file, sh.wave_size<<bit16, SEEK_CUR);
			return 1;
		}
		memset(ins->note+lownote, j, highnote-lownote);
	}

	memcpy(s->name, sh.wave_name, 7);
	s->name[7]=0;
	s->sampnum=sampnum;
	s->handle=-1;
	s->normnote=getnote(sh.root_frequency);
	if ((s->normnote&0xFF)>=0xFE)
		s->normnote=(s->normnote+2)&~0xFF;
	if ((s->normnote&0xFF)<=0x02)
		s->normnote=s->normnote&~0xFF;
	sip->length=sh.wave_size;
	sip->loopstart=sh.start_loop;
	sip->loopend=sh.end_loop;
	sip->samprate=sh.sample_rate;
	sip->type=((sh.modes&4)?(mcpSampLoop|((sh.modes&8)?mcpSampBiDi:0)):0)|(bit16?mcpSamp16Bit:0)|((sh.modes&2)?mcpSampUnsigned:0);
	/* fprintf(stderr, "env: "); */
	for (q=0; q<6; q++)
	{
/*
		s->volrte[q]=((sh.envelope_rate[q]&63)<<(9-3*(sh.envelope_rate[q]>>6)))*1220*16/(64*vox)*2/3; */
		s->volrte[q]=((int)(((((int)sh.envelope_rate[q])&63)*11025))>>((int)3*((int)(sh.envelope_rate[q]>>6))))*14/(int)vox;
		s->volpos[q]=((int)sh.envelope_offset[q])<<8;
/*
		if (q<s->end) fprintf(stderr, "%d:%d ", s->volrte[q], s->volpos[q]); */
	}
	/* fprintf(stderr, "\n"); */

	s->end=(sh.modes&128)?3:6;
	s->sustain=(sh.modes&32)?3:7;
	s->tremswp=((int)sh.tremolo_sweep)*(int)64/45;
	s->vibswp=((int)sh.vibrato_sweep)*(int)64/45;
	s->tremdep=((int)sh.tremolo_depth*4*(int)256)/255   /2;
	s->vibdep=((int)sh.vibrato_depth)*12*(int)256/255   /4;
	s->tremrte=(((int)sh.tremolo_rate)*7+15)*65536/(300*64);
	s->vibrte=(((int)sh.vibrato_rate)*7+15)*65536/(300*64);

/*
	fprintf(stderr, "   -> %d %d %d, %d %d %d\n", s->tremswp, s->tremrte, s->tremdep,
	                                              s->vibswp, s->vibrte, s->vibdep);
 */

	if (sh.scale_factor<=2)
		s->sclfac=((int)sh.scale_factor)<<8;
	else
		s->sclfac=sh.scale_factor>>2;
	s->sclbas=sh.scale_frequency;

	if (!(smpp=calloc(sip->length<<bit16, 1)))
		return errAllocMem;
	if (fread(smpp, 1, sip->length<<bit16, file)!=(sip->length<<bit16))
		fprintf(stderr, "[*.PAT loader] premature EOF (warning)\n");
	sip->ptr=smpp;
	s->handle=(*samplenum)++;
	return errOk;
}

int __attribute__ ((visibility ("internal")))
loadpatchPAT( FILE               *file,
              struct minstrument *ins,
              uint8_t             program,
              uint8_t            *sampused,
              struct sampleinfo **smps,
              uint16_t           *samplenum)
{
	struct PATCHHEADER ph;
	struct INSTRUMENTDATA ih;
	struct LAYERDATA lh;
	int i;
	int cursamp=0;
	uint8_t lowest;

	ins->sampnum=0;
	*ins->name=0;

	if (fread(&ph, sizeof(ph), 1, file) != 1)
	{
		fprintf(stderr, "[*.PAT loader] fread failed #2\n");
		return errFileRead;
	}
	PATCHHEADER_endian(&ph);
	if (memcmp(ph.header, "GF1PATCH110", 12))
	{
		fprintf(stderr, "[*.PAT loader] Invalid header\n");
		return errFormStruc;
	}
	if (ph.instruments<1)
	{
		fprintf(stderr, "[*.PAT loader] Invalid number of instruments\n");
		return errFormStruc;
	}

	if (fread(&ih, sizeof(ih), 1, file) != 1)
	{
		fprintf(stderr, "[*.PAT loader] fread failed #3\n");
		return errFileRead;
	}
	INSTRUMENTDATA_endian(&ih);
	if (ih.layers>1)
	{
		fprintf(stderr, "[*.PAT loader] We don't know how to handle layers (#1 = %d)\n", ih.layers);
		return errFormStruc;
	}
	strcpy(ins->name, ih.instrument_name);
	ins->name[16]=0;
	if (!*ins->name)
	if (midInstrumentNames[program])
	{
		char name[NAME_MAX+1];
		_splitpath(midInstrumentNames[program], 0, 0, name, 0);
		snprintf(ins->name, sizeof(ins->name), "%s", name);
	}

	if (fread(&lh, sizeof(lh), 1, file) != 1)
	{
		fprintf(stderr, "[*.PAT loader] fread failed #4\n");
		return errFileRead;
	}
	LAYERDATA_endian(&lh);
	if (!(ins->samples=calloc(sizeof(struct msample), lh.samples)))
		return errAllocMem;
	if (!(*smps=calloc(sizeof(struct sampleinfo), lh.samples)))
		return errAllocMem;
	ins->sampnum=lh.samples;
	memset(*smps, 0, lh.samples*sizeof(**smps));

	memset(ins->note, 0xFF, 0x80);

	for (i=0; i<ins->sampnum; i++)
	{
		int st=loadsamplePAT(file, ins, cursamp, ph.voices, 1, i, sampused, (*smps)+cursamp, samplenum);
		if (st<errOk)
			return st;
		if (st!=1)
			cursamp++;
	}
	ins->sampnum=cursamp;

	lowest=0xFF;
	for (i=0; i<0x80; i++)
		if (ins->note[i]!=0xFF)
		{
			lowest=ins->note[i];
			break;
		}

	for (i=0; i<0x80; i++)
		if (ins->note[i]!=0xFF)
			lowest=ins->note[i];
		else
			ins->note[i]=lowest;
	return errOk;
}

int __attribute__ ((visibility ("internal")))
addpatchPAT (FILE               *file,
             struct minstrument *ins,
             uint8_t             program,
             uint8_t             sn,
             uint8_t             sampnum,
             struct sampleinfo  *sip,
             uint16_t           *samplenum)
{
	struct msample *s;
	int st;
	struct PATCHHEADER ph;
	struct INSTRUMENTDATA ih;
	struct LAYERDATA lh;

	s=&ins->samples[sn];

	if (fread(&ph, sizeof(ph), 1, file) != 1)
	{
		fprintf(stderr, "[*.PAT loader] fread failed #5\n");
		return errFileRead;
	}
	PATCHHEADER_endian(&ph);
	if (memcmp(ph.header, "GF1PATCH110", 12))
	{
		fprintf(stderr, "[*.PAT loader] Invalid version...\n");
		return errFormStruc;
	}
	if (ph.instruments>1)
	{
		fprintf(stderr, "[*.PAT loader] Invalid number of instruments\n");
		return errFormStruc;
	}
	if (fread(&ih, sizeof(ih), 1, file) != 1)
	{
		fprintf(stderr, "[*.PAT loader] fread failed #6\n");
		return errFileRead;
	}
	INSTRUMENTDATA_endian(&ih);
	if (ih.layers<1)
	{
		int q;
		uint8_t *smpp;
/*
		struct msample s=ins->samples[sn];  overkill */

		strcpy(s->name, "no sample");
		s->handle=-1;
		s->sampnum=sampnum;
		s->normnote=getnote(440000);
		sip->length=1;
		sip->loopstart=0;
		sip->loopend=0;
		sip->samprate=44100;
		sip->type=0;
		for (q=0; q<6; q++)
		{
			s->volpos[q]=0;
			s->volrte[q]=0;
		}
		s->end=1;
		s->sustain=-1;
		s->vibdep=0;
		s->vibrte=0;
		s->vibswp=0;
		s->tremdep=0;
		s->tremrte=0;
		s->tremswp=0;
		s->sclfac=256;
		s->sclbas=60;

		if (!(smpp=malloc(sizeof(uint8_t))))
			return errAllocMem;
		*smpp=0;
		sip->ptr=smpp;
		s->handle=(*samplenum)++;
		return 0;
	}

	if (fread(&lh, sizeof(lh), 1, file) != 1)
	{
		fprintf(stderr, "[*.PAT loader] fread failed #7\n");
		return errFileRead;
	}
	LAYERDATA_endian(&lh);
	if (lh.samples!=1)
	{
		fprintf(stderr, "[*.PAT loader] # Samples != 1\n");
		return errFormStruc;
	}

	if ((st=loadsamplePAT(file, ins, sn, ph.voices, 0, sampnum, 0, sip, samplenum)))
		return st;

	strcpy(s->name, ih.instrument_name);
	s->name[16]=0;
	if (!*s->name)
	{
		char name[NAME_MAX+1];
		_splitpath(midInstrumentNames[program], 0, 0, name, 0);
		snprintf(s->name, sizeof(s->name), "%s", name);
	}

	return errOk;
}
