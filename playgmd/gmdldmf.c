/* OpenCP Module Player
 * copyright (c) 1994-'21 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for X-Tracker modules
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
#include "filesel/filesystem.h"
#include "gmdplay.h"
#include "stuff/err.h"

static uint8_t *ibuf;
static uint8_t bitnum;
static uint32_t bitlen;

static inline uint16_t readbitsdmf(uint8_t n)
{
	uint16_t retval=0;
	int offset = 0;
	while (n)
	{
		int m=n;

		if (!bitlen)
		{
			fprintf(stderr, "readbitsdmf: ran out of buffer\n");
			return 0;
		}

		if (m>bitnum)
			m=bitnum;
		retval|=(*ibuf&((1L<<m)-1))<<offset;
		*ibuf>>=m;
		n-=m;
		offset+=m;
		if ( ! ( bitnum-=m ) )
		{
			bitlen--;
			ibuf++;
			bitnum=8;
		}
	}
	return retval;
}

static uint16_t nodenum, lastnode;
static int16_t huff[255][3];

static void readtree(void)
{
	int16_t (*node)[3];
	uint8_t left;
	uint8_t right;

	huff[nodenum][2]=readbitsdmf(7);
	node=&huff[lastnode];
	left=readbitsdmf(1);
	right=readbitsdmf(1);

	lastnode=++nodenum;
	if (left)
	{
		(*node)[0]=lastnode;
		readtree();
	} else
		(*node)[0]=-1;
	lastnode=nodenum;
	if (right)
	{
		(*node)[1]=lastnode;
		readtree();
	} else
		(*node)[1]=-1;
}

static void unpack0(uint8_t *ob, uint8_t *ib, uint32_t len)
{
	uint32_t i;

	ibuf=ib;
	bitnum=8;
	bitlen = len;

	nodenum=lastnode=0;
	readtree();

	for (i=0; i<len; i++)
	{
		uint8_t sign=readbitsdmf(1)?0xFF:0;
		uint16_t pos=0;
		while ((huff[pos][0]!=-1)&&(huff[pos][1]!=-1))
			pos=huff[pos][readbitsdmf(1)];
		*ob++=huff[pos][2]^sign;
	}
}

static inline void putcmd(uint8_t **p, uint8_t c, uint8_t d)
{
	*(*p)++=c;
	*(*p)++=d;
}

static void calctempo(uint16_t rpm, uint8_t *tempo, uint8_t *bpm)
{
	for ((*tempo)=30; (*tempo)>1; (*tempo)--)
		if ((rpm*(*tempo)/24)<256)
			break;
	(*bpm)=rpm*(*tempo)/24;
}

static int _mpLoadDMF (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	struct __attribute__((packed)) {
		uint8_t sig[4];
		uint8_t ver;
		char tracker[8];
		char name[30];
		char composer[20];
		char date[3];
	} hdr;

	uint8_t sig[4];
	uint32_t next;

	uint16_t ordloop,ordnum;
	uint16_t *orders = 0;

	uint16_t patnum;
	uint8_t chnnum;

	uint8_t *patbuf = 0;
	uint8_t **patadr = 0;
	uint8_t (*temptrack)[3000] = 0;

	uint8_t *curadr;
	uint_fast32_t i;

	uint16_t nordnum;

	uint16_t curord;

	uint8_t speed;
	uint8_t ttype;
	uint8_t pbeat;
	uint8_t *pp=0;
	uint8_t voc=0; /* suppres warning for now ...*/
	uint16_t len=0; /* suppres warning for now ...*/
	uint8_t nextinfobyte[33];

	uint8_t smppack[256];

	int retval = 0;

	mpReset(m);

	if (file->read (file, &hdr, sizeof (hdr)) != sizeof (hdr))
	{
		fprintf(stderr, __FILE__ ": warning, read failed #1\n");
	}
	if (memcmp(hdr.sig, "DDMF", 4))
		return errFormSig;

	if (hdr.ver<5)
		return errFormOldVer;

	m->options=MOD_TICK0|MOD_EXPOFREQ;

	memcpy(m->name, hdr.name, 30);
	m->name[30]=0;

	memcpy(m->composer, hdr.composer, 20);
	m->composer[20]=0;

	if (file->read (file, sig, sizeof (sig)) != sizeof (sig))
	{
		fprintf(stderr, __FILE__ ": warning, read failed #2\n");
	}
	if (ocpfilehandle_read_uint32_le (file, &next))
	{
		next = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #3\n");
	}

	if (!memcmp(sig, "INFO", 4))
	{
		file->seek_cur (file, next);
		if (file->read (file, sig, sizeof (sig)) != sizeof (sig))
		{
			fprintf(stderr, __FILE__ ": warning, read failed #4\n");
		}
		if (ocpfilehandle_read_uint32_le (file, &next))
		{
			next = 0;
			fprintf(stderr, __FILE__ ": warning, read failed #5\n");
		}
	}

	if (!memcmp(sig, "CMSG", 4))
	{
		uint8_t waste;
		uint16_t msglen;
		int16_t t;
		int16_t i;
		if (ocpfilehandle_read_uint8 (file, &waste))
		{
			fprintf(stderr, __FILE__ ": warning, read failed #6\n");
		}
		msglen=(next-1)/40;

		if (msglen)
		{
			m->message=malloc(sizeof(char *)*(msglen+1));
			if (!m->message)
				return errAllocMem;
			*m->message=malloc(sizeof(char)*(msglen*41));
			if (!*m->message)
				return errAllocMem;

			for (t=0; t<msglen; t++)
			{
				m->message[t]=*m->message+t*41;
				if (file->read (file, m->message[t], 40) != 40)
				{
					fprintf(stderr, __FILE__ ": warning, read failed #7\n");
				}

				for (i=0; i<40; i++)
					if (!m->message[t][i])
						m->message[t][i]=' ';
				m->message[t][40]=0;
			}
			m->message[msglen]=0;
		}

		if (file->read (file, sig, sizeof (sig)) != sizeof (sig))
		{
			fprintf(stderr, __FILE__ ": warning, read failed #8\n");
		}
		if (ocpfilehandle_read_uint32_le (file, &next))
		{
			next = 0;
			fprintf(stderr, __FILE__ ": warning, read failed #9\n");
		}
	}

	if ((memcmp(sig, "SEQU", 4))||(next&1))
		return errFormStruc;

	orders=malloc(next-4);
	if (!orders)
		return errAllocMem;
	if (ocpfilehandle_read_uint16_le (file, &ordloop))
	{
		ordloop = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #10\n");
	}
	if (ocpfilehandle_read_uint16_le (file, &ordnum))
	{
		ordnum = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #11\n");
	}
	if (file->read (file, orders, next-4) != (next - 4))
	{
		fprintf(stderr, __FILE__ ": warning, read failed #12\n");
	}

	{
		unsigned int i;
		for (i=0;i<((next-4)>>1);i++)
			orders[i] = uint16_little (orders[i]);
	}
	ordnum++;

	if ((unsigned)2*ordnum>(next-4))
		ordnum=(next-4)/2; /* 2 = sizeof(uint16_t) */
	if (ordloop>=ordnum)ordloop=0;

	if (file->read (file, sig, sizeof (sig)) != sizeof (sig))
	{
		fprintf(stderr, __FILE__ ": warning, read failed #13\n");
	}
	if (ocpfilehandle_read_uint32_le (file, &next))
	{
		next = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #14\n");
	}

	if (memcmp(sig, "PATT", 4))
	{
		retval = errFormStruc;
		goto safeout;
	}

	if (ocpfilehandle_read_uint16_le (file, &patnum))
	{
		patnum = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #15\n");
	}
	if (ocpfilehandle_read_uint8 (file, &chnnum))
	{
		chnnum = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #16\n");
	}
	m->channum=chnnum;

	patbuf=malloc(sizeof(uint8_t)*(next-3));
	patadr=malloc(sizeof(uint8_t *)*(patnum));
	temptrack=malloc(sizeof(uint8_t)*(m->channum+1)*3000);
	if (!patbuf||!patadr||!temptrack)
	{
		retval = errAllocMem;
		goto safeout;
	}
	if (file->read (file, patbuf, next-3) != (next - 3))
	{
		fprintf(stderr, __FILE__ ": warning, read failed #17\n");
	}

/* get the pattern start adresses */
	curadr=patbuf; /* this part can easy crash if values are fucked in buffer we just red, TODO */
	for (i=0; i<patnum; i++)
	{
		patadr[i]=curadr;
		curadr += 8 + uint32_little (*(uint32_t *)(curadr+4));
	}

/* get the new order number */
	nordnum=0;
	for (i=0; i<ordnum; i++)
		nordnum+=(uint16_little(*(uint16_t *)((patadr[orders[i]])+2))>256)?2:1;

/* relocate orders */
	curord=nordnum;
	for (i=ordnum-1; (signed)i>=0; i--)
	{
		if (uint16_little(*(uint16_t *)(patadr[orders[i]]+2))>256)
		{
			curord-=2;
			orders[curord]=orders[i];
			orders[curord+1]=orders[i]|0x8000;
		} else
			orders[--curord]=orders[i];
		if (i==ordloop)
			ordloop=curord;
	}
	ordnum=nordnum;

	m->patnum=ordnum;
	m->ordnum=ordnum;
	m->endord=m->patnum;
	m->loopord=ordloop;
	m->tracknum=ordnum*(m->channum+1);

	if (!mpAllocTracks(m, m->tracknum)||!mpAllocPatterns(m, m->patnum)||!mpAllocOrders(m, m->ordnum))
	{
		retval = errAllocMem;
		goto safeout;
	}

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=i;

	speed=125;
	ttype=1;
	pbeat=8;

/* convert patterns */
	for (i=0; i<ordnum; i++)
	{
/*    if (orders[i]&0x8000)
        return errFormStruc; */
		uint_fast32_t j;
		int16_t row;
		int16_t rownum;

		uint8_t *(tp[33]);

		struct gmdtrack *trk;
		uint16_t tlen;

		if (!(orders[i]&0x8000))
		{
			pp=patadr[orders[i]&~0x8000];
			voc=*pp++;
			pbeat=(*pp++)>>4;
			if (!pbeat)
				pbeat=8;
			len=uint16_little(*(uint16_t *)pp);
			pp+=6;
			memset(nextinfobyte, 0, 33);
			if (len>256)
				rownum=256;
			else
				rownum=len;
		} else
			rownum=len-256;

		m->patterns[i].patlen=rownum;

		for (j=0; j<=m->channum; j++)
			tp[j]=temptrack[j];

		for (j=voc; j<m->channum; j++)
		{
			*tp[j]++=0;
			*tp[j]++=2;
			*tp[j]++=cmdKeyOff;
			*tp[j]++=0;
		}

		for (row=0; row<rownum; row++)
		{
			if (!nextinfobyte[m->channum])
			{
				uint8_t info;
				uint8_t data=data; /* supress warning.. it depends on info, and that is safe */
				uint8_t *cp;
				uint8_t tempochange;

				if (!pp)
				{
					fprintf(stderr, "playgmd: gmdldmf.c: pp not set\n");
					retval = errFormStruc;
					goto safeout;
				}
				info=*pp++;

				if (info&0x80)
					nextinfobyte[m->channum]=*pp++;
				info&=~0x80;
				if (info)
					data=*pp++;

				cp=tp[m->channum]+2;
				tempochange=!row&&ttype&&!(orders[i]&0x8000);

				switch (info)
				{
					case 1:
						ttype=0;
						speed=data;
						tempochange=1;
						break;
					case 2:
						ttype=1;
						speed=data;
						tempochange=1;
						break;
					case 3:
						pbeat=data>>4;
						tempochange=ttype;
						break;
				}

				if (tempochange)
				{
					uint8_t tempo;
					uint8_t bpm;
					if (ttype&&pbeat)
						calctempo(speed*pbeat, &tempo, &bpm);
					else
						calctempo((speed+1)*15, &tempo, &bpm);
					putcmd(&cp, cmdTempo, tempo);
					putcmd(&cp, cmdSpeed, bpm);
				}

				if (cp!=(tp[m->channum]+2))
				{
					tp[m->channum][0]=row;
					tp[m->channum][1]=cp-tp[m->channum]-2;
					tp[m->channum]=cp;
				}
			} else
				nextinfobyte[m->channum]--;

			for (j=0; j<voc; j++)
				if (!nextinfobyte[j])
				{
					uint8_t cmds[9];
					uint8_t info=*pp++;
					uint8_t *cp;

					if (info&0x80)
						nextinfobyte[j]=*pp++;
					if (info&0x40)
						cmds[0]=*pp++;
					else
						cmds[0]=0;
					if (info&0x20)
						cmds[1]=*pp++;
					else
						cmds[1]=0;
					if (info&0x10)
						cmds[2]=*pp++;
					else
						cmds[2]=0;
					if (info&0x08)
					{
						cmds[3]=*pp++;
						cmds[4]=*pp++;
					} else
						cmds[3]=cmds[4]=0;
					if (info&0x04)
					{
						cmds[5]=*pp++;
						cmds[6]=*pp++;
					} else
						cmds[5]=cmds[6]=0;
					if (info&0x02)
					{
						cmds[7]=*pp++;
						cmds[8]=*pp++;
					} else
						cmds[7]=cmds[8]=0;

					cp=tp[j]+2;

					if (cmds[0]||(cmds[1]&&(cmds[1]!=255))||cmds[2]||(cmds[7]==7))
					{
						unsigned char *act=cp;
						*cp++=cmdPlayNote;
						if (cmds[0])
						{
							*act|=cmdPlayIns;
							*cp++=cmds[0]-1;
						}
						if (cmds[1]&&(cmds[1]!=255))
						{
							*act|=cmdPlayNte;
							*cp++=cmds[1]+23;
						}
						if (cmds[2])
						{
							*act|=cmdPlayVol;
							*cp++=cmds[2]-1;
						}
						if (cmds[7]==7)
						{
							*act|=cmdPlayPan;
							*cp++=cmds[8];
						}
					}
					if (cmds[1]==255)
						putcmd(&cp, cmdKeyOff, 0);

					switch (cmds[3])
					{
						case 1:
							putcmd(&cp, cmdKeyOff, 0); /* falsch! */
							break;
						case 2:
							putcmd(&cp, cmdSetLoop, 0);
							break;
						case 6:
							putcmd(&cp, cmdOffset, cmds[4]);
							break;
					}

					switch (cmds[5])
					{
						case 1:
							putcmd(&cp, cmdRowPitchSlideDMF, cmds[6]);
							break;
						case 3:
							putcmd(&cp, cmdArpeggio, cmds[6]);
							break;
						case 4:
							putcmd(&cp, cmdPitchSlideUDMF, cmds[6]);
							break;
						case 5:
							putcmd(&cp, cmdPitchSlideDDMF, cmds[6]);
							break;
						case 6:
							putcmd(&cp, cmdPitchSlideNDMF, cmds[6]);
							break;
						case 8:
							putcmd(&cp, cmdPitchVibratoSinDMF, cmds[6]);
							break;
						case 9:
							putcmd(&cp, cmdPitchVibratoTrgDMF, cmds[6]);
							break;
						case 10:
							putcmd(&cp, cmdPitchVibratoRecDMF, cmds[6]);
							break;
						case 12:
							putcmd(&cp, cmdKeyOff, 0); /* falsch! */
							break;
					}

					switch (cmds[7])
					{
						case 1:
							putcmd(&cp, cmdVolSlideUDMF, cmds[8]);
							break;
						case 2:
							putcmd(&cp, cmdVolSlideDDMF, cmds[8]);
							break;
						case 4:
							putcmd(&cp, cmdVolVibratoSinDMF, cmds[8]);
							break;
						case 5:
							putcmd(&cp, cmdVolVibratoTrgDMF, cmds[8]);
							break;
						case 6:
							putcmd(&cp, cmdVolVibratoRecDMF, cmds[8]);
							break;
						case 8:
							putcmd(&cp, cmdPanSlideLDMF, cmds[8]);
							break;
						case 9:
							putcmd(&cp, cmdPanSlideRDMF, cmds[8]);
							break;
						case 10:
							putcmd(&cp, cmdPanVibratoSinDMF, cmds[8]);
							break;
					}

					if (cp!=(tp[j]+2))
					{
						tp[j][0]=row;
						tp[j][1]=cp-tp[j]-2;
						tp[j]=cp;
					}
				} else
					nextinfobyte[j]--;
		}

		for (j=0; j<m->channum; j++)
		{
			struct gmdtrack *trk;
			uint16_t tlen;

			m->patterns[i].tracks[j]=i*(m->channum+1)+j;

			trk=&m->tracks[i*(m->channum+1)+j];
			tlen=tp[j]-temptrack[j];

			if (!tlen)
				trk->ptr=trk->end=0;
			else {
				trk->ptr=malloc(sizeof(uint8_t)*tlen);
				trk->end=trk->ptr+tlen;
				if (!trk->ptr)
				{
					retval = errAllocMem;
					goto safeout;
				}
				memcpy(trk->ptr, temptrack[j], tlen);
			}
		}

		m->patterns[i].gtrack=i*(m->channum+1)+m->channum;

		trk=&m->tracks[i*(m->channum+1)+m->channum];
		tlen=tp[m->channum]-temptrack[m->channum];

		if (!tlen)
			trk->ptr=trk->end=0;
		else {
			trk->ptr=malloc(sizeof(uint8_t)*tlen);
			trk->end=trk->ptr+tlen;
			if (!trk->ptr)
			{
				retval = errAllocMem;
				goto safeout;
			}
			memcpy(trk->ptr, temptrack[m->channum], tlen);
		}
	}

	free(temptrack);
	free(patbuf);
	free(patadr);
	free(orders);
	temptrack=0;
	patbuf=0;
	patadr=0;
	orders=0;

	if (file->read (file, sig, sizeof (sig)) != sizeof (sig))
	{
		fprintf(stderr, __FILE__ ": warning, read failed #17\n");
	}
	if (ocpfilehandle_read_uint32_le (file, &next))
	{
		next = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #18\n");
	}
/* inst!! */

	if (memcmp(sig, "SMPI", 4))
		return errFormStruc;

	{
		uint8_t instnum;

		if (ocpfilehandle_read_uint8 (file, &instnum))
		{
			instnum = 0;
			fprintf(stderr, __FILE__ ": warning, read failed #19\n");
		}
		m->instnum = instnum;
		m->modsampnum=m->sampnum=m->instnum = instnum;
	}

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum))
		return errAllocMem;

	for (i=0; i<m->instnum; i++)
	{
		struct gmdinstrument *ip=&m->instruments[i];
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];

		uint8_t namelen;

		struct __attribute__((packed)) {
			uint32_t length;
			uint32_t loopstart;
			uint32_t loopend;
			uint16_t freq;
			uint8_t vol;
			uint8_t type;
			uint8_t filler[10];
			uint32_t crc32;
		} smp;
		uint8_t bit16;

		int j;

		if (ocpfilehandle_read_uint8 (file, &namelen))
		{
			namelen = 0;
			fprintf(stderr, __FILE__ ": warning, read failed #20\n");
		}
		if (namelen>31)
		{
			if (file->read (file, ip->name, 31) != 31)
			{
				fprintf(stderr, __FILE__ ": warning, read failed #21\n");
				file->seek_cur (file, namelen - 31);
				namelen=31;
			}
		} else {
			if (file->read (file, ip->name, namelen) != namelen)
			{
				fprintf(stderr, __FILE__ ": warning, read failed #22\n");
			}
		}
		ip->name[namelen]=0;

		if (file->read (file, &smp, sizeof (smp)) != sizeof (smp))
		{
			fprintf(stderr, __FILE__ ": warning, read failed #23\n");
		}
		smp.length = uint32_little (smp.length);
		smp.loopstart = uint32_little (smp.loopstart);
		smp.loopend = uint32_little (smp.loopend);
		smp.freq = uint16_little (smp.freq);
		smp.crc32 = uint32_little (smp.crc32);

		smppack[i]=!!(smp.type&0x04);
		bit16=!!(smp.type&0x02);
		if (smp.type&0x88)
			return errFormSupp; /* can't do this */
		if (bit16&&smppack[i])
			return errFormSupp; /* don't want 16 bit packed samples.. */
		sip->length=smp.length>>bit16;
		sip->loopstart=smp.loopstart>>bit16;
		sip->loopend=smp.loopend>>bit16;
		sip->samprate=smp.freq;
		sip->type=((smp.type&4)?mcpSampDelta:0)|(bit16?mcpSamp16Bit:0)|((smp.type&1)?mcpSampLoop:0);

		if (!smp.length)
			continue;

		for (j=0; j<128; j++)
			ip->samples[j]=i;
		*sp->name=0;
		sp->handle=i;
		sp->normnote=0;
		sp->stdvol=smp.vol?smp.vol:-1;
		sp->stdpan=-1;
		sp->opt=bit16?MP_OFFSETDIV2:0;
	}

	if (file->read (file, sig, sizeof (sig)) != sizeof (sig))
	{
		fprintf(stderr, __FILE__ ": warning, read failed #24\n");
	}
	if (ocpfilehandle_read_uint32_le (file, &next))
	{
		next = 0;
		fprintf(stderr, __FILE__ ": warning, read failed #25\n");
	}

	if (memcmp(sig, "SMPD", 4))
		return errFormStruc;

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];     NOT USED */
		struct gmdsample *sp=&m->modsamples[i];
		struct sampleinfo *sip=&m->samples[i];

		uint32_t len;
		uint8_t *smpp;

		if (ocpfilehandle_read_uint32_le (file, &len))
		{
			len = 0;
			fprintf(stderr, __FILE__ ": warning, read failed #26\n");
		}

		if (sp->handle==0xFFFF)
		{
			file->seek_cur (file, len);
			continue;
		}

		smpp=malloc(sizeof(unsigned char)*len+4/* worst case padding */);
		if (!smpp)
			return errAllocMem;
		if (file->read (file, smpp, len) != len)
		{
			fprintf(stderr, __FILE__ ": warning, read failed #27\n");
		}
		if (smppack[i])
		{
			uint8_t *dbuf=malloc(sizeof(unsigned char)*(sip->length+16));
			if (!dbuf)
			{
				free(smpp);
				return errAllocMem;
			}
			unpack0(dbuf, smpp, sip->length);
			free(smpp);
			smpp=dbuf;
		}

		sip->ptr=smpp;
	}

	return errOk;

safeout:
	if (orders)
		free(orders);
	if (patbuf)
		free(patbuf);
	if (patadr)
		free(patadr);
	if (temptrack)
		free(temptrack);
	return retval;
}

struct gmdloadstruct mpLoadDMF = { _mpLoadDMF };

struct linkinfostruct dllextinfo = {.name = "gmdldmf", .desc = "OpenCP Module Loader: *.DMF (c) 1994-'22 Niklas Beisert, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
