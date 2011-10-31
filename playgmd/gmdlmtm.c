/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMDPlay loader for MultiTracker modules
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
#include "dev/mcp.h"
#include "gmdplay.h"
#include "stuff/err.h"

struct LoadMTMResources
{
	uint8_t *temptrack;
	uint8_t *tbuffer;
	uint16_t (*trackseq)[32];
};

static inline void putcmd(uint8_t **p, uint8_t c, uint8_t d)
{
	*(*p)++=c;
	*(*p)++=d;
}

static void FreeResources (struct LoadMTMResources *r)
{
	if (r->temptrack)
	{
		free(r->temptrack);
		r->temptrack = 0;
	}
	if (r->tbuffer)
	{
		free(r->tbuffer);
		r->tbuffer = 0;
	}
	if (r->trackseq)
	{
		free(r->trackseq);
		r->trackseq = 0;
	}
}

static int _mpLoadMTM(struct gmdmodule *m, FILE *file)
{
	struct __attribute__((packed)) {
		uint32_t sig;
		char name[20];
		uint16_t trknum;
		uint8_t patnum;
		uint8_t ordnum;
		uint16_t comlen;
		uint8_t insnum;
		uint8_t attr;
		uint8_t patlen;
		uint8_t channum;
		char pan[32];
	} header;

	unsigned int i, t;

	uint8_t orders[128];

	struct gmdpattern *pp;
	uint32_t filetracks;
	struct LoadMTMResources r;

	r.temptrack = 0;
	r.tbuffer = 0;
	r.trackseq = 0;

	mpReset(m);

	if (fread(&header, 66, 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #1\n");

	if ((header.sig&0xFFFFFF)!=0x4D544D)
		return errFormSig;

	if ((header.sig&0xFF000000)!=0x10000000)
		return errFormOldVer;

	memcpy(m->name, header.name, 20);
	m->name[20]=0;

	m->options=0;
	m->channum=header.channum;
	m->modsampnum=m->sampnum=m->instnum=header.insnum;
	m->ordnum=header.ordnum+1;
	m->patnum=header.ordnum+1;
	m->endord=m->patnum;
	m->tracknum=(header.channum+1)*(header.patnum+1);
	m->loopord=0;

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocPatterns(m, m->patnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum)||!mpAllocOrders(m, m->ordnum))
		return errAllocMem;

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=i;

	for (i=0; i<m->instnum; i++)
	{

		struct gmdinstrument *ip;
		struct gmdsample *sp;
		struct sampleinfo *sip;

		struct __attribute__((packed)) {
			char name[22];
			uint32_t length;
			uint32_t loopstart;
			uint32_t loopend;
			int8_t finetune;
			uint8_t volume;
			uint8_t attr; /* 1=16 bit */
		} mi;
		if (fread(&mi, sizeof(mi), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #2\n");

		if (mi.length<4)
			mi.length=0;
		if (mi.loopend<4)
			mi.loopend=0;
		if (mi.attr&1)
		{
			mi.length>>=1;
			mi.loopstart>>=1;
			mi.loopend>>=1;
		}
		if (mi.finetune&0x08)
			mi.finetune|=0xF0;

		ip=&m->instruments[i];
		sp=&m->modsamples[i];
		sip=&m->samples[i];

		memcpy(ip->name, mi.name, 22);
		ip->name[22]=0;
		if (!mi.length)
			continue;
		for (t=0; t<128; t++)
			ip->samples[t]=i;
		*sp->name=0;
		sp->handle=i;
		sp->normnote=-mi.finetune*32;
		sp->stdvol=(mi.volume>=0x3F)?0xFF:(mi.volume<<2);
		sp->stdpan=-1;
		sp->opt=0;

		sip->loopstart=mi.loopstart;
		sip->loopend=mi.loopend;
		sip->length=mi.length;
		sip->samprate=8363;
		sip->type=((mi.loopend)?mcpSampLoop:0)|((mi.attr&1)?mcpSamp16Bit:0)|mcpSampUnsigned;
	}

	if (fread(orders, 128, 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #3\n");

	for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
	{
		pp->patlen=header.patlen;
		for (i=0; i<m->channum; i++)
			pp->tracks[i]=orders[t]*(m->channum+1)+i;
		pp->gtrack=orders[t]*(m->channum+1)+m->channum;
	}

	filetracks=ftell(file);

	r.temptrack=malloc(sizeof(uint8_t)*2000);
	r.tbuffer=malloc(sizeof(uint8_t)*(192*header.trknum+192));
	r.trackseq=malloc(sizeof(uint16_t)*(header.patnum+1)*32);
	if (!r.tbuffer||!r.temptrack||!r.trackseq)
	{
		FreeResources (&r);
		return errAllocMem;
	}

	memset(r.tbuffer, 0, 192);
	if (fread(r.tbuffer+192, 192*header.trknum, 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #4\n");
	if (fread(r.trackseq, 64*(header.patnum+1), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #5\n");

	for (t=0; t<=header.patnum; t++)
	{
		uint8_t *buffer[32];
		uint8_t *tp;
		uint8_t *buf;
		uint8_t row;
		struct gmdtrack *trk;
		uint16_t len;

		for (i=0; i<m->channum; i++)
		{
			uint8_t *tp;
			uint8_t *buf;
			uint8_t row;
			struct gmdtrack *trk;
			uint16_t len;

			buffer[i]=r.tbuffer+192*r.trackseq[t][i]; /* needs checks, since this can CRASH player on broken modules TODO */

			tp=r.temptrack;
			buf=buffer[i]; /* needs checks, since this can CRASH player on broken modules TODO */

			for (row=0; row<64; row++, buf+=3)
			{
				uint8_t *cp=tp+2;

				int16_t nte=(buf[0]>>2);
				int16_t vol=-1;
				int16_t pan=-1;
				int16_t ins=(((buf[0]&0x03)<<4)|((buf[1]&0xF0)>>4))-1;
				uint8_t command=buf[1]&0xF;
				uint8_t data=buf[2];
				uint8_t pansrnd=0;

				if (command==0xE)
				{
					command=(data&0xF0)|0xE;
					data&=0xF;
				}

				if (!row&&(t==orders[0]))
					pan=(header.pan[i]&0xF)+((header.pan[i]&0xF)<<4);

				if (nte)
					nte+=36;
				else
					nte=-1;

				if (command==0xC)
					vol=(data>0x3F)?0xFF:(data<<2);

				if (command==0x8)
				{
					pan=data;
					if (pan==164)
						pan=0xC0;
					if (pan>0x80)
					{
						pan=0x100-pan;
						pansrnd=1;
					}
					pan=(pan==0x80)?0xFF:(pan<<1);
				}

				if (command==0x8E)
					pan=data*0x11;

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
					if (command==0xDE)
					{
						*act|=cmdPlayDelay;
						*cp++=data;
					}
				}

				if (pansrnd)
					putcmd(&cp, cmdPanSurround, 0);

				switch (command)
				{
					case 0x0:
						if (data)
							putcmd(&cp, cmdArpeggio, data);
						break;
					case 0x1:
						if (data)
							putcmd(&cp, cmdPitchSlideUp, data);
						break;
					case 0x2:
						if (data)
							putcmd(&cp, cmdPitchSlideDown, data);
						break;
					case 0x3:
						putcmd(&cp, cmdPitchSlideToNote, data);
						break;
					case 0x4:
						putcmd(&cp, cmdPitchVibrato, data);
						break;
					case 0x5:
						if ((data&0x0F)&&(data&0xF0))
							data=0;
						putcmd(&cp, cmdPitchSlideToNote, 0);
						if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else
							if (data&0x0F)
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;
					case 0x6:
						if ((data&0x0F)&&(data&0xF0))
							data=0;
						putcmd(&cp, cmdPitchVibrato, 0);
						if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else
							if (data&0x0F)
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;
					case 0x7:
						putcmd(&cp, cmdVolVibrato, data);
						break;
					case 0x9:
						putcmd(&cp, cmdOffset, data);
						break;
					case 0xA:
						if ((data&0x0F)&&(data&0xF0))
							data=0;
						if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else
							if (data&0x0F)
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;
					case 0x1E:
						if (data)
							putcmd(&cp, cmdRowPitchSlideUp, data<<4);
						break;
					case 0x2E:
						if (data)
							putcmd(&cp, cmdRowPitchSlideDown, data<<4);
						break;
					case 0x3E:
						putcmd(&cp, cmdSpecial, data?cmdGlissOn:cmdGlissOff);
						break;
					case 0x4E:
						if (data<4)
							putcmd(&cp, cmdPitchVibratoSetWave, data);
						break;
					case 0x7E:
						if (data<4)
							putcmd(&cp, cmdVolVibratoSetWave, data);
						break;
					case 0x9E:
						if (data)
							putcmd(&cp, cmdRetrig, data);
						break;
					case 0xAE:
						if (data)
							putcmd(&cp, cmdRowVolSlideUp, data<<2);
						break;
					case 0xBE:
						if (data)
							putcmd(&cp, cmdRowVolSlideDown, data<<2);
						break;
					case 0xCE:
						putcmd(&cp, cmdNoteCut, data);
						break;
				}

				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
				}
			}

			trk=&m->tracks[t*(m->channum+1)+i];
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
		for (row=0; row<64; row++)
		{
			uint8_t *cp=tp+2;
			for (i=0; i<m->channum; i++)
			{
				uint8_t command;
				uint8_t data;

				buf=buffer[i]+row*3;
				command=buf[1]&0xF;
				data=buf[2];
				if (command==0xE)
				{
					command=(data&0xF0)|0xE;
					data&=0xF;
				}
				switch (command)
				{
					case 0xB:
						putcmd(&cp, cmdGoto, data);
						break;
					case 0xD:
						if (data>=0x64)
							data=0;
						putcmd(&cp, cmdBreak, (data&0xF)+(data>>4)*10);
						break;
					case 0x6E:
						putcmd(&cp, cmdSetChan, i);
						putcmd(&cp, cmdPatLoop, data);
						break;
					case 0xEE:
						putcmd(&cp, cmdPatDelay, data);
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
			{
				FreeResources (&r);
				return errAllocMem;
			}
			memcpy(trk->ptr, r.temptrack, len);
		}
	}

	FreeResources (&r);

	if (header.comlen&&!(header.comlen%40))
	{
		header.comlen/=40;
		m->message=malloc(sizeof(char *)*(header.comlen+1));
		if (!m->message)
			return errAllocMem;
		*m->message=malloc(sizeof(char)*(header.comlen*41));
		if (!*m->message)
			return errAllocMem;
		for (t=0; t<header.comlen; t++)
		{
			int16_t xxx;

			m->message[t]=m->message[0]+t*41;
			if (fread(m->message[t], 40, 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #6\n");
			for (xxx=0; xxx<40; xxx++)
				if (!m->message[t][xxx])
					m->message[t][xxx]=' ';
			m->message[t][40]=0;
		}
		m->message[header.comlen]=0;
	} else
		fseek(file, header.comlen, SEEK_CUR);

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];   NOT USED */
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];
		uint32_t l;

		if (sp->handle==0xFFFF)
			continue;
		l=sip->length<<(!!(sip->type&mcpSamp16Bit));
		sip->ptr=malloc(sizeof(char)*(l+16));
		if (!sip->ptr)
			return errAllocMem;
		if (fread(sip->ptr, l, 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #7\n");
	}

	return errOk;
}

struct gmdloadstruct mpLoadMTM = { _mpLoadMTM };

struct linkinfostruct dllextinfo = {"gmdlmtm", "OpenCP Module Loader: *.MTM (c) 1994-09 Niklas Beisert", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
