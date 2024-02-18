/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for UltraTracker modules
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

static inline void putcmd(uint8_t **p, uint8_t c, uint8_t d)
{
	*(*p)++=c;
	*(*p)++=d;
}

struct LoadULTResources
{
	uint8_t *temptrack;
	uint8_t *buffer;
};

static void FreeResources(struct LoadULTResources *r)
{
	if (r->temptrack)
	{
		free(r->temptrack);
		r->temptrack = 0;
	}
	if (r->buffer)
	{
		free(r->buffer);
		r->buffer = 0;
	}
}

OCP_INTERNAL int LoadULT (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	char id[15];
	uint8_t ver;
	uint8_t msglen;
	uint8_t insn;
	uint32_t samplen;
	unsigned int i,j;
	uint8_t orders[256];
	uint8_t chnn;
	uint8_t patn;
	uint8_t panpos[32];
	int16_t ordn;
	struct gmdpattern *pp;
	unsigned int t;
	uint32_t patlength;
	uint8_t *bp;
	uint8_t *chbp[32];
	unsigned int q;
	uint8_t *chcurcmd[32];
	uint8_t chrepn[32];
	struct LoadULTResources r;

	r.buffer=0;
	r.temptrack=0;

	mpReset(m);

	if (file->read (file, id, 15) != 15)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] error, read failed #1\n");
		return errFileRead;
	}
	if (memcmp(id, "MAS_UTrack_V00", 14))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] file too old? (Invalid signature)\n");
		return errFormSig;
	}

	ver=id[14]-'0';

	if (ver>4)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] file too new (%d > 4)\n", ver);
		return errFormNewVer;
	}

	m->options=(ver<2)?MOD_GUSVOL:0;

	if (file->read (file, m->name, 32) != 32)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #2\n");
		return errFileRead;
	}
	m->name[31]=0;

	if (ocpfilehandle_read_uint8 (file, &msglen))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] error, read failed #2.1\n");
		return errFileRead;
	}

	if (msglen)
	{
		int16_t t;

		m->message=malloc(sizeof(char *)*(msglen+1));
		if (!m->message)
			return errAllocMem;

		*m->message=malloc(sizeof(char)*msglen*33);
		if (!*m->message)
			return errAllocMem;
		for (t=0; t<msglen; t++)
		{
			m->message[t]=*m->message+t*33;
			if (file->read (file, m->message[t], 32) != 32)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] error, read failed #3\n");
				return errFileRead;
			}
			m->message[t][32]=0;
		}
		m->message[msglen]=0;
	}

	if (ocpfilehandle_read_uint8 (file, &insn))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] error, read failed #3.1\n");
		return errFileRead;
	}

	m->modsampnum=m->sampnum=m->instnum=insn;

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum))
		return errAllocMem;

	samplen=0;

	for (i=0; i<m->instnum; i++)
	{
		uint32_t length;
		struct gmdinstrument *ip;
		struct gmdsample *sp;
		struct sampleinfo *sip;

		struct __attribute__((packed))
		{
			char name[32];
			char dosname[12];
			uint32_t loopstart;
			uint32_t loopend;
			uint32_t sizestart;
			uint32_t sizeend;
			uint8_t vol;
			uint8_t opt;
			uint16_t c2spd;
			uint16_t finetune;
		} mi;

		if (ver<4) /* files prior to version 1.4, did not have c2spd field */
		{
			if (file->read (file, &mi, sizeof (mi) - sizeof (uint16_t)) != (sizeof (mi) - sizeof (uint16_t)))
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #3\n");
			}
			mi.finetune=mi.c2spd;
			mi.c2spd=8363;
		} else {
			if (file->read (file, &mi, sizeof (mi)) != sizeof (mi))
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #3\n");
			}
		}

		mi.loopstart = uint32_little (mi.loopstart);
		mi.loopend   = uint32_little (mi.loopend);
		mi.sizestart = uint32_little (mi.sizestart);
		mi.sizeend   = uint32_little (mi.sizeend);
		mi.c2spd     = uint16_little (mi.c2spd);
		mi.finetune  = uint16_little (mi.finetune);

		length=mi.sizeend-mi.sizestart;
		if (mi.opt&4)
		{
			mi.loopstart>>=1;
			mi.loopend>>=1;
		}
		if (mi.loopstart>length)
			mi.opt&=~8;
		if (mi.loopend>length)
			mi.loopend=length;
		if (mi.loopstart==mi.loopend)
			mi.opt&=~8;

		ip=&m->instruments[i];
		sp=&m->modsamples[i];

		memcpy(ip->name, mi.name, 31);
		ip->name[31]=0;
		if (!length)
			continue;
		for (j=0; j<128; j++)
			ip->samples[j]=i;

		memcpy(sp->name, mi.dosname, 12);
		sp->name[12]=0;

		sp->handle=i;
		sp->normnote=-cpifaceSession->mcpAPI->GetNote8363(mi.c2spd);
		sp->stdvol=mi.vol;
		sp->stdpan=-1;
		sp->opt=(mi.opt&4)?MP_OFFSETDIV2:0;

		sip=&m->samples[i];
		sip->length=length;
		sip->loopstart=mi.loopstart;
		sip->loopend=mi.loopend;
		sip->samprate=8363;
		sip->type=((mi.opt&8)?mcpSampLoop:0)|((mi.opt&16)?mcpSampBiDi:0)|((mi.opt&4)?mcpSamp16Bit:0);

		samplen+=((mi.opt&4)?2:1)*sip->length;
	}

	if (file->read (file, orders, 256) != 256)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #4\n");
	}

	if (ocpfilehandle_read_uint8 (file, &chnn))
	{
		chnn = 0;
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #4.1\n");
	}

	if (ocpfilehandle_read_uint8 (file, &patn))
	{
		patn = 0;
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #4.2\n");
	}

	if (chnn>31)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] Too many channels (%d > 32)\n", 1 + chnn);
		return errFormStruc;
	}

	m->channum=chnn+1;

	if (ver>=3)
	{
		if (file->read (file, panpos, m->channum) != m->channum)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #5\n");
		}
	} else
		memcpy(panpos, "\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF", 32);

	m->loopord=0;

	for (ordn=0; ordn<256; ordn++)
		if (orders[ordn]>patn)
			break;

	m->patnum=ordn;
	m->ordnum=ordn;
	m->endord=m->patnum;
	m->tracknum=(patn+1)*(m->channum+1);

	if (!mpAllocPatterns(m, m->patnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocOrders(m, m->ordnum))
		return errAllocMem;

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=i;

	for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
	{
		pp->patlen=64;
		for (i=0; i<m->channum; i++)
			pp->tracks[i]=orders[t]*(m->channum+1)+i;
		pp->gtrack=orders[t]*(m->channum+1)+m->channum;
	}

	patlength=file->filesize (file) - file->getpos (file) - samplen;

	r.temptrack=malloc(sizeof(uint8_t)*2000);
	r.buffer=malloc(sizeof(uint8_t)*patlength);
	if (!r.buffer||!r.temptrack)
	{
		FreeResources(&r);
		return errAllocMem;
	}

	if (file->read (file, r.buffer, patlength) != patlength)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #6\n");
	}

	bp=r.buffer;

	for (q=0; q<m->channum; q++)
	{
		chbp[q]=bp;

		for (t=0; t<=patn; t++)
		{
			uint8_t *curcmd=0;
			uint8_t repn=0;

			uint8_t *tp=r.temptrack;

			uint8_t  row;

			struct gmdtrack *trk;
			uint16_t len;

			for (row=0; row<64; row++)
			{
				uint8_t *cp;

				int16_t ins;
				int16_t nte;
				int16_t pan;
				int16_t vol;
				uint8_t command[2];
				uint8_t data[2];
				int16_t f;

				if (!repn)
				{
					if (*bp==0xFC)
					{
						repn=bp[1]-1;
						curcmd=bp+2;
						bp+=7;
					} else {
						curcmd=bp;
						bp+=5;
					}
				} else
					repn--;

				cp=tp+2;

				if (!curcmd)
				{
					cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] curcmd==NULL\n");
					FreeResources (&r);
					return errFormStruc;
				}
				ins=(int16_t)curcmd[1]-1;
				nte=curcmd[0]?(curcmd[0]+35):-1;
				pan=-1;
				vol=-1;
				command[0]=curcmd[2]>>4;
				command[1]=curcmd[2]&0xF;
				data[0]=curcmd[4];
				data[1]=curcmd[3];

				if (command[0]==0xE)
				{
					command[0]=(data[0]&0xF0)|0xE;
					data[0]&=0xF;
				}
				if (command[1]==0xE)
				{
					command[1]=(data[1]&0xF0)|0xE;
					data[1]&=0xF;
				}
				if (command[0]==0x5)
				{
					command[0]=(data[0]&0xF0)|0x5;
					data[0]&=0xF;
				}
				if (command[1]==0x5)
				{
					command[1]=(data[1]&0xF0)|0x5;
					data[1]&=0xF;
				}

				if (!row&&(t==orders[0]))
					pan=panpos[q]*0x11;

				if (command[0]==0xC)
					vol=data[0];
				if (command[1]==0xC)
					vol=data[1];

				if (command[0]==0xB)
					pan=data[0]*0x11;
				if (command[1]==0xB)
					pan=data[1]*0x11;

				if (((command[0]==0x3)||(command[1]==0x3))&&(nte!=-1))
					nte|=128;

				if ((ins!=-1)||(nte!=-1)||(vol!=-1)||(pan!=-1))
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
					if (pan!=-1)
					{
						*act|=cmdPlayPan;
						*cp++=pan;
					}
					if (command[1]==0xDE)
					{
						*act|=cmdPlayDelay;
						*cp++=data[1];
					}
					else if (command[0]==0xDE)
					{
						*act|=cmdPlayDelay;
						*cp++=data[0];
					}
				}
				for (f=0; f<2; f++)
				{
					switch (command[f])
					{
						case 0x1:
							if (data[f])
								putcmd(&cp, cmdPitchSlideUp, data[f]);
							break;
						case 0x2:
							if (data[f])
								putcmd(&cp, cmdPitchSlideDown, data[f]);
							break;
						case 0x3:
							putcmd(&cp, cmdPitchSlideToNote, data[f]);
							break;
						case 0x4:
							putcmd(&cp, cmdPitchVibrato, data[f]);
							break;
						case 0xA:
							if ((data[f]&0x0F)&&(data[f]&0xF0))
								data[f]=0;
							if (data[f]&0xF0)
								putcmd(&cp, cmdVolSlideUp, data[f]>>4);
							else if (data[f]&0x0F)
								putcmd(&cp, cmdVolSlideDown, data[f]&0xF);
							break;
						case 0x1E:
							if (data[f])
								putcmd(&cp, cmdRowPitchSlideUp, data[f]<<4);
							break;
						case 0x2E:
							if (data[f])
								putcmd(&cp, cmdRowPitchSlideDown, data[f]<<4);
							break;
						case 0x9E:
							if (data[f])
								putcmd(&cp, cmdRetrig, data[f]);
							break;
						case 0xAE:
							if (data[f])
								putcmd(&cp, cmdRowVolSlideUp, data[f]);
							break;
						case 0xBE:
							if (data[f])
								putcmd(&cp, cmdRowVolSlideDown, data[f]);
							break;
						case 0xCE:
							putcmd(&cp, cmdNoteCut, data[f]);
							break;
					}
				}
				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
				}
			}

			trk=&m->tracks[t*(m->channum+1)+q];
			len=tp-r.temptrack;

			if (!len)
				trk->ptr=trk->end=0;
			else {
				trk->ptr=malloc(sizeof(uint8_t)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
					return errAllocMem;
				memcpy(trk->ptr, r.temptrack, len);
			}
		}
	}

	for (t=0; t<=patn; t++)
	{
		uint8_t *tp=r.temptrack;
		uint8_t row;
		struct gmdtrack *trk;
		uint16_t len;

		for (q=0; q<m->channum; q++)
			chrepn[q]=0;
		for (row=0; row<64; row++)
		{
			uint8_t *cp=tp+2;

			for (q=0; q<m->channum; q++)
			{
				uint8_t command[2];
				uint8_t data[2];
				uint8_t f;

				if (!chrepn[q])
					if (*chbp[q]==0xFC)
					{
						chrepn[q]=chbp[q][1]-1;
						chcurcmd[q]=chbp[q]+2;
						chbp[q]+=7;
					} else {
						chcurcmd[q]=chbp[q];
						chbp[q]+=5;
					} else
						chrepn[q]--;
				command[0]=chcurcmd[q][2]>>4;
				command[1]=chcurcmd[q][2]&0xF;
				data[0]=chcurcmd[q][4];
				data[1]=chcurcmd[q][3];

				for (f=0; f<2; f++)
				{
					switch (command[f])
					{
						case 0xD:
							if (data[f]>=0x64)
								data[f]=0;
							putcmd(&cp, cmdBreak, (data[f]&0xF)+(data[f]>>4)*10);
							break;
						case 0xF:
							if (data[f])
							{
								if (data[f]<=0x20)
									putcmd(&cp, cmdTempo, data[f]);
								else
									putcmd(&cp, cmdSpeed, data[f]);
							}
							break;
					}
				}
			}

			if (cp!=(tp+2))
			{
				tp[0]=row;
				tp[1]=cp-tp-2;
				tp=cp;
			}
		}

		trk=&m->tracks[t*(m->channum+1)+m->channum];
		len=tp-r.temptrack;

		if (!len)
			trk->ptr=trk->end=0;
		else {
			trk->ptr=malloc(sizeof(uint8_t)*len);
			trk->end=trk->ptr+len;
			if (!trk->ptr)
				return errAllocMem;
			memcpy(trk->ptr, r.temptrack, len);
		}
	}
	FreeResources(&r);

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];  NOT USED */
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];
		uint32_t l;
		if (sp->handle==0xFFFF)
			continue;
		l=sip->length<<(!!(sip->type&mcpSamp16Bit));

		sip->ptr=malloc(sizeof(uint8_t)*(l+16));
		if (!sip->ptr)
			return errAllocMem;
		if (file->read (file, sip->ptr, l) != l)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/ULT] warning, read failed #6\n");
		}
	}

	return errOk;
}
