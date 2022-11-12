/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay .XM module loader
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 *    -added module flag "ismod" to handle some protracker quirks
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "xmplay.h"

struct LoadModuleResources
{
	struct sampleinfo **smps;
	struct xmpsample **msmps;
	unsigned int *instsmpnum;
};

static void FreeResources(struct LoadModuleResources *r, uint_fast16_t ninst)
{
	int l;
	if (r->smps)
	{
		for (l=0;l<ninst;l++)
			if (r->smps[l])
				free(r->smps[l]);
		free(r->smps);
		r->smps = 0;
	}
	if (r->msmps)
	{
		for (l=0;l<ninst;l++)
			if (r->msmps[l])
				free(r->msmps[l]);
		free(r->msmps);
		r->msmps = 0;
	}
	if (r->instsmpnum)
	{
		free(r->instsmpnum);
		r->instsmpnum = 0;
	}
}


int __attribute__ ((visibility ("internal"))) xmpLoadModule(struct xmodule *m, struct ocpfilehandle_t *file)
{
	struct __attribute__((packed))
	{
		int8_t sig[17];
		int8_t name[20];
		int8_t eof;
		int8_t tracker[20];
		uint16_t ver;
		uint32_t hdrsize;
	} head1;

	struct __attribute__((packed))
	{
		uint16_t nord;
		uint16_t loopord;
		uint16_t nchan;
		uint16_t npat;
		uint16_t ninst;
		uint16_t freqtab;
		uint16_t tempo;
		uint16_t bpm;
		uint8_t ord[256];
	} head2;

	unsigned int i,j,k;

	struct LoadModuleResources r;

	r.smps = 0;
	r.msmps = 0;
	r.instsmpnum = 0;

	m->envelopes=0;
	m->samples=0;
	m->instruments=0;
	m->sampleinfos=0;
	m->patlens=0;
	m->patterns=0;
	m->orders=0;
	m->ismod=0;
	m->ft2_e60bug=1;

	if (file->read (file, &head1, sizeof(head1)) != sizeof (head1))
	{
		fprintf(stderr, __FILE__ ": fread failed #1\n");
		FreeResources (&r, head2.ninst);
		return errFileRead;
	}
	head1.ver     = uint16_little (head1.ver);
	head1.hdrsize = uint32_little (head1.hdrsize);
	if (memcmp(head1.sig, "Extended Module: ", 17))
	{
		fprintf(stderr, __FILE__ ": Malformed header (\"Extended Module: \" string missing)\n");
		FreeResources (&r, head2.ninst);
		return errFormStruc;
	}
	if (head1.eof!=26)
	{
		fprintf(stderr, __FILE__ ": Malformed header (head1.eof!=26), trying to ignore\n");
		/*
		FreeResources (&r, head2.ninst);
		return errFormStruc;
		*/
	}
	if (head1.ver<0x104)
	{
		fprintf(stderr, __FILE__ ": File too old\n");
		FreeResources (&r, head2.ninst);
		return errFormOldVer;
	}

	if (file->read (file, &head2, sizeof(head2)) != sizeof (head2))
	{
		fprintf(stderr, __FILE__ ": fread failed #2\n");
		FreeResources (&r, head2.ninst);
		return errFileRead;
	}
	head2.nord    = uint16_little (head2.nord);
	head2.loopord = uint16_little (head2.loopord);
	head2.nchan   = uint16_little (head2.nchan);
	head2.npat    = uint16_little (head2.npat);
	head2.ninst   = uint16_little (head2.ninst);
	head2.freqtab = uint16_little (head2.freqtab);
	head2.tempo   = uint16_little (head2.tempo);
	head2.bpm     = uint16_little (head2.bpm);
	if (file->seek_cur (file, head1.hdrsize - 4 - sizeof(head2)) < 0)
	{
		fprintf(stderr, __FILE__ ": fseek failed #1\n");
		FreeResources (&r, head2.ninst);
		return errFormStruc;
	}

	if (!head2.ninst)
	{
		fprintf(stderr, __FILE__ ": No instruments\n");
		FreeResources (&r, head2.ninst);
		return errFormMiss;
	}

	memcpy(m->name, head1.name, 20);
	m->name[20]=0;

	m->linearfreq=!!(head2.freqtab&1);
	m->nchan=head2.nchan;
	m->ninst=head2.ninst;
	m->nenv=head2.ninst*2;
	m->npat=head2.npat+1;
	m->nord=head2.nord;
	m->loopord=head2.loopord;
	m->inibpm=head2.bpm;
	m->initempo=head2.tempo;

	m->orders=malloc(sizeof(uint16_t)*head2.nord);
	m->patterns=(uint8_t (**)[5])calloc(sizeof(void *), (head2.npat+1));
	m->patlens=malloc(sizeof(uint16_t)*(head2.npat+1));
	m->instruments=malloc(sizeof(struct xmpinstrument)*head2.ninst);
	m->envelopes=calloc(sizeof(struct xmpenvelope), head2.ninst*2);
	r.smps=calloc(sizeof(struct sampleinfo *), head2.ninst);
	r.msmps=calloc(sizeof(struct xmpsample *), head2.ninst);
	r.instsmpnum=malloc(sizeof(int)*head2.ninst);

	if (!r.smps||!r.msmps||!r.instsmpnum||!m->instruments||!m->envelopes||!m->patterns||!m->orders||!m->patlens)
	{
		fprintf(stderr, __FILE__ ": malloc failed #1 (debug %p %p %p %p %p %p %p %p)\n", r.smps, r.msmps, r.instsmpnum, m->instruments, m->envelopes, m->patterns, m->orders, m->patlens);
		FreeResources (&r, head2.ninst);
		return errAllocMem;
	}

	for (i=0; i<m->nchan; i++)
		m->panpos[i]=0x80;

	for (i=0; i<head2.nord; i++)
		m->orders[i]=(head2.ord[i]<head2.npat)?head2.ord[i]:head2.npat;

	m->patlens[head2.npat]=64;
	m->patterns[head2.npat]=calloc(sizeof(uint8_t), 64*head2.nchan*5);
	if (!m->patterns[head2.npat])
	{
		fprintf(stderr, __FILE__ ": malloc failed #1 (size=%d)\n", (int)sizeof(uint8_t)*64*head2.nchan*5);
		FreeResources (&r, head2.ninst);
		return errAllocMem;
	}

	for (i=0; i<head2.npat; i++)
	{
		struct __attribute__((packed))
		{
			uint32_t len;
			uint8_t ptype;
			uint16_t rows;
			uint16_t patdata;
		} pathead;
		uint8_t *pbuf, *pbp, *cur;

		if (file->read (file, &pathead, sizeof (pathead)) != sizeof (pathead))
		{
			fprintf(stderr, __FILE__ ": fread failed #3\n");
			FreeResources (&r, head2.ninst);
			return errFileRead;
		}
		pathead.len     = uint32_little (pathead.len);
		pathead.rows    = uint16_little (pathead.rows);
		pathead.patdata = uint16_little (pathead.patdata);
		if (file->seek_cur (file, pathead.len - sizeof(pathead)))
		{
			fprintf(stderr, __FILE__ ": fseek failed #2\n");
			FreeResources (&r, head2.ninst);
			return errFormStruc;
		}
		m->patlens[i]=pathead.rows;
		m->patterns[i]=calloc(sizeof(uint8_t), pathead.rows*head2.nchan*5);
		if (!m->patterns[i])
		{
			fprintf(stderr, __FILE__ ": malloc failed #3 (i=%d/%d, size=%d)\n", i, head2.npat, (int)sizeof(uint8_t)*pathead.rows*head2.nchan*5);
			FreeResources (&r, head2.ninst);
			return errAllocMem;
		}
		if (!pathead.patdata)
			continue;
		pbp=pbuf=malloc(sizeof(uint8_t)*pathead.patdata);
		if (!pbuf)
		{
			fprintf(stderr, __FILE__ ": malloc failed #4 (i=%d/%d, size=%d)\n", i, head2.npat, (int)sizeof(uint8_t)*pathead.patdata);
			FreeResources (&r, head2.ninst);
			return errAllocMem;
		}
		if (file->read (file, pbuf, pathead.patdata) != pathead.patdata)
		{
			fprintf(stderr, __FILE__ ": fread failed #4\n");
			FreeResources (&r, head2.ninst);
			return errFileRead;
		}
		cur=(uint8_t *)(m->patterns[i]);
		for (j=0; j<((unsigned int)pathead.rows*head2.nchan); j++)
		{
			uint8_t pack=(*pbp&0x80)?(*pbp++):0x1F;
			for (k=0; k<5; k++)
			{
				*cur++=(pack&1)?*pbp++:0;
				pack>>=1;
			}
			if (cur[-2]==0xE)
			{
				cur[-2]=36+(cur[-1]>>4);
				cur[-1]&=0xF;
			}
		}
		free(pbuf);
	}

	m->nsampi=0;
	m->nsamp=0;
	for (i=0; i<m->ninst; i++)
	{
		struct xmpinstrument *ip=&m->instruments[i];
		struct xmpenvelope *env=m->envelopes+2*i;
		struct __attribute__((packed))
		{
			uint32_t size;
			uint8_t name[22];
			uint8_t type;
			uint16_t samp;
		} ins1;
		struct __attribute__((packed))
		{
			uint32_t shsize;
			uint8_t snum[96];
			uint16_t venv[12][2];
			uint16_t penv[12][2];
			uint8_t vnum, pnum;
			uint8_t vsustain, vloops, vloope, psustain, ploops, ploope;
			uint8_t vtype, ptype;
			uint8_t vibtype, vibsweep, vibdepth, vibrate;
			uint16_t volfade;
			/*uint16_t res; this is variable size*/
		} ins2;
		uint16_t volfade;
		int k, j;

		if (file->read (file, &ins1, sizeof (ins1.size)) != sizeof (ins1.size))
		{
			/*
			fprintf(stderr, __FILE__ ": fread failed #5.1 (%d/%d)\n", i, m->ninst);
			FreeResources (&r, head2.ninst);
			return errFormStruc;
			*/
			fprintf(stderr, __FILE__ ": warning, fread failed #5.1\n");
			ins1.size=0;
		}
		ins1.size = uint32_little (ins1.size);
		/* this next block is a fucking hack to support SMALL files */
		if (ins1.size<4)
		{
			fprintf(stderr, __FILE__ ": Warning, ins1.size<4\n");
			memset(&ins1, 0, sizeof(ins1));
		} else if (ins1.size < sizeof(ins1))
		{
			fprintf(stderr, __FILE__ ": Warning, ins1.size<=sizeof(ins1), reading %d more bytes and zeroing last %d bytes\n", (int)(ins1.size-(int)sizeof(ins1.size)), (int)(sizeof(ins1)-(ins1.size-sizeof(ins1.size))));
			if (file->read (file, ins1.name, ins1.size - sizeof (ins1.size)) != (ins1.size - sizeof (ins1.size)))
			{
				fprintf(stderr, __FILE__ ": fread failed #5.2\n");
				FreeResources (&r, head2.ninst);
				return errFileRead;
			}
			memset(ins1.name + ins1.size-sizeof(ins1.size), 0, sizeof(ins1)-(ins1.size-sizeof(ins1.size)));
		} else {
			if (file->read (file, ins1.name, sizeof (ins1) - sizeof (ins1.size)) != (sizeof (ins1) - sizeof (ins1.size)))
			{
				fprintf(stderr, __FILE__ ": fread failed #5.2\n");
				FreeResources (&r, head2.ninst);
				return errFormStruc;
			}
		}

		ins1.samp = uint16_little (ins1.samp);

		memcpy(ip->name, ins1.name, 22);
		ip->name[22]=0;
		memset(ip->samples, 0xff, 2*128);

		r.instsmpnum[i]=ins1.samp;
		if (!ins1.samp)
		{
			if (ins1.size > sizeof(ins1))
			{
				if (file->seek_cur (file, ins1.size - sizeof(ins1)) < 0)
				{
					fprintf(stderr, __FILE__ ": fseek failed #3\n");
					FreeResources (&r, head2.ninst);
					return errFormStruc;
				}
			}
			continue;
		}

		/* this next block is a fucking hack to support SMALL files*/
		if (ins1.size<=sizeof(ins1))
		{
			fprintf(stderr, __FILE__ ": Warning, ins1.size<=sizeof(ins1), zeroing ins2\n");
			memset(&ins2, 0, sizeof(ins2));
		} else if (ins1.size<=(sizeof(ins1)+sizeof(ins2)))
		{
			fprintf(stderr, __FILE__ ": Warning, ins2.size<=(sizeof(ins1)+sizeof(ins2)), reading %d bytes and zeroing last %d bytes\n", (int)(ins1.size-(int)sizeof(ins1)), (int)(sizeof(ins2)-(ins1.size-sizeof(ins1))));
			if (file->read (file, &ins2, ins1.size - sizeof (ins1)) != (ins1.size - sizeof (ins1)))
			{
				fprintf(stderr, __FILE__ ": fread failed #6.1\n");
				FreeResources (&r, head2.ninst);
				return errFormStruc;
			}
			memset((char *)(&ins2)+ins1.size-sizeof(ins1), 0, sizeof(ins2)-(ins1.size-sizeof(ins1)));
		} else {
			if (file->read (file, &ins2, sizeof (ins2)) != sizeof (ins2))
			{
				fprintf(stderr, __FILE__ ": fread failed #6.2 (Failed to read instrument %d/%d)\n", i + 1, m->ninst);
				FreeResources (&r, head2.ninst);
				return errFormStruc;
			}
			if (file->seek_cur (file, ins1.size - sizeof(ins1) - sizeof(ins2)) < 0)
			{
				fprintf(stderr, __FILE__ ": fseek failed #4 (Failed to seek past extra data in instrument %d/%d)\n", i + 1, m->ninst);
				FreeResources (&r, head2.ninst);
				return errFormStruc;
			}
		}

		ins2.shsize = uint32_little (ins2.shsize);
		if (ins2.shsize==0)
		{
			fprintf(stderr, __FILE__ ": warning, ins2 header size is zero, setting to 0x28\n");
			ins2.shsize=0x28;
		}
		if (ins2.vnum>12)
		{
			fprintf(stderr, __FILE__ ": warning, Number of volume points %d > 12, truncating\n", ins2.vnum);
			ins2.vnum=12;
		}
		if (ins2.pnum>12)
		{
			fprintf(stderr, __FILE__ ": warning, Number of panning points %d > 12, truncating\n", ins2.pnum);
			ins2.pnum=12;
		}
		if ((ins2.vtype&1)&&!ins2.vnum)
		{
			fprintf(stderr, __FILE__ ": warning, No volume envelopes!\n");
			ins2.vnum=1;
		}
		if ((ins2.ptype&1)&&!ins2.pnum)
		{
			fprintf(stderr, __FILE__ ": warning, No volume envelopes!\n");
			ins2.pnum=1;
		}
		if (ins2.vsustain&&(ins2.vsustain>=ins2.vnum))
		{
			fprintf(stderr, __FILE__ ": warning, Volume sustain point (%d) >= Number of volume point (%d), truncating\n", ins2.vsustain, ins2.vnum);
			ins2.vsustain=ins2.vnum-1;
		}
		if (ins2.vloops&&(ins2.vloops>=ins2.vnum))
		{
			fprintf(stderr, __FILE__ ": warning, Volume loop start point (%d) >= Number of volume point (%d), truncating\n", ins2.vloops, ins2.vnum);
			ins2.vloops=ins2.vnum-1;
		}
		if (ins2.vloope&&(ins2.vloope>=ins2.vnum))
		{
			fprintf(stderr, __FILE__ ": warning, Volume loop end point (%d) >= Number of volume point (%d), truncating\n", ins2.vloope, ins2.vnum);
			ins2.vloope=ins2.vnum-1;
		}
		if (ins2.psustain&&(ins2.psustain>=ins2.pnum))
		{
			fprintf(stderr, __FILE__ ": warning, Panning sustain point (%d) >= Number of panning points (%d), truncating\n", ins2.psustain, ins2.pnum);
			ins2.psustain=ins2.pnum-1;
		}
		if (ins2.ploops&&(ins2.ploops>=ins2.pnum))
		{
			fprintf(stderr, __FILE__ ": warning, Panning loop start point (%d) >= Number of panning points (%d), truncating\n", ins2.ploops, ins2.pnum);
			ins2.ploops=ins2.pnum-1;
		}
		if (ins2.ploope&&(ins2.ploope>=ins2.pnum))
		{
			fprintf(stderr, __FILE__ ": warning, Panning loop end point (%d) >= Number of panning points (%d), truncating\n", ins2.ploope, ins2.pnum);
			ins2.ploope=ins2.pnum-1;
		}
		for (k=0;k<12;k++)
			for (j=0;j<2;j++)
			{
				ins2.venv[j][k] = uint16_little (ins2.venv[j][k]);
				ins2.penv[j][k] = uint16_little (ins2.penv[j][k]);
			}
		ins2.volfade = uint16_little (ins2.volfade);
		/*ins2.res     = uint16_little (ins2.res);*/
		r.smps[i]=calloc(sizeof(struct sampleinfo), ins1.samp);
		r.msmps[i]=calloc(sizeof(struct xmpsample), ins1.samp);
		if (!r.smps[i]||!r.msmps[i])
		{
			fprintf(stderr, __FILE__ ": malloc failed #5 (i=%d/%d, %p(%d) %p(%d))\n", i, m->ninst, r.smps[i], (int)sizeof(struct sampleinfo)*ins1.samp, r.msmps[i], (int)sizeof(struct xmpsample)*ins1.samp);
			FreeResources (&r, head2.ninst);
			return errAllocMem;
		}

		for (j=0; j<96; j++)
			if (ins2.snum[j]<ins1.samp)
				ip->samples[j]=m->nsamp+ins2.snum[j];
		volfade=0xFFFF;
		volfade=ins2.volfade;
		if (ins2.vtype&1)
		{
			int16_t k, h;
			int p;
			env[0].speed=0;
			env[0].type=0;
			env[0].env=malloc(sizeof(uint8_t)*(ins2.venv[ins2.vnum-1][0]+1));
			if (!env[0].env)
			{
				fprintf(stderr, __FILE__ ": malloc failed #6 (size=%d)\n", (int)sizeof(uint8_t)*(ins2.venv[ins2.vnum-1][0]+1));
				FreeResources (&r, head2.ninst);
				return errAllocMem;
			}
			p=0;
			h=ins2.venv[0][1]*4;
			for (j=1; j<ins2.vnum; j++)
			{
				int16_t l=ins2.venv[j][0]-p;
				int16_t dh=ins2.venv[j][1]*4-h;
				for (k=0; k<l; k++)
				{
					int16_t cv=h+dh*k/l;
					if ((p+1)>=(ins2.venv[ins2.vnum-1][0]+1))
					{
						fprintf(stderr, __FILE__ ": sanity-check in volume envelopes failed, bailing out, sample will contain errors\n");
						goto bail1;
					}
					env[0].env[p++]=(cv>255)?255:cv;
				}
				h+=dh;
			}
bail1:
			env[0].len=p;
			env[0].env[p]=(h>255)?255:h;
			if (ins2.vtype&2)
			{
				env[0].type|=xmpEnvSLoop;
				env[0].sustain=ins2.venv[ins2.vsustain][0];
			}
			if (ins2.vtype&4)
			{
				env[0].type|=xmpEnvLoop;
				env[0].loops=ins2.venv[ins2.vloops][0];
				env[0].loope=ins2.venv[ins2.vloope][0];
			}
		}

		if (ins2.ptype&1)
		{
			int16_t k, h;
			int p;
			env[1].speed=0;
			env[1].type=0;
			env[1].env=malloc(sizeof(uint8_t)*(ins2.penv[ins2.pnum-1][0]+1));
			if (!env[1].env)
			{
				fprintf(stderr, __FILE__ ": malloc failed #7 (size=%d)\n", (int)sizeof(uint8_t)*(ins2.penv[ins2.pnum-1][0]+1));
				FreeResources (&r, head2.ninst);
				return errAllocMem;
			}
			p=0;
			h=ins2.penv[0][1]*4;
			for (j=1; j<ins2.pnum; j++)
			{
				int16_t l=ins2.penv[j][0]-p;
				int16_t dh=ins2.penv[j][1]*4-h;
				for (k=0; k<l; k++)
				{
					int16_t cv=h+dh*k/l;
					if ((p+1)>=(ins2.penv[ins2.pnum-1][0]+1))
					{
						fprintf(stderr, __FILE__ ": sanity-check in panning envelopes failed, bailing out, sample will contain errors\n");
						goto bail2;
					}
					env[1].env[p++]=(cv>255)?255:cv;
				}
				h+=dh;
			}
bail2:
			env[1].len=p;
			env[1].env[p]=(h>255)?255:h;
			if (ins2.ptype&2)
			{
				env[1].type|=xmpEnvSLoop;
				env[1].sustain=ins2.penv[ins2.psustain][0];
			}
			if (ins2.ptype&4)
			{
				env[1].type|=xmpEnvLoop;
				env[1].loops=ins2.penv[ins2.ploops][0];
				env[1].loope=ins2.penv[ins2.ploope][0];
			}
		}
		for (j=0; j<ins1.samp; j++)
		{
			struct __attribute__((packed))
			{
				uint32_t samplen;
				uint32_t loopstart;
				uint32_t looplen;
				uint8_t vol;
				int8_t finetune;
				uint8_t type;
				uint8_t pan;
				int8_t relnote;
				uint8_t res;
				int8_t name[22];
			} samp;
			struct xmpsample *sp=&r.msmps[i][j];
			struct sampleinfo *sip=&r.smps[i][j];

			if (file->read (file, &samp, sizeof (samp)) != sizeof (samp))
			{
				fprintf(stderr, __FILE__ ": fread() failed #7\n");
				FreeResources (&r, head2.ninst);
				return errFormStruc;
			}
			samp.samplen   = uint32_little (samp.samplen);
			samp.loopstart = uint32_little (samp.loopstart);
			samp.looplen   = uint32_little (samp.looplen);

			if (file->seek_cur (file, ins2.shsize - sizeof (samp)) < 0)
			{
				fprintf(stderr, __FILE__ ": fseek failed #5\n");
				FreeResources (&r, head2.ninst);
				return errFormStruc;
			}
			if (samp.type&16)
			{
				samp.samplen>>=1;
				samp.loopstart>>=1;
				samp.looplen>>=1;
			}

			memcpy(sp->name, samp.name, 22);
			sp->name[22]=0;
			sp->handle=0xFFFF;
			sp->normnote=-samp.relnote*256-samp.finetune*2;
			sp->normtrans=-samp.relnote*256;
			sp->stdvol=(samp.vol>0x3F)?0xFF:(samp.vol<<2);
			sp->stdpan=samp.pan;
			sp->opt=0;
			sp->volfade=volfade;
			sp->vibtype=ins2.vibtype;
			sp->vibdepth=ins2.vibdepth<<2;
			sp->vibspeed=0;
			sp->vibrate=ins2.vibrate<<8;
			sp->vibsweep=0xFFFF/(ins2.vibsweep+1);
			sp->volenv=env[0].env?(2*i+0):0xFFFF;
			sp->panenv=env[1].env?(2*i+1):0xFFFF;
			sp->pchenv=0xFFFF;

			sip->loopstart=samp.loopstart;
			sip->loopend=samp.loopstart+samp.looplen;
			sip->samprate=8363;
			sip->type=mcpSampDelta| ((samp.type&32)?mcpSampStereo:0) | ((samp.type&16)?mcpSamp16Bit:0) | ((samp.type&3)?(((samp.type&3)>=2)?(mcpSampLoop|mcpSampBiDi):mcpSampLoop):0);
			sip->length=samp.samplen >> (!!(sip->type & mcpSampStereo));
		}

		for (j=0; j<ins1.samp; j++)
		{
			struct xmpsample *sp=&r.msmps[i][j];
			struct sampleinfo *sip=&r.smps[i][j];
			uint32_t l=sip->length<<((!!(sip->type&mcpSamp16Bit)) + (!!(sip->type & mcpSampStereo)));
			if (!l)
				continue;
			sip->ptr=malloc(sizeof(uint8_t)*(l+528));
			if (!sip->ptr)
			{
				fprintf(stderr, __FILE__ ": malloc failed #8 (i=%d, j=%d/%d size=%d)\n", i, j, ins1.samp, (int)sizeof(uint8_t)*(l+528));
				FreeResources (&r, head2.ninst);
				return errAllocMem;
			}
			if (file->read (file, sip->ptr, l) != l)
			{
				fprintf(stderr, __FILE__ ": fread failed #8\n");
				FreeResources (&r, head2.ninst);
				return errFileRead;
			}
			sp->handle=m->nsampi++;
		}
		m->nsamp+=ins1.samp;
	}

	m->samples=malloc(sizeof(struct xmpsample)*m->nsamp);
	m->sampleinfos=malloc(sizeof(struct sampleinfo)*m->nsampi);
	if (!m->samples||!m->sampleinfos)
	{
		fprintf(stderr, __FILE__ ": malloc failed #9 (%p(%d) %p(%d))\n", m->samples, (int)sizeof(struct xmpsample)*m->nsamp, m->sampleinfos, (int)sizeof(struct sampleinfo)*m->nsampi);
		FreeResources (&r, head2.ninst);
		return errAllocMem;
	}

	m->nsampi=0;
	m->nsamp=0;
	for (i=0; i<m->ninst; i++)
	{
		for (j=0; j<r.instsmpnum[i]; j++)
		{
			m->samples[m->nsamp++]=r.msmps[i][j];
			if (r.smps[i][j].ptr)
				m->sampleinfos[m->nsampi++]=r.smps[i][j];
		}
		free(r.smps[i]);
		free(r.msmps[i]);
	}
	free(r.smps);
	free(r.msmps);
	free(r.instsmpnum);

	return errOk;
}
