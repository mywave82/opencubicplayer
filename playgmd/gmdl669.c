/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for Composer 669 modules
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

OCP_INTERNAL int Load669 (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	unsigned int t;

	struct __attribute__((packed)) {
		uint16_t sig;
		char msg[108];
		uint8_t insnum;
		uint8_t patnum;
		uint8_t loop;
		uint8_t orders[0x80];
		uint8_t tempo[0x80];
		uint8_t patlen[0x80];
	} hdr;

	char *msg;

	unsigned int i,j;

	struct gmdpattern *pp;

	uint8_t *buffer;
	uint8_t *temptrack;
	char chanused[8];

	uint8_t commands[8];
	uint8_t data[8];

	mpReset(m);

	if (file->read (file, &hdr, sizeof(hdr)) != sizeof(hdr))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/669] read failed #1\n");
		return errFileRead;
	}

	if ((hdr.sig!=*(uint16_t *)"if")&&(hdr.sig!=*(uint16_t *)"JN"))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/669] invalid signature\n");
		return errFormSig;
	}

	memcpy(m->name, hdr.msg, 31);
	m->name[31]=0;

	m->channum=8;
	m->instnum=hdr.insnum;
	m->sampnum=hdr.insnum;
	m->modsampnum=hdr.insnum;
	m->options=0;
	m->loopord=hdr.loop;

	m->patnum=0x80;
	for (t=0x7F; (signed)t>=0; t--)
	{
		if (hdr.orders[t]<hdr.patnum)
			break;
		m->patnum--;
	}
	if (!m->patnum)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/669] no patterns\n");
		return errFormMiss;
	}
	m->ordnum=m->patnum;
	m->endord=m->patnum;

	m->tracknum=m->patnum*9;

	m->message=malloc(sizeof(char *)*4);
	if (!mpAllocInstruments(m, m->instnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocPatterns(m, m->patnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum)||!m->message||!mpAllocOrders(m, m->ordnum))
		return errAllocMem;

	msg=malloc(sizeof(char)*111);
	if (!msg)
		return errAllocMem;

	m->message[0]=msg;
	m->message[1]=msg+37;
	m->message[2]=msg+74;
	m->message[3]=0;
	memcpy(m->message[0], hdr.msg, 36);
	m->message[0][36]=0;
	memcpy(m->message[1], hdr.msg+36, 36);
	m->message[1][36]=0;
	memcpy(m->message[2], hdr.msg+72, 36);
	m->message[2][36]=0;

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=i;

	for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
	{
		pp->patlen=hdr.patlen[hdr.orders[t]]+1;
		for (i=0; i<8; i++)
			pp->tracks[i]=t*9+i;
		pp->gtrack=t*9+8;
	}

	for (i=0; i<m->instnum; i++)
	{
		struct __attribute__((packed)) {
			char name[13];
			uint32_t length;
			uint32_t loopstart;
			uint32_t loopend;
		} sins;

		struct gmdinstrument *ip;
		struct gmdsample *sp;
		struct sampleinfo *sip;

		if (file->read (file, &sins, sizeof(sins)) != sizeof(sins))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/669] warning, read failed #2\n");
		}

		sins.length = uint32_little (sins.length);
		sins.loopstart = uint32_little (sins.length);
		sins.loopend = uint32_little (sins.loopend);

		ip=&m->instruments[i];
		sp=&m->modsamples[i];
		sip=&m->samples[i];

		memcpy(ip->name, sins.name, 13);
		if (!sins.length)
			continue;

		for (j=0; j<128; j++)
			ip->samples[j]=i;

		*sp->name=0;
		sp->handle=i;
		sp->normnote=0;
		sp->stdvol=-1;
		sp->stdpan=-1;
		sp->opt=0;

		sip->length=sins.length;
		sip->loopstart=sins.loopstart;
		sip->loopend=sins.loopend;
		sip->samprate=8448; /* ?? */
		sip->type=((sins.loopend<=sins.length)?mcpSampLoop:0)|mcpSampUnsigned;
	}

	buffer=malloc(sizeof(uint16_t)*0x600*hdr.patnum);
	if (!buffer)
		return errAllocMem;

	temptrack=malloc(sizeof(uint8_t)*2000);
	if (!temptrack)
	{
		free(buffer);
		return errAllocMem;
	}

	memset(chanused, 0, 8);

	if (file->read (file, buffer, 0x600 * hdr.patnum) != ( 0x600 * hdr.patnum) )
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/669] warning, read failed #3\n");
	}

	for (t=0; t<8; t++)
		commands[t]=0xFF;

	for (t=0; t<m->patnum; t++)
	{
		int j;

		uint8_t *tp;
		uint8_t *bp;

		uint8_t row;
		struct gmdtrack *trk;
		uint16_t len;

		for (j=0; j<8; j++)
		{
			uint8_t *bp=buffer+j*3+0x600*hdr.orders[t];
			uint8_t *tp=temptrack;

			int16_t row;

			struct gmdtrack *trk;
			uint16_t len;

			for (row=0; row<=hdr.patlen[hdr.orders[t]]; row++, bp+=24)
			{
				uint8_t *cp=tp+2;

				int16_t ins=-1;
				int16_t nte=-1;
				int16_t pan=-1;
				int16_t vol=-1;

				if (bp[0]<0xFE)
				{
					ins=((bp[0]<<4)|(bp[1]>>4))&0x3F;
					nte=(bp[0]>>2)+36;
					commands[j]=0xFF;
				}

				if (bp[0]!=0xFF)
					vol=(bp[1]&0xF)*0x11;

				if (bp[2]!=0xFF)
				{
					commands[j]=bp[2]>>4;
					data[j]=bp[2]&0xF;
					if (!data[j])
						commands[j]=0xFF;
					if (commands[j]==2)
					{
						ins=-1;
						nte|=128;
					}
				}

				if ((bp[0]!=0xFF)||(bp[2]!=0xFF))
					chanused[j]=1;

				if (!row&&!t)
					pan=(j&1)?0xFF:0;

				if ((bp[0]!=0xFF)||(pan!=-1))
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
				}

				switch (commands[j])
				{
					case 0:
						putcmd(&cp, cmdPitchSlideUp, data[j]);
						break;
					case 1:
						putcmd(&cp, cmdPitchSlideDown, data[j]);
						break;
					case 2:
						putcmd(&cp, cmdPitchSlideToNote, data[j]);
						break;
					case 3:
						putcmd(&cp, cmdRowPitchSlideUp, data[j]<<2); /* correct? down? both? */
						break;
					case 4:
						putcmd(&cp, cmdPitchVibrato, (data[j]<<4)|1);
						break;
				}

				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
				}
			}

			trk=&m->tracks[t*9+j];
			len=tp-temptrack;

			if (!len)
				trk->ptr=trk->end=0;
			else {
				trk->ptr=malloc(sizeof(uint8_t)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
				{
					free(buffer);
					free(temptrack);
					return errAllocMem;
				}
				memcpy(trk->ptr, temptrack, len);
			}
		}

		tp=temptrack;
		bp=buffer+0x600*hdr.orders[t];

		for (row=0; row<=hdr.patlen[hdr.orders[t]]; row++)
		{
			uint8_t *cp=tp+2;
			uint8_t q;

			if (!row)
			{
				if (!t)
					putcmd(&cp, cmdSpeed, 78);
				putcmd(&cp, cmdTempo, hdr.tempo[hdr.orders[t]]);
			}

			for (q=0; q<8; q++, bp+=3)
			{
				if ((bp[2]>>4)==5)
					if (bp[2]&0xF)
						putcmd(&cp, cmdTempo, bp[2]&0xF);
			}

			if (cp!=(tp+2))
			{
				tp[0]=row;
				tp[1]=cp-tp-2;
				tp=cp;
			}
		}

		trk=&m->tracks[t*9+8];
		len=tp-temptrack;

		if (!len)
			trk->ptr=trk->end=0;
		else {
			trk->ptr=malloc(sizeof(uint8_t)*(len+8));
			trk->end=trk->ptr+len;
			if (!trk->ptr)
			{
				free(buffer);
				free(temptrack);
				return errAllocMem;
			}
			memcpy(trk->ptr, temptrack, len);
		}
	}
	free(buffer);
	free(temptrack);

	for (i=0; i<m->instnum; i++)
	{
		/*struct gmdinstrument *ip=&m->instruments[i];              NOT USED */
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];

		if (sp->handle==0xFFFF)
			continue;

		sip->ptr=malloc(sizeof(char)*sip->length);
		if (!sip->ptr)
			return errAllocMem;
		if (file->read (file, sip->ptr, sip->length) != sip->length)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/669] warning, read failed #5\n");
		}
	}

/*  for (i=m.channum-1; i>=0; i--)
 *  {
 *    if (chanused[i])
 *      break;
 *    m.channum--;
 *  }
 * if (!m.channum)
 *    return MP_LOADFILE;
 */

	return errOk;
}
