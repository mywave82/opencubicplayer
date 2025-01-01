/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for PolyTracker modules
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
#include "filesel/filesystem.h"
#include "dev/mcp.h"
#include "gmdplay.h"
#include "stuff/err.h"

static inline void putcmd(uint8_t **p, uint8_t c, uint8_t d)
{
	*(*p)++=c;
	*(*p)++=d;
}

struct LoadPTMResources
{
	uint8_t *buffer;
	uint8_t *temptrack;
};

static void FreeResources (struct LoadPTMResources *r)
{
	if (r->buffer)
	{
		free(r->buffer);
		r->buffer = 0;
	}
	if (r->temptrack)
	{
		free(r->temptrack);
		r->temptrack = 0;
	}
}

OCP_INTERNAL int LoadPTM (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{

	uint16_t t;
	uint8_t orders[256];
	uint16_t patpara[129];
	unsigned int i,j;
	struct gmdpattern *pp;
	uint32_t inspos[256];
	uint16_t bufSize;
	struct LoadPTMResources r;

	r.buffer = 0;
	r.temptrack = 0;


	struct __attribute__((packed))
	{
		char name[28];
		uint8_t end;
		uint16_t type;
		uint8_t d1;
		uint16_t orders,ins,pats,chan,flags,d2;
		char magic[4];
		char d3[16];
		uint8_t channels[32];
	} hdr;

	mpReset(m);

	if (file->read (file, &hdr, sizeof(hdr)) != sizeof (hdr))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] error, read failed #1\n");
		return errFileRead;
	}
	if (memcmp(hdr.magic, "PTMF", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] invalid signature\n");
		return errFormSig;
	}
	hdr.type = uint16_little(hdr.type);
	hdr.orders = uint16_little(hdr.orders);
	hdr.ins = uint16_little(hdr.ins);
	hdr.pats = uint16_little(hdr.pats);
	hdr.chan = uint16_little(hdr.chan);
	hdr.flags = uint16_little(hdr.flags);
	hdr.d2 = uint16_little(hdr.d2);

	memcpy(m->name, hdr.name, 28);
	m->name[28]=0;

	m->channum=hdr.chan;
	m->modsampnum=m->sampnum=m->instnum=hdr.ins;
	m->patnum=hdr.orders;
	m->ordnum=hdr.orders;
	m->endord=m->patnum;
	m->tracknum=hdr.pats*(m->channum+1)+1;
	m->options=MOD_S3M;
	m->loopord=0;

	if (file->read (file, orders, 256) != 256)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] error, read failed #2\n");
		return errFileRead;
	}

	if (!m->patnum)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] no patterns\n");
		return errFormMiss;
	}

	if (file->read (file, patpara, 256) != 256)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] error, read failed #3\n");
		return errFileRead;
	}
	for (i=0;i<128;i++)
		patpara[i] = uint16_little(patpara[i]);

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocPatterns(m, m->patnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum)||!mpAllocOrders(m, m->ordnum))
		return errAllocMem;


	for (i=0; i<m->ordnum; i++)
		m->orders[i]=i;

	for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
	{
		pp->patlen=64;
		if ((orders[t]!=255)&&(orders[t]<hdr.pats))
		{
			for (i=0; i<m->channum; i++)
				pp->tracks[i]=orders[t]*(m->channum+1)+i;
			pp->gtrack=orders[t]*(m->channum+1)+m->channum;
		} else {
			for (i=0; i<m->channum; i++)
				pp->tracks[i]=m->tracknum-1;
			pp->gtrack=m->tracknum-1;
		}
	}

	for (i=0; i<m->instnum; i++)
	{
		struct gmdinstrument *ip;
		struct gmdsample *sp;
		struct sampleinfo *sip;

		struct __attribute__((packed))
		{
			uint8_t type; /* 0:not used, 1:sample, 2:opl, 3:midi 4:loop, 8:pingpong, 10:16bit */
			char dosname[12];
			uint8_t volume;
			uint16_t samprate;
			uint16_t d1;
			uint32_t offset;
			uint32_t length;
			uint32_t loopstart;
			uint32_t loopend;
			uint32_t d2;
			uint32_t d3;
			uint32_t d4;
			uint8_t d5;
			uint8_t d7;
			char name[28];
			int32_t magic;
		} sins;

		if (file->read (file, &sins, sizeof (sins)) != sizeof (sins))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] error, read failed #4\n");
			return errFileRead;
		}
		sins.samprate  = uint16_little (sins.samprate);
		sins.d1        = uint16_little (sins.d1);
		sins.offset    = uint32_little (sins.offset);
		sins.length    = uint32_little (sins.length);
		sins.loopstart = uint32_little (sins.loopstart);
		sins.loopend   = uint32_little (sins.loopend);
		sins.d2        = uint32_little (sins.d2);
		sins.d3        = uint32_little (sins.d3);
		sins.d4        = uint32_little (sins.d4);
		sins.magic     = uint32_little (sins.magic);
		if ((sins.magic!=0x534D5450)&&(sins.magic!=0))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] invalid sample signature\n");
			return errFormStruc;
		}
		if (!i)
			patpara[hdr.pats]=sins.offset>>4;
		inspos[i]=sins.offset;

		if (sins.type&0x10)
		{
			sins.length>>=1;
			sins.loopstart>>=1;
			sins.loopend>>=1;
		}
		ip=&m->instruments[i];
		sp=&m->modsamples[i];
		sip=&m->samples[i];

		memcpy(ip->name, sins.name, 28);
		ip->name[28]=0;
		if (!(sins.type&3))
			continue;
		if ((sins.type&3)!=1)
			continue;

		for (j=0; j<128; j++)
			ip->samples[j]=i;

		memcpy(sp->name, sins.dosname, 12);
		sp->name[13]=0;
		sp->handle=i;
		sp->normnote=-cpifaceSession->mcpAPI->GetNote8363(sins.samprate);
		sp->stdvol=(sins.volume>0x3F)?0xFF:(sins.volume<<2);
		sp->stdpan=-1;
		sp->opt=(sins.type&0x10)?MP_OFFSETDIV2:0;

		sip->length=sins.length;
		sip->loopstart=sins.loopstart;
		sip->loopend=sins.loopend;
		sip->samprate=8363;
		sip->type=((sins.type&4)?mcpSampLoop:0)|((sins.type&8)?mcpSampBiDi:0)|((sins.type&0x10)?mcpSamp16Bit:0);
	}

	bufSize=1024;
	r.buffer=malloc(sizeof(uint8_t)*bufSize);
	r.temptrack=malloc(sizeof(uint8_t)*2000);
	if (!r.temptrack||!r.buffer)
	{
		FreeResources (&r);
		return errAllocMem;
	}

	for (t=0; t<hdr.pats; t++)
	{
		uint16_t patSize;

		uint8_t *tp;
		uint8_t *bp;
		uint8_t *cp;
		uint8_t row;
		struct gmdtrack *trk;
		uint16_t len;

		file->seek_set (file, patpara[t]*16);
		patSize=(patpara[t+1]-patpara[t])*16;
		if (patSize>bufSize)
		{
			bufSize=patSize;
			free(r.buffer);
			r.buffer=malloc(sizeof(uint8_t)*bufSize);
			if (!r.buffer)
			{
				FreeResources (&r);
				return errAllocMem;
			}
		}
		if (file->read (file, r.buffer, patSize) != patSize)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] error, read failed #5\n");
			return errFileRead;
		}

		for (j=0; j<m->channum; j++)
		{
			uint8_t *bp=r.buffer;
			uint8_t *tp=r.temptrack;

			uint8_t *cp=tp+2;
			char setorgpan=t==orders[0];
/*
			char setorgvwav=t==orders[0];
			char setorgpwav=t==orders[0];     NOT USED */
			int16_t row=0;
			struct gmdtrack *trk;
			uint16_t len;

			while (row<64)
			{
				int16_t nte;
				int16_t ins;
				int16_t vol;
				uint8_t command;
				uint8_t data;
				signed int pan;
				/* uint8_t pansrnd; */

				uint8_t c=*bp++;
				if (!c)
				{
					if (setorgpan)
					{
						putcmd(&cp, cmdPlayNote|cmdPlayPan, hdr.channels[j]*0x11);
						putcmd(&cp, cmdVolVibratoSetWave, 0x10);
						putcmd(&cp, cmdPitchVibratoSetWave, 0x10);
						setorgpan=0;
					}

					if (cp!=(tp+2))
					{
						tp[0]=row;
						tp[1]=cp-tp-2;
						tp=cp;
						cp=tp+2;
					}
					row++;
					continue;
				}
				if ((c&0x1F)!=j)
				{
					bp+=((c&0x20)>>4)+((c&0x40)>>5)+((c&0x80)>>7);
					continue;
				}
				nte=-1;
				ins=-1;
				vol=-1;
				command=0;
				data=0;
				pan=-1;
				/* pansrnd=0; */

				if (!row&&(t==orders[0]))
				{
					setorgpan=0;
					pan=hdr.channels[j]*0x11;
					putcmd(&cp, cmdVolVibratoSetWave, 0x10);
					putcmd(&cp, cmdPitchVibratoSetWave, 0x10);
				}

				if (c&0x20)
				{
					nte=*bp++;
					ins=*bp++-1;
					if ((nte<=120)||!nte)
						nte=nte+11;
					else {
						if (nte==254)
							putcmd(&cp, cmdNoteCut, 0);
						nte=-1;
					}
				}
				if (c&0x40)
				{
					command=*bp++;
					data=*bp++;
				}
				if (c&0x80)
				{
					vol=*bp++;
					vol=(vol>0x3F)?0xFF:(vol<<2);
				}

				if (command==0xC)
					vol=(data>0x3F)?0xFF:(data<<2);

				if ((command==0xE)&&((data>>4)==0x8))
					pan=(data&0xF)*0x11;

				if (((command==0x3)||(command==0x5))&&(nte!=-1))
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
					if ((command==0xE)&&((data>>4)==0xD))
					{
						*act|=cmdPlayDelay;
						*cp++=data&0xF;
					}
				}

/*        if (pansrnd)
 *          putcmd(cp, cmdPanSurround, 0);
 */

				switch (command)
				{
					case 0x0:
						if (data)
							putcmd(&cp, cmdArpeggio, data);
						break;
					case 0x1:
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContMixPitchSlideUp);
						else if (data<0xE0)
							putcmd(&cp, cmdPitchSlideUp, data);
						else if (data<0xF0)
							putcmd(&cp, cmdRowPitchSlideUp, (data&0xF)<<2);
						else
							putcmd(&cp, cmdRowPitchSlideUp, (data&0xF)<<4);
						break;
					case 0x2:
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContMixPitchSlideDown);
						else if (data<0xE0)
							putcmd(&cp, cmdPitchSlideDown, data);
						else if (data<0xF0)
							putcmd(&cp, cmdRowPitchSlideDown, (data&0xF)<<2);
						else
							putcmd(&cp, cmdRowPitchSlideDown, (data&0xF)<<4);
						break;
					case 0x3:
						putcmd(&cp, cmdPitchSlideToNote, data);
						break;
					case 0x4:
						putcmd(&cp, cmdPitchVibrato, data);
						break;
					case 0x5:
						putcmd(&cp, cmdPitchSlideToNote, 0);
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						if ((data&0x0F)&&(data&0xF0))
							data=0;
						if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else if (data&0x0F)
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;
					case 0x6:
						putcmd(&cp, cmdPitchVibrato, 0);
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						if ((data&0x0F)&&(data&0xF0))
							data=0;
						if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else if (data&0x0F)
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;
					case 0x7:
						putcmd(&cp, cmdVolVibrato, data);
						break;
					case 0x9:
						putcmd(&cp, cmdOffset, data);
						break;
					case 0xA:
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContMixVolSlide);
						else if ((data&0x0F)==0x00)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else if ((data&0xF0)==0x00)
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						else if ((data&0x0F)==0x0F)
							putcmd(&cp, cmdRowVolSlideUp, (data>>4)<<2);
						else if ((data&0xF0)==0xF0)
							putcmd(&cp, cmdRowVolSlideDown, (data&0xF)<<2);
						break;
					case 0xE:
						command=data>>4;
						data&=0x0F;
						switch (command)
						{
							case 0x1:
								putcmd(&cp, cmdRowPitchSlideUp, data<<4);
								break;
							case 0x2:
								putcmd(&cp, cmdRowPitchSlideDown, data<<4);
								break;
							case 0x3:
								putcmd(&cp, cmdSpecial, data?cmdGlissOn:cmdGlissOff);
								break;
							case 0x4:
								if (data<4)
									putcmd(&cp, cmdPitchVibratoSetWave, (data&3)+0x10);
								break;
							case 0x5: /* finetune */
								break;
							case 0x7:
								if (data<4)
									putcmd(&cp, cmdVolVibratoSetWave, (data&3)+0x10);
								break;
							case 0x9:
								if (data)
									putcmd(&cp, cmdRetrig, data);
								break;
							case 0xA:
								putcmd(&cp, cmdRowVolSlideUp, data<<2);
								break;
							case 0xB:
								putcmd(&cp, cmdRowVolSlideDown, data<<2);
								break;
							case 0xC:
								putcmd(&cp, cmdNoteCut, data);
								break;
						}
						break;
					case 0x11:
						putcmd(&cp, cmdRetrig, data);
						break;
					case 0x12:
						putcmd(&cp, cmdPitchVibratoFine, data);
						break;
					case 0x13: /* note slide down  xy x speed, y notecount */
						break;
					case 0x14: /* note slide up */
						break;
					case 0x15: /* note slide down + retrigger */
						break;
					case 0x16: /* note slide up + retrigger */
						break;
					case 0x17:
						putcmd(&cp, cmdOffsetEnd, data);
						break;
				}
			}

			trk=&m->tracks[t*(m->channum+1)+j];
			len=tp-r.temptrack;

			if (!len)
				trk->ptr=trk->end=0;
			else {
				trk->ptr=malloc(sizeof(uint8_t)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
				{
					FreeResources (&r);
					return errAllocMem;
				}
				memcpy(trk->ptr, r.temptrack, len);
			}
		}

		tp=r.temptrack;
		bp=r.buffer;
		cp=tp+2;

		if (t==orders[0])
		{
			/*      if (hdr.it!=6)
			 *        putcmd(cp, cmdTempo, hdr.it);
			 *      if (hdr.is!=125)
			 *        putcmd(cp, cmdSpeed, hdr.is);
			 */
		}

		row=0;
		while (row<64)
		{
			uint8_t c=*bp++;
			uint8_t command;
			uint8_t data;
			unsigned int curchan;

			if (!c)
			{
				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
					cp=tp+2;
				}
				row++;
				continue;
			}
			command=0;
			data=0;
			if (c&0x20)
				bp+=2;
			if (c&0x40)
			{
				command=*bp++;
				data=*bp++;
			}
			if (c&0x80)
				bp++;

			curchan=c&0x1F;
			if (curchan>=m->channum)
				continue;

			switch (command)
			{
				case 0xB:
					putcmd(&cp, cmdGoto, data);
					break;
				case 0xD:
					putcmd(&cp, cmdBreak, (data&0x0F)+(data>>4)*10);
					break;
				case 0xE:
					switch (data>>4)
					{
						case 0x6:
							putcmd(&cp, cmdSetChan, curchan);
							putcmd(&cp, cmdPatLoop, data&0xF);
							break;
						case 0xE:
							putcmd(&cp, cmdPatDelay, data&0xF);
							break;
					}
					break;
				case 0xF:
					if (data)
					{
						if (data<0x20)
							putcmd(&cp, cmdTempo, data);
						else
							putcmd(&cp, cmdSpeed, data);
					}
					break;
				case 0x10:
					data=(data>0x3F)?0xFF:(data<<2);
					putcmd(&cp, cmdGlobVol, data);
					break;
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
			{
				FreeResources (&r);
				return errAllocMem;
			}
			memcpy(trk->ptr, r.temptrack, len);
		}
	}
	FreeResources (&r);

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];    NOT USED */
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];
		char bit16;
		uint32_t slen;
		int8_t x;

		if (sp->handle==0xFFFF)
			continue;
		bit16=!!(sip->type&mcpSamp16Bit);

		slen=sip->length<<bit16;
		file->seek_set (file, inspos[i]);
		sip->ptr=malloc(sizeof(uint8_t)*(slen+16));
		if (!sip->ptr)
			return errAllocMem;
		if (file->read (file, sip->ptr, slen) != slen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/PTM] warning, read failed #6\n");
		}
		x=0;
		for (j=0; j<slen; j++)
			((unsigned char *)sip->ptr)[j]=x+=((unsigned char*)sip->ptr)[j];
	}

	return errOk;
}
