/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for Oktalyzer modules
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
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

struct LoadOKTResources
{
	uint8_t *temptrack;
	uint8_t *buffer;
};

static void FreeResources(struct LoadOKTResources *r)
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

OCP_INTERNAL int LoadOKT (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{

	uint8_t hsig[8];
	struct __attribute__((packed))
	{
		uint8_t sig[4];
		uint32_t blen;
	} chunk;
	uint16_t cflags[4];
	uint8_t cflag2[8];
	unsigned int i,t;
	uint16_t orgticks;
	uint16_t pn;
	uint16_t ordn;
	uint8_t orders[128];
	struct gmdpattern *pp;
	struct LoadOKTResources r;

	r.temptrack = 0;
	r.buffer = 0;

	mpReset(m);

	if (file->read (file, hsig, 8) != 8)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #1\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing sig \"%c%c%c%c%c%c%c%c\" against \"OKTASONG\"\n", hsig[0], hsig[1], hsig[2], hsig[3], hsig[4], hsig[5], hsig[6], hsig[7]);
#endif
	if (memcmp(hsig, "OKTASONG", 8))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid signature\n");
		return errFormSig;
	}

	*m->name=0;
	m->message=0;

	m->options=MOD_TICK0;

	if (file->read (file, &chunk, sizeof(chunk)) != sizeof(chunk))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #3\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"CMOD\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
	if (memcmp(chunk.sig, "CMOD", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid \"SAMP\" signature\n");
		return errFormStruc;
	}
	chunk.blen=uint32_big(chunk.blen);
	if (chunk.blen!=8)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid blen %d, should had been 8\n", (int)chunk.blen);
		return errFormStruc;
	}
	if (file->read (file, cflags, 8) != 8)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #4\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] chan_flags[4]: 0x%04x 0x%04x 0x%04x 0x%04x\n", uint16_big(cflags[0]), uint16_big(cflags[1]), uint16_big(cflags[2]), uint16_big(cflags[3]));
#endif
	t=0;
	for (i=0; i<4; i++)
	{
		cflag2[t]=(uint16_big(cflags[i])&1)|((i+i+i)&2);
		if (cflag2[t++]&1)
			cflag2[t++]=(uint16_big(cflags[i])&1)|((i+i+i)&2);
	}
	m->channum=t;
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] channels: %d\n", (int)t);
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] %d %d %d %d %d %d %d %d\n", (int)cflag2[0], (int)cflag2[1], (int)cflag2[2], (int)cflag2[3], (int)cflag2[4], (int)cflag2[5], (int)cflag2[6], (int)cflag2[7]);
#endif

	if (file->read (file, &chunk, sizeof(chunk)) != sizeof (chunk))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #5\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"SAMP\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
	if (memcmp(chunk.sig, "SAMP", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid \"SAMP\" header\n");
		return errFormStruc;
	}
	chunk.blen=uint32_big(chunk.blen);
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] checking chunk length.. samples=%d module=%d (last should be zero)\n", (int)chunk.blen/32, (int)chunk.blen%32);
#endif
	if (chunk.blen&31)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid SAMP chunk length (modulo 32 test failed)\n");
		return errFormStruc;
	}
	m->modsampnum=m->sampnum=m->instnum=(chunk.blen)>>5;

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum))
		return errAllocMem;

	for (i=0; i<m->instnum; i++)
	{
		struct gmdinstrument *ip;
		struct gmdsample *sp;
		struct sampleinfo *sip;

		struct __attribute__((packed))
		{
			char name[20];
			uint32_t length;
			uint16_t repstart;
			uint16_t replen;
			uint8_t pad1;
			uint8_t vol;
			uint16_t pad2;
		} mi;
		if (file->read(file, &mi, sizeof (mi)) != sizeof (mi))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #7\n");
			return errFileRead;
		}
		mi.length=uint32_big(mi.length);
		mi.repstart=uint16_big(mi.repstart);
		mi.replen=uint16_big(mi.replen);
#ifdef OKT_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] sample[%d]\n", i);
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] length=%d\n", (int)mi.length);
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] repstart=%d\n", (int)mi.repstart);
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] replen=%d\n", (int)mi.replen);
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] pad1=%d\n", (int)mi.pad1);
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] vol=%d\n", (int)mi.vol);
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] pad2=%d\n", (int)mi.pad2);
#endif
		if (mi.length<4)
			mi.length=0;
		if (mi.replen<4)
			mi.replen=0;
		if (!mi.replen||(mi.repstart>=mi.length))
			mi.replen=0;
		else
			if ((mi.repstart+mi.replen)>mi.length)
				mi.replen=mi.length-mi.repstart;

		ip=&m->instruments[i];
		sp=&m->modsamples[i];
		sip=&m->samples[i];

		memcpy(ip->name, mi.name, 20);
		ip->name[20]=0;
		if (!mi.length)
			continue;

		for (t=0; t<128; t++)
			ip->samples[t]=i;

		*ip->name=0;
		sp->handle=i;
		sp->normnote=0;
		sp->stdvol=(mi.vol>0x3F)?0xFF:(mi.vol<<2);
		sp->stdpan=-1;
		sp->opt=0;

		sip->length=mi.length;
		sip->loopstart=mi.repstart;
		sip->loopend=mi.repstart+mi.replen;
		sip->samprate=8363;
		sip->type=mi.replen?mcpSampLoop:0;
	}

	if (file->read (file, &chunk, sizeof(chunk)) != sizeof(chunk))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #8\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"SPEE\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
	if (memcmp(chunk.sig, "SPEE", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] expected sub-header \"SPEE\" not found\n");
		return errFormStruc;
	}
	chunk.blen=uint32_big(chunk.blen);
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] chunk length should be 2, checking (%d)\n", (int)chunk.blen);
#endif
	if (chunk.blen!=2)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid length of sub chunk \"SPEE\": %d\n", (int)chunk.blen);
		return errFormStruc;
	}

	if (ocpfilehandle_read_uint16_be (file, &orgticks))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #10\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] orgticks: %d\n", (int)orgticks);
#endif

	if (file->read (file, &chunk, sizeof(chunk)) != sizeof (chunk))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #11\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"SLEN\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
	if (memcmp(chunk.sig, "SLEN", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] expected sub-header \"SLEN\" not found\n");
		return errFormStruc;
	}
	chunk.blen=uint32_big(chunk.blen);
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] chunk length should be 2, checking (%d)\n", (int)chunk.blen);
#endif
	if (chunk.blen!=2)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid length of sub chunk \"SPEE\": %d\n", (int)chunk.blen);
		return errFormStruc;
	}
	if (ocpfilehandle_read_uint16_be (file, &pn))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #13\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] song length (patterns): %d\n", (int)pn);
#endif

	if (file->read (file, &chunk, sizeof(chunk)) != sizeof (chunk))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #14\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"PLEN\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
	if (memcmp(chunk.sig, "PLEN", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] Expected sub-header \"PLEN\" not found\n");
		return errFormStruc;
	}
	chunk.blen = uint32_big(chunk.blen);
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] chunk length should be 2, checking (%d)\n", (int)chunk.blen);
#endif
	if (chunk.blen!=2)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid length of sub chunk \"PLEN\": %d\n", (int)chunk.blen);
		return errFormStruc;
	}
	if (ocpfilehandle_read_uint16_be (file, &ordn))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] warning, read failed #16\n");
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] orders: %d\n", (int)ordn);
#endif

	if (file->read (file, &chunk, sizeof(chunk)) != sizeof (chunk))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #17\n");
		return errFileRead;
	}
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"PATT\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
	if (memcmp(chunk.sig, "PATT", 4))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] Expected sub-header \"PATT\" not found\n");
		return errFormStruc;
	}
	chunk.blen=uint32_big(chunk.blen);
#ifdef OKT_LOAD_DEBUG
	cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] checking chunk length (should less than or equal to 128): %d\n", (int)chunk.blen);
#endif
	if (chunk.blen>128)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid length of sub chunk \"PATT\": %d\n", (int)chunk.blen);
		return errFormStruc;
	}

	if (file->read (file, orders, chunk.blen) != chunk.blen)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #19\n");
		return errFileRead;
	}
	if (chunk.blen<ordn)
		ordn=chunk.blen;
	m->loopord=0;

	m->patnum=ordn;
	m->ordnum=ordn;
	m->endord=m->patnum;
	m->tracknum=pn*(m->channum+1);

	if (!mpAllocPatterns(m, m->patnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocOrders(m, m->ordnum))
		return errAllocMem;

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=i;

	for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
	{
		for (i=0; i<m->channum; i++)
			pp->tracks[i]=orders[t]*(m->channum+1)+i;
		pp->gtrack=orders[t]*(m->channum+1)+m->channum;
	}

	r.temptrack=malloc(sizeof(uint8_t)*3000);
	r.buffer=malloc(sizeof(uint8_t)*(1024*m->channum));
	if (!r.buffer||!r.temptrack)
	{
		FreeResources (&r);
		return errAllocMem;
	}

	for (t=0; t<pn; t++)
	{
		uint16_t patlen;
		uint16_t q;
		uint8_t *tp;
		uint8_t *buf;
		uint8_t row;
		struct gmdtrack *trk;
		uint16_t len;

		if (file->read (file, &chunk, sizeof(chunk)) != sizeof (chunk))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #20\n");
			FreeResources (&r);
			return errFileRead;
		}
#ifdef OKT_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"PBOD\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
		if (memcmp(chunk.sig, "PBOD", 4))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] Expected sub-header \"PBOD\" not found\n");
			FreeResources (&r);
			return errFormStruc;
		}
		chunk.blen=uint32_big(chunk.blen);
		if (ocpfilehandle_read_uint16_be (file, &patlen))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] warning, read failed #22\n");
		}
#ifdef OKT_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] patlen red is %d. It should not be bigger than 256, and this should match:\n (blen!=(2+4*m->channum*patlen)\n %d!=2+4*%d*%d\n %d!=%d\n", (int)patlen, (int)chunk.blen, (int)m->channum, (int)patlen, (int)chunk.blen, (int)(2+4*m->channum*patlen));
#endif
		if ((chunk.blen!=(2+4*m->channum*patlen))||(patlen>256))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] invalid patlen: %d\n", (int)patlen);
			FreeResources (&r);
			return errFormStruc;
		}

		for (q=0; q<m->patnum; q++)
			if (t==orders[q])
				m->patterns[q].patlen=patlen;

		if (file->read (file, r.buffer, 4 * m->channum * patlen) != (4 * m->channum * patlen))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #23\n");
			FreeResources (&r);
			return errFileRead;
		}
		for (q=0; q<m->channum; q++)
		{
			uint8_t *tp=r.temptrack;
			uint8_t *buf=r.buffer+4*q;

			struct gmdtrack *trk;
			uint16_t len;

			uint8_t row;
			for (row=0; row<patlen; row++, buf+=m->channum*4)
			{
				uint8_t *cp=tp+2;

				uint8_t command=buf[2];
				uint8_t data=buf[3];
				int16_t nte=buf[0]?(buf[0]+60-17+4):-1;
				int16_t ins=buf[0]?buf[1]:-1;
				int16_t pan=-1;
				int16_t vol=-1;

				if (!row&&(t==orders[0]))
					pan=(cflag2[q]&2)?0xFF:0x00;

				if ((command==31)&&(data<=0x40))
				{
					vol=(data>0x3F)?0xFF:(data<<2);
					command=0;
				}
				if ((ins!=-1)||(nte!=-1)||(vol!=-1)||(pan!=-1))
				{
					unsigned char *act=cp;
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
				switch (command)
				{
					case 13: /* note down */
					case 17: /* note up */
					case 21: /* note - */
					case 30: /* note + */
						break;
					case 10: /* LNH */
					case 11: /* NHNL */
					case 12: /* HHNLL */
						break;
					case 31:
						if (data<=0x50)
							putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
						else
							if (data<=0x60)
								putcmd(&cp, cmdVolSlideUp, (data&0xF)<<2);
							else
								if (data<=0x70)
									putcmd(&cp, cmdRowVolSlideDown, (data&0xF)<<2);
								else
									if (data<=0x80)
										putcmd(&cp, cmdRowVolSlideUp, (data&0xF)<<2);
						break;
					case 27: /* release!!! */
						putcmd(&cp, cmdSetLoop, 0);
						break;
					case 0x1:
						putcmd(&cp, cmdPitchSlideDown, data);
						break;
					case 0x2:
						putcmd(&cp, cmdPitchSlideUp, data);
						break;
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
				{
					FreeResources (&r);
					return errAllocMem;
				}
				memcpy(trk->ptr, r.temptrack, len);
			}
		}

		tp=r.temptrack;
		buf=r.buffer;
		for (row=0; row<patlen; row++)
		{
			uint8_t *cp=tp+2;

			if (!row&&(t==orders[0]))
				putcmd(&cp, cmdTempo, orgticks);

			for (q=0; q<m->channum; q++, buf+=4)
			{
				uint8_t command=buf[2];
				uint8_t data=buf[3];

				switch (command)
				{
					case 25:
						putcmd(&cp, cmdGoto, data);
						break;
					case 28:
						if (data)
							putcmd(&cp, cmdTempo, data);
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

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];  NOT USED */
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];
		if (sp->handle==0xFFFF)
			continue;

		if (file->read (file, &chunk, sizeof(chunk)) != sizeof (chunk))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] read failed #24\n");
			return errFileRead;
		}
#ifdef OKT_LOAD_DEBUG
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] comparing header \"%c%c%c%c\" against \"SBOD\"\n", chunk.sig[0], chunk.sig[1], chunk.sig[2], chunk.sig[3]);
#endif
		if (memcmp(chunk.sig, "SBOD", 4))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] Expected sub-header \"SBOD\" not found\n");
			return errFormStruc;
		}
		chunk.blen=uint32_big(chunk.blen);

		sip->ptr=malloc(sizeof(char)*(chunk.blen+8));
		if (!sip->ptr)
			return errAllocMem;

		if (file->read (file, sip->ptr, chunk.blen) != chunk.blen)
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/OKT] warning, read failed #26\n");
		}
		if (sip->length>chunk.blen)
			sip->length=chunk.blen;
		if (sip->loopend>chunk.blen)
			sip->loopend=chunk.blen;
		if (sip->loopstart>=sip->loopend)
			sip->type&=~mcpSampLoop;
	}

	return errOk;
}
