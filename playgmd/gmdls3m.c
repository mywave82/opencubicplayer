/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for ScreamTracker ]I[ modules
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
 *  -kb980717 Tammo Hinrichs <kb@nwn.de>
 *    -re-enabled and fixed pattern reordering to finally get rid
 *     of the mysterious SCB (Skaven Crash Bug)  ;)
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

OCP_INTERNAL int LoadS3M (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	unsigned int t,i;
	uint8_t orders[256];
	uint16_t inspara[256];
	uint16_t patpara[256];
	uint32_t smppara[256];
	uint8_t defpan[32];
	int16_t t2;
	struct gmdpattern *pp;

	int bufsize;
	uint8_t *buffer;
	uint8_t *temptrack;
	char chanused[32];

	struct __attribute__((packed))
	{
		char name[28];
		uint8_t end,type;
		uint16_t d1;
		uint16_t orders,ins,pats,flags,cwt,ffv;
		char magic[4];
		uint8_t gv,it,is,mv,uc,dp;
		uint32_t d2;
		uint32_t d3;
		uint16_t special;
		uint8_t channels[32];
	} hdr;

	mpReset(m);

#ifdef S3M_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "Reading header: %d bytes\n", (int)sizeof(hdr));
#endif

	if (file->read (file, &hdr, sizeof(hdr)) != sizeof (hdr))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #1\n");
	}
	hdr.d1      = uint16_little (hdr.d1);
	hdr.orders  = uint16_little (hdr.orders);
	hdr.ins     = uint16_little (hdr.ins);
	hdr.pats    = uint16_little (hdr.pats);
	hdr.flags   = uint16_little (hdr.flags);
	hdr.cwt     = uint16_little (hdr.cwt);
	hdr.ffv     = uint16_little (hdr.ffv);
	hdr.d2      = uint32_little (hdr.d2);
	hdr.d3      = uint32_little (hdr.d3);
	hdr.special = uint16_little (hdr.special);
	if (memcmp(hdr.magic, "SCRM", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Invalid signature\n");
		return errFormSig;
	}
	if ((hdr.orders>255)||(hdr.pats>=254))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] too many orders and/or patterns\n");
		return errFormStruc;
	}

	memcpy(m->name, hdr.name, 28);
	m->name[28]=0;

	m->channum=0;
	for (t=0; t<32; t++)
		if (hdr.channels[t]!=0xFF)
			m->channum=t+1;
	m->modsampnum=m->sampnum=m->instnum=hdr.ins;
	m->patnum=hdr.orders;
	m->tracknum=hdr.pats*(m->channum+1)+1;
	m->options=MOD_S3M|((((hdr.cwt&0xFFF)<=0x300)||(hdr.flags&64))?MOD_S3M30:0);
	m->loopord=0;

	for (t=0; t<m->channum; t++)
	  if (((hdr.channels[t]&8)>>2)^((t+t+t)&2))
		  break;
	if (t==m->channum)
		m->options|=MOD_MODPAN;

#ifdef S3M_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Reading pattern orders, %d bytes\n", (int)m->patnum);
#endif
	if (file->read (file, orders, m->patnum) != m->patnum)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #2\n");
	}
	for (t=m->patnum-1; (signed)t>=0; t--)
	{
		if (orders[t]<254)
			break;
		m->patnum--;
	}
	if (!m->patnum)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] No patterns\n");
		return errFormMiss;
	}

	t2=0;
	for (t=0; t<m->patnum; t++)
	{
		orders[t2]=orders[t];
		if (orders[t]!=254)
			t2++;
	}
	m->patnum=t2;

	m->ordnum=m->patnum;

	for (t=0; t<m->patnum; t++)
		if (orders[t]==255)
			break;
	m->endord=t;

#ifdef S3M_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Reading instruments-parameters, %d bytes\n", (int)m->instnum*2);
#endif
	if (file->read (file, inspara, m->instnum*2) != (m->instnum * 2))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #2\n");
	}
	for (i=0;i<(m->instnum*2);i++)
		inspara[i] = uint16_little (inspara[i]);
#ifdef S3M_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Reading pattern-parameters, %d bytes\n", (int)hdr.pats*2);
#endif
	if (file->read (file, patpara, hdr.pats*2) != (hdr.pats * 2))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #3\n");
	}
	for (i=0;i<(hdr.pats*(unsigned)2);i++)
		patpara[i] = uint16_little (patpara[i]);

	/*  hdr.mv|=0x80; */
	for (i=0; i<32; i++)
		defpan[i]=(hdr.mv&0x80)?((hdr.channels[i]&8)?0x2F:0x20):0;
	if (hdr.dp==0xFC)
	{
#ifdef S3M_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Reading default panning, 32 bytes\n");
#endif
		if (file->read (file, defpan, 32) != 32)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #4\n");
		}
	}
	for (i=0; i<32; i++)
		defpan[i]=(defpan[i]&0x20)?((defpan[i]&0xF)*0x11):((hdr.mv&0x80)?((hdr.channels[i]&8)?0xCC:0x33):0x80);

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocPatterns(m, m->patnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum)||!mpAllocOrders(m, m->ordnum))
	{
#ifdef S3M_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Out of mem #1\n");
#endif
		return errAllocMem;
	}

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=(orders[i]==254)?0xFFFF:i;

	for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
	{
		if (orders[t]==254)
			continue;
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
		unsigned int j;

		struct __attribute__((packed))
		{
			uint8_t type;
			char dosname[12];
			uint8_t sampptrh;
			uint16_t sampptr;
			uint32_t length;
			uint32_t loopstart;
			uint32_t loopend;
			uint8_t volume;
			char d1;
			uint8_t pack;
			uint8_t flag;
			uint32_t c2spd;
			char d2[12];
			char name[28];
			int32_t magic;
		} sins;

#ifdef S3M_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Seeking to (%d/%d)  %d\n", i, (int)m->instnum, (int)((int32_t)inspara[i])*16);
#endif
		file->seek_set (file, ((int32_t)inspara[i])*16);
#ifdef S3M_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Reading %d bytes of instrument header\n", (int)sizeof(sins));
#endif
		if (file->read (file, &sins, sizeof (sins)) != sizeof (sins))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #5\n");
		}
		sins.sampptr   = uint16_little (sins.sampptr);
		sins.length    = uint32_little (sins.length);
		sins.loopstart = uint32_little (sins.loopstart);
		sins.loopend   = uint32_little (sins.loopend);
		sins.c2spd     = uint32_little (sins.c2spd);
		sins.magic     = uint32_little (sins.magic);
		if (sins.magic==0x49524353)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] adlib sample detected, use OPL playback plugin instead\n");
			return errFormStruc;
		}
		if ((sins.magic!=0x53524353)&&(sins.magic!=0))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Invalid magic on sample (0x%08lx)\n", (unsigned long int)sins.magic);
			return errFormStruc;
		}
		smppara[i]=sins.sampptr+(sins.sampptrh<<16);

		if (sins.flag&4)
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
		if (!sins.length)
			continue;
		if (sins.type!=1)
		{
			/* type 0 = message / empty sample, 2-7 are different adlib types */
			if (sins.type)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] non-PCM type sample, try to use OPL playback plugin instead\n");
				return errFormStruc;
			}
			continue;
		}
		if (sins.pack)
			continue;
		if (sins.flag&2)
			continue;

		sp->handle=i;
		for (j=0; j<128; j++)
			ip->samples[j]=i;
		memcpy(sp->name, sins.dosname, 12);
		sp->name[12]=0;
		sp->normnote=-cpifaceSession->mcpAPI->GetNote8363(sins.c2spd);
		sp->stdvol=(sins.volume>0x3F)?0xFF:(sins.volume<<2);
		sp->stdpan=-1;
		sp->opt=0;

		sip->length=sins.length;
		sip->loopstart=sins.loopstart;
		sip->loopend=sins.loopend;
		sip->samprate=8363;
		sip->type=((sins.flag&1)?mcpSampLoop:0)|((sins.flag&4)?mcpSamp16Bit:0)|((hdr.ffv==1)?0:mcpSampUnsigned);
	}

	bufsize=1024;
	if (!(buffer=malloc(sizeof(uint8_t)*bufsize)))
	{
#ifdef S3M_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Out of mem #2\n");
#endif
		return errAllocMem;
	}
	if (!(temptrack=malloc(sizeof(uint8_t)*5000))) /* a full-blown pattern with maxed out channels and globals can fill 4224 bytes */
	{
#ifdef S3M_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Out of mem #3\n");
#endif
		free(buffer);
		return errAllocMem;
	}

	memset(chanused, 0, 32);

	for (t=0; t<hdr.pats; t++)
	{
		uint16_t patsize;
		uint16_t j;
		uint8_t *tp;
		uint8_t *bp;
		uint8_t *cp;
		uint8_t row;
		struct gmdtrack *trk;
		uint16_t len;

		file->seek_set (file, patpara[t]*16);
		if (ocpfilehandle_read_uint16_le (file, &patsize))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #6\n");
		}
		if (patsize>bufsize)
		{
			void *new;
			bufsize=patsize;
			if (!(new=realloc(buffer, sizeof(uint8_t)*bufsize)))
			{
#ifdef S3M_LOAD_DEBUG
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Out of mem #4\n");
#endif
				free(buffer);
				free(temptrack);
				return errAllocMem;
			}
			buffer=new;
		}
		if (file->read (file, buffer, patsize) != patsize)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #7\n");
		}

		for (j=0; j<m->channum; j++)
		{
			char setorgpan=t==orders[0];
/*
			char setorgvwav=t==orders[0];
			char setorgpwav=t==orders[0]; NOT USED
*/
			int16_t row=0;

			tp=temptrack;
			cp=tp+2;

			bp=buffer;

			while (row<64)
			{
				int16_t nte;
				int16_t ins;
				int16_t vol;
				uint8_t command;
				uint8_t data;
				signed int pan;
				uint8_t pansrnd;

				uint8_t c=*bp++;

				if (!c)
				{
					if (setorgpan)
					{
						putcmd(&cp, cmdPlayNote|cmdPlayPan, defpan[j]);
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
				if (((c&0x1F)!=j)||(hdr.channels[j]==0xFF))
				{
					bp+=((c&0x20)>>4)+((c&0xC0)>>6);
					continue;
				}
				chanused[j]=1;
				nte=-1;
				ins=-1;
				vol=-1;
				command=0;
				data=0;
				pan=-1;
				pansrnd=0;

				if (!row&&(t==orders[0]))
				{
					setorgpan=0;
					pan=defpan[j];
					putcmd(&cp, cmdVolVibratoSetWave, 0x10);
					putcmd(&cp, cmdPitchVibratoSetWave, 0x10);
				}

				if (c&0x20)
				{
					nte=*bp++;
					ins=*bp++-1;
					if (nte<254)
						nte=(nte>>4)*12+(nte&0x0F)+12;
					else {
						if (nte==254)
						{
							putcmd(&cp, cmdNoteCut, 0);
						}
						nte=-1;
					}
				}
				if (c&0x40)
				{
					vol=*bp++;
					vol=(vol>0x3F)?0xFF:(vol<<2);
				}
				if (c&0x80)
				{
					command=*bp++;
					data=*bp++;
				}

				if (command==0x18)
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

				if ((command==0x13)&&((data>>4)==0x8))
					pan=(data&0xF)+((data&0xF)<<4);

				if (((command==0x7)||(command==0xC))&&(nte!=-1))
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
					if ((command==0x13)&&((data>>4)==0xD))
					{
						*act|=cmdPlayDelay;
						*cp++=data&0xF;
					}
				}

				if (pansrnd)
				{
					putcmd(&cp, cmdPanSurround, 0);
				}

				switch (command)
				{
					case 0x04:
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContMixVolSlide);
						else if (((data & 0xF0) == 0xF0) && (data & 0x0F)) /* fine-slide */
							putcmd(&cp, cmdRowVolSlideDown, (data & 0x0F)<<2);
						else if (((data & 0x0F) == 0x0F) && (data & 0xF0))
							putcmd(&cp, cmdRowVolSlideUp, (data & 0xF0)>>2);
						else if (data&0x0F)
							putcmd(&cp, cmdVolSlideDown, (data&0x0F)<<2);
						else
							putcmd(&cp, cmdVolSlideUp, (data & 0xF0)>>2);
						break;
					case 0x05:
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContMixPitchSlideDown);
						else if (data<0xE0)
							putcmd(&cp, cmdPitchSlideDown, data);
						else if (data<0xF0)
							putcmd(&cp, cmdRowPitchSlideDown, (data&0xF)<<2);
						else
							putcmd(&cp, cmdRowPitchSlideDown, (data&0xF)<<4);
						break;
					case 0x06:
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContMixPitchSlideUp);
						else if (data<0xE0)
							putcmd(&cp, cmdPitchSlideUp, data);
						else if (data<0xF0)
							putcmd(&cp, cmdRowPitchSlideUp, (data&0xF)<<2);
						else
							putcmd(&cp, cmdRowPitchSlideUp, (data&0xF)<<4);
						break;
					case 0x07:
						putcmd(&cp, cmdPitchSlideToNote, data);
						break;
					case 0x08:
						putcmd(&cp, cmdPitchVibrato, data);
						break;
					case 0x09:
						putcmd(&cp, cmdTremor, data);
						break;
					case 0x0A:
						putcmd(&cp, cmdArpeggio, data);
						break;
					case 0x0B:
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
					case 0x0C:
						putcmd(&cp, cmdPitchSlideToNote, 0);
						if (!data)
							putcmd(&cp, cmdSpecial, cmdContVolSlide);
						if ((data&0x0F)&&(data&0xF0))
							data=0;
						if (data&0xF0)
							putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
						else
							if (data&0x0F)
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						break;
					case 0x0F:
						putcmd(&cp, cmdOffset, data);
						break;
					case 0x11:
						putcmd(&cp, cmdRetrig, data);
						break;
					case 0x12:
						putcmd(&cp, cmdVolVibrato, data);
						break;
					case 0x13:
						command=data>>4;
						data&=0x0F;
						switch (command)
						{
							case 0x1:
								putcmd(&cp, cmdSpecial, data?cmdGlissOn:cmdGlissOff);
								break;
							case 0x2:
								break; /* TODO: SET FINETUNE not ok (see protracker)  */
							case 0x3:
								putcmd(&cp, cmdPitchVibratoSetWave, (data&3)+0x10);
								/* TODO data & 0x04 == 0x00, reset waveform on note-hit, else it is sticky */
								break;
							case 0x4:
								putcmd(&cp, cmdVolVibratoSetWave, (data&3)+0x10);
								/* TODO data & 0x04 == 0x00, reset waveform on note-hit, else it is sticky */
								break;
							case 0x8:
								putcmd(&cp, cmdRowPanSlide, (data&0x0f) | ((data&0x0f) << 4));
							case 0x9:
								if (data == 0x01)
								{
									putcmd (&cp, cmdPanSurround, 0);
								}
								break;
							case 0xC:
								putcmd(&cp, cmdNoteCut, data);
								break;
						}
						break;
					case 0x15:
						putcmd(&cp, cmdPitchVibratoFine, data);
						break;
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
#ifdef S3M_LOAD_DEBUG
					cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Out of mem #5\n");
#endif
					free(buffer);
					free(temptrack);
					return errAllocMem;
				}
				memcpy(trk->ptr, temptrack, len);
			}
		}

		tp=temptrack;
		bp=buffer;
		cp=tp+2;

		if (t==orders[0])
		{
			if (hdr.it!=6)
				putcmd(&cp, cmdTempo, hdr.it);
			if (hdr.is!=125)
				putcmd(&cp, cmdSpeed, hdr.is);
			if (hdr.gv!=0x40)
				putcmd(&cp, cmdGlobVol, hdr.gv*4);
		}

		row=0;
		while (row<64)
		{
			uint8_t command=0;
			uint8_t data=0;
			unsigned int curchan;

			uint8_t c=*bp++;
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
				bp++;
			if (c&0x80)
			{
				command=*bp++;
				data=*bp++;
			}

			curchan=c&0x1F;
			if (curchan>=m->channum)
				continue;

			switch (command)
			{
				case 0x01:
					if (data)
						putcmd(&cp, cmdTempo, data);
					break;
				case 0x02:
					if (data<m->ordnum)
						putcmd(&cp, cmdGoto, data);
					break;
				case 0x03:
					if (data>=0x64)
						data=0;
					putcmd(&cp, cmdBreak, (data&0x0F)+(data>>4)*10);
					break;
				case 0x13:
					command=data>>4;
					data&=0x0F;
					switch (command)
					{
						case 0xB:
							putcmd(&cp, cmdSetChan, curchan);
							putcmd(&cp, cmdPatLoop, data);
							break;
						case 0xE:
							if (data)
								putcmd(&cp, cmdPatDelay, data);
							break;
					}
					break;
				case 0x14:
					if (data>=0x20)
						putcmd(&cp, cmdSpeed, data);
					break;
				case 0x16:
					data=(data>0x3F)?0xFF:(data<<2);
					putcmd(&cp, cmdGlobVol, data);
					break;
			}
		}

		trk=&m->tracks[t*(m->channum+1)+m->channum];
		len=tp-temptrack;

		if (!len)
			trk->ptr=trk->end=0;
		else {
			trk->ptr=malloc(sizeof(uint8_t)*len);
			trk->end=trk->ptr+len;
			if (!trk->ptr)
			{
#ifdef S3M_LOAD_DEBUG
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Out of mem #6\n");
#endif
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
/*
		struct gmdinstrument *ip=&m->instruments[i];          NOT USED */
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];
		int l;
		if (sp->handle==0xFFFF)
			continue;

		l=((sip->type&mcpSamp16Bit)?2:1)*sip->length;
		file->seek_set (file, smppara[i]*16);
		sip->ptr=malloc(sizeof(int8_t)*(l+16));
		if (!sip->ptr)
		{
#ifdef S3M_LOAD_DEBUG
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] Out of mem #7\n");
#endif
			return errAllocMem;
		}
		if (file->read (file, sip->ptr, l) != l)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] warning, read failed #8\n");
		}
	}

	for (i=m->channum-1; (signed)i>=0; i--)
	{
		if (chanused[i])
			break;
		m->channum--;
	}

	if (!m->channum)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/S3M] No channels left after optimize\n");
		return errFormMiss;
	}

	return errOk;
}
