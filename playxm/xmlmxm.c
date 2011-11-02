/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * XMPlay .MXM module loader
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -EXPERIMENTAL STAGE - i need MANY .MXMs to test! :)
 *    -loops don't seem to be correct sometimes, don't know why
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "xmplay.h"
#include "stuff/err.h"

struct LoadMXMResources
{
	struct sampleinfo **smps;
	struct xmpsample **msmps;
	unsigned int *instsmpnum;
};


static void FreeResources(struct LoadMXMResources *r, struct xmodule *m)
{
	int i;

	if (r->smps||r->msmps)
		for (i=0; i<m->ninst; i++)
		{
			if (r->smps)
				if (r->smps[i])
					free(r->smps[i]);
			if (r->msmps)
				if (r->msmps[i])
				free(r->msmps[i]);
		}
	if (r->smps)
	{
		free(r->smps);
		r->smps = 0;
	}
	if (r->msmps)
	{
		free(r->msmps);
		r->msmps = 0;
	}
	if (r->instsmpnum)
	{
		free(r->instsmpnum);
		r->instsmpnum = 0;
	}
}

int __attribute__ ((visibility ("internal"))) xmpLoadMXM(struct xmodule *m, FILE *file)
{
	uint8_t deltasamps/*, modpanning*/;

	struct __attribute__((packed))
	{
		uint8_t sig[4];
		uint32_t ordnum;
		uint32_t restart;
		uint32_t channum;
		uint32_t patnum;
		uint32_t insnum;
		uint8_t tempo;
		uint8_t speed;
		uint16_t opt;
		uint32_t sampstart;
		uint32_t samples8;
		uint32_t samples16;
		int32_t lowpitch;
		int32_t highpitch;
		uint8_t panpos[32];
		uint8_t ord[256];
		uint32_t insofs[128];
		uint32_t patofs[256];
	} mxmhead;


	unsigned int i,j;

	uint32_t guspos[128*16];
	struct LoadMXMResources r;

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

	/* 1st: read headers */

	if (fread(&mxmhead, sizeof(mxmhead), 1, file)!=1)
	{
		fprintf(stderr, "xmlxmx.c: fread() header failed\n");
		return errFormStruc;
	}
	if (memcmp(&mxmhead.sig, "MXM\0", 4))
		return errFormStruc;

	mxmhead.ordnum    = uint32_little (mxmhead.ordnum);
	mxmhead.restart   = uint32_little (mxmhead.restart);
	mxmhead.channum   = uint32_little (mxmhead.channum);
	mxmhead.patnum    = uint32_little (mxmhead.patnum);
	mxmhead.insnum    = uint32_little (mxmhead.insnum);
	mxmhead.opt       = uint16_little (mxmhead.opt);
	mxmhead.sampstart = uint32_little (mxmhead.sampstart);
	mxmhead.samples8  = uint32_little (mxmhead.samples8);
	mxmhead.samples16 = uint32_little (mxmhead.samples16);
	mxmhead.lowpitch  = int32_little (mxmhead.lowpitch);
	mxmhead.highpitch = int32_little (mxmhead.highpitch);

	for (i=0;i<128;i++)
		mxmhead.insofs[i] = uint32_little (mxmhead.insofs[i]);
	for (i=0;i<256;i++)
		mxmhead.patofs[i] = uint32_little (mxmhead.patofs[i]);

	memcpy(m->name, "MXMPlay module      ", 20);
	m->name[20]=0;

	/* TODO? modpanning = !!(mxmhead.opt&2);*/
	deltasamps = !!(mxmhead.opt&4);

	m->linearfreq=!!(mxmhead.opt&1);
	m->nchan=mxmhead.channum;

	m->ninst=mxmhead.insnum;
	m->nenv=2*mxmhead.insnum;

	m->npat=mxmhead.patnum+1;

	m->nord=mxmhead.ordnum;
	m->loopord=mxmhead.restart;

	m->inibpm=mxmhead.speed;
	m->initempo=mxmhead.tempo;

	m->orders=malloc(sizeof(uint16_t)*m->nord);

	m->patterns=(uint8_t (**)[5])calloc(sizeof(void *), m->npat);
	m->patlens=malloc(sizeof(uint16_t)*m->npat);

	m->instruments=calloc(sizeof(struct xmpinstrument), m->ninst);
	m->envelopes=calloc(sizeof(struct xmpenvelope), m->nenv);

	r.smps = calloc(sizeof(struct sampleinfo *),m->ninst);
	r.msmps = calloc(sizeof(struct xmpsample *),m->ninst);
	r.instsmpnum = malloc(sizeof(int)*m->ninst);

	if (!r.smps||!r.msmps||!r.instsmpnum||!m->instruments||!m->envelopes||!m->patterns||!m->orders||!m->patlens)
	{
		FreeResources (&r, m);
		return errAllocMem;
	}

	for (i=0; i<32; i++)
		m->panpos[i]=0x11*mxmhead.panpos[i];

	for (i=0; i<m->nord; i++)
		m->orders[i]=(mxmhead.ord[i]<mxmhead.patnum)?mxmhead.ord[i]:mxmhead.patnum;

	m->patlens[mxmhead.patnum]=64;
	m->patterns[mxmhead.patnum]= calloc(sizeof(uint8_t)*64,mxmhead.channum*5);
	if (!m->patterns[mxmhead.patnum])
	{
		FreeResources (&r, m);
		return errAllocMem;
	}

	/* 2nd: read instruments */

	memset(guspos,0xff,4*128*16);

	m->nsampi=0;
	m->nsamp=0;

	for (i=0; i<m->ninst; i++)
	{
		struct xmpinstrument *ip=&m->instruments[i];
		struct xmpenvelope *env=m->envelopes+2*i;

		struct __attribute__((packed))
		{
			uint32_t sampnum;
			uint8_t snum[96];
			uint16_t volfade;
			uint8_t vibtype, vibsweep, vibdepth, vibrate;
			uint8_t vnum, vsustain, vloops, vloope;
			uint16_t venv[12][2];
			uint8_t pnum, psustain, ploops, ploope;
			uint16_t penv[12][2];
			uint8_t res[46];
		} mxmins;
		uint16_t volfade;
		long el=0;
		int16_t k, p=0, h;

		fseek(file, mxmhead.insofs[i], SEEK_SET);

		/*
		r.smps[i]=0;
		r.msmps[i]=0;
		*/

		if (fread(&mxmins, sizeof(mxmins), 1, file)!=1)
			fprintf(stderr, __FILE__ ": warning, read failed #1\n");
		mxmins.sampnum = uint32_little (mxmins.sampnum);
		mxmins.volfade = uint16_little (mxmins.volfade);
		for (k=0;k<12;k++)
			for (h=0;h<2;h++)
			{
				mxmins.venv[k][h] = uint16_little (mxmins.venv[k][h]);
				mxmins.penv[k][h] = uint16_little (mxmins.penv[k][h]);
			}

		memcpy(ip->name,"                      ",22);
		ip->name[22]=0;

		memset(ip->samples, 0xff, 2*128);
		r.instsmpnum[i]=mxmins.sampnum;

		r.smps[i]=calloc(sizeof(struct sampleinfo), mxmins.sampnum);
		r.msmps[i]=calloc(sizeof(struct xmpsample), mxmins.sampnum);

		if (!r.smps[i]||!r.msmps[i])
		{
			FreeResources (&r, m);
			return errAllocMem;
		}

		for (j=0; j<96; j++)
			if (mxmins.snum[j]<mxmins.sampnum)
				ip->samples[j]=m->nsamp+mxmins.snum[j];

		volfade = mxmins.volfade;

		env[0].speed=0;
		env[0].type=0;
		for (j=0; j<mxmins.vnum; j++)
			el+=mxmins.venv[j][0];
		env[0].env=malloc(sizeof(uint8_t)*(el+1));
		if (!env[0].env)
		{
			FreeResources (&r, m);
			return errAllocMem;
		}
		h=mxmins.venv[0][1]*4;
		for (j=0; j<mxmins.vnum; j++)
		{
			int16_t l=mxmins.venv[j][0];
			int16_t dh=mxmins.venv[j+1][1]*4-h;
			for (k=0; k<l; k++)
			{
				int16_t cv=h+dh*k/l;
				env[0].env[p++]=(cv>255)?255:cv;
			}
			h+=dh;
		}
		env[0].len=p;
		env[0].env[p]=(h>255)?255:h;
		if (mxmins.vsustain<0xff)
		{
			env[0].type|=xmpEnvSLoop;
			env[0].sustain=0;
			for (j=0; j<mxmins.vsustain; j++)
				env[0].sustain+=mxmins.venv[j][0];
		}
		if (mxmins.vloope<0xff)
		{
			env[0].type|=xmpEnvLoop;
			env[0].loops=env[0].loope=0;
			for (j=0; j<mxmins.vloops; j++)
				env[0].loops+=mxmins.venv[j][0];
			for (j=0; j<mxmins.vloope; j++)
				env[0].loope+=mxmins.venv[j][0];
		}

		env[1].speed=0;
		env[1].type=0;
		el=0;
		for (j=0; j<mxmins.pnum; j++)
			el+=mxmins.penv[j][0];
		env[1].env=malloc(sizeof(uint8_t)*(el+1));
		if (!env[1].env)
		{
			FreeResources (&r, m);
			return errAllocMem;
		}
		p=0;
		h=mxmins.penv[0][1]*4;
		for (j=0; j<mxmins.pnum; j++)
		{
			int16_t l=mxmins.penv[j][0];
			int16_t dh=mxmins.penv[j+1][1]*4-h;
			for (k=0; k<l; k++)
			{
				int16_t cv=h+dh*k/l;
				env[1].env[p++]=(cv>255)?255:cv;
			}
			h+=dh;
		}
		env[1].len=p;
		env[1].env[p]=(h>255)?255:h;
		if (mxmins.psustain<0xff)
		{
			env[1].type|=xmpEnvSLoop;
			env[1].sustain=0;
			for (j=0; j<mxmins.psustain; j++)
				env[1].sustain+=mxmins.penv[j][0];
		}
		if (mxmins.ploope<0xff)
		{
			env[1].type|=xmpEnvLoop;
			env[1].loops=env[1].loope=0;
			for (j=0; j<mxmins.ploops; j++)
				env[1].loops+=mxmins.penv[j][0];
			for (j=0; j<mxmins.ploope; j++)
				env[1].loope+=mxmins.penv[j][0];
		}


		for (j=0; j<mxmins.sampnum; j++)
		{
			struct __attribute__((packed))
			{
				uint16_t gusstartl;
				uint8_t gusstarth;
				uint16_t gusloopstl;
				uint8_t gusloopsth;
				uint16_t gusloopendl;
				uint8_t gusloopendh;
				uint8_t gusmode;
				uint8_t vol;
				uint8_t pan;
				uint16_t relpitch;
				uint8_t res[2];
			} mxmsamp;
			uint8_t bit16, sloop, sbidi;
			struct xmpsample *sp=&r.msmps[i][j];
			struct sampleinfo *sip=&r.smps[i][j];

			int8_t rpf;
			uint32_t sampstart, loopstart, loopend;
			uint32_t l;

			if (fread(&mxmsamp, sizeof(mxmsamp), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #2\n");
			mxmsamp.gusstartl   = uint16_little (mxmsamp.gusstartl);
			mxmsamp.gusloopstl  = uint16_little (mxmsamp.gusloopstl);
			mxmsamp.gusloopendl = uint16_little (mxmsamp.gusloopendl);
			mxmsamp.relpitch    = uint16_little (mxmsamp.relpitch);

			bit16=mxmsamp.gusmode&0x04;
			sloop=mxmsamp.gusmode&0x08;
			sbidi=mxmsamp.gusmode&0x18;

			memcpy(sp->name, "                      ", 22);
			sp->name[22]=0;
			sp->handle=0xFFFF;

			sp->normnote=-mxmsamp.relpitch;
			rpf=mxmsamp.relpitch&0xff;
			sp->normtrans=rpf-mxmsamp.relpitch;
			sp->stdvol=(mxmsamp.vol>0x3F)?0xFF:(mxmsamp.vol<<2);
			sp->stdpan=mxmsamp.pan;
			sp->opt=0;
			sp->volfade=volfade;
			sp->vibtype=(mxmins.vibtype==1)?3:(mxmins.vibtype==2)?1:(mxmins.vibtype==3)?2:0;
			sp->vibdepth=mxmins.vibdepth<<2;
			sp->vibspeed=0;
			sp->vibrate=mxmins.vibrate<<8;
			sp->vibsweep=0xFFFF/(mxmins.vibsweep+1);
			sp->volenv=env[0].env?(2*i+0):0xFFFF;
			sp->panenv=env[1].env?(2*i+1):0xFFFF;
			sp->pchenv=0xFFFF;

			sampstart=mxmsamp.gusstartl+(mxmsamp.gusstarth<<16);
			loopstart=mxmsamp.gusloopstl+(mxmsamp.gusloopsth<<16);
			loopend=mxmsamp.gusloopendl+(mxmsamp.gusloopendh<<16);
			guspos[m->nsampi]=sampstart;
			if (loopstart<sampstart)
				loopstart=sampstart;
			if (loopend<sampstart)
				loopend=sampstart;
			loopstart-=sampstart;
			loopend-=sampstart;

			if (bit16)
			{
				loopstart>>=1;
				loopend>>=1;
			}

			sip->length=loopend;
			sip->loopstart=loopstart;
			sip->loopend=loopend+1;
			sip->samprate=8363;
			sip->type=(bit16?mcpSamp16Bit:0)|(sloop?mcpSampLoop:0)|(sbidi?mcpSampBiDi:0);

			l=sip->length<<(!!(sip->type&mcpSamp16Bit));
			if (!l)
				continue;
			sip->ptr=malloc(sizeof(uint8_t)*(l+528));
			if (!sip->ptr)
			{
				FreeResources (&r, m);
				return errAllocMem;
			}
			sp->handle=m->nsampi++;
		}

		m->nsamp+=mxmins.sampnum;
	}

	m->samples=malloc(sizeof(struct xmpsample)*m->nsamp);
	m->sampleinfos=malloc(sizeof(struct sampleinfo)*m->nsampi);

	if (!m->samples||!m->sampleinfos)
	{
		FreeResources (&r, m);
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
	r.smps=0;
	r.msmps=0;
	r.instsmpnum=0;

	for (i=0; i<mxmhead.patnum; i++)
	{
		uint32_t patrows;
		uint8_t  pack;
		uint8_t pd[2];

		fseek(file, mxmhead.patofs[i], SEEK_SET);

		if (fread(&patrows, sizeof(uint32_t), 1, file)!=1)
			fprintf(stderr, __FILE__ ": warning, read failed #3\n");
		patrows = uint32_little (patrows);

		m->patlens[i]=patrows;
		m->patterns[i]=malloc(sizeof(uint8_t)*patrows*mxmhead.channum*5);
		if (!m->patterns[i])
			return errAllocMem;

		memset(m->patterns[i], 0, patrows*mxmhead.channum*5);

		for (j=0; j<patrows; j++)
		{
			uint8_t *currow=(uint8_t *)(m->patterns[i])+j*mxmhead.channum*5;
			if (fread(&pack, 1, 1, file)!=1)
				fprintf(stderr, __FILE__ ": warning, read failed #4\n");
			while (pack)
			{
				uint8_t *cur=currow+5*(pack&0x1f);
				if (pack&0x20)
				{
					if (fread(pd, sizeof(pd), 1, file)!=1)
						fprintf(stderr, __FILE__ ": warning, read failed #5\n");
					cur[0]=pd[0];
					cur[1]=pd[1];
				}
				if (pack&0x40)
				{
					if (fread(pd, 1, 1, file)!=1)
						fprintf(stderr, __FILE__ ": warning, read failed #6\n");
					cur[2]=pd[0];
				}
				if (pack&0x80)
				{
					if (fread(pd, 2, 1, file)!=1)
						fprintf(stderr, __FILE__ ": warning, read failed #7\n");
					cur[3]=pd[0];
					cur[4]=pd[1];
				}
				if (fread(&pack, 1, 1, file)!=1)
					fprintf(stderr, __FILE__ ": warning, read failed #8\n");
			}
		}
	}

	{
		uint32_t gsize=mxmhead.samples8+2*mxmhead.samples16;
		int8_t *gusmem = malloc(sizeof(int8_t)*gsize);
		int16_t *gus16 = (int16_t *)(gusmem+mxmhead.samples8);
		fseek(file, mxmhead.sampstart, SEEK_SET);
		if (fread(gusmem, gsize, 1, file)!=1)
			fprintf(stderr, __FILE__ ": warning, read failed #9\n");

		if (deltasamps)
		{
			int8_t db=0;
			int16_t dw=0;
			for (i=0; i<mxmhead.samples8; i++)
				gusmem[i]=db+=gusmem[i];
			for (i=0; i<mxmhead.samples16; i++)
				gus16[i]=dw+=gus16[i];
		}

		for (i=0; i<m->nsampi; i++)
		{
			uint32_t poslo, poshi;

			uint32_t actpos=guspos[i];
			uint32_t len=m->sampleinfos[i].length;
			if (m->sampleinfos[i].type&mcpSamp16Bit)
			{
				actpos<<=1;
				len<<=1;
				poslo=actpos&0x03FFFE;
				poshi=actpos&0x180000;
				actpos=poslo|(poshi>>1);
			}
			if ((len+actpos)>gsize)
			{
				fprintf(stderr, "sample #%d has sample that goes outside GUS memorywindow, chopping\n", i);
				fprintf(stderr, "debug: sample #%d guspos hi:0x%02x lo:0x%04x len=0x%08x actpos=0x%08x gsize=0x%08x\n", i, guspos[i]>>8, guspos[8]&0xffff, len, actpos, gsize);
				len = gsize-actpos;
			}

			memcpy(m->sampleinfos[i].ptr, gusmem+actpos, len);
		}

		free(gusmem);
	}
	return errOk;
}
