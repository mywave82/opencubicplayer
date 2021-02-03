/* OpenCP Module Player
 * copyright (c) '94-'21 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
 * GMDPlay loader for DigiTrakker modules
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

static inline uint16_t readbits(uint8_t n)
{
	uint16_t retval=0;
	int offset = 0;
	while (n)
	{
		int m=n;

		if (!bitlen)
		{
			fprintf(stderr, "readbits: ran out of buffer\n");
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




/*
static short vibsintab[256]=
 {    0,    50,   100,   151,   201,   251,   301,   350,
    400,   449,   498,   546,   595,   642,   690,   737,
    784,   830,   876,   921,   965,  1009,  1053,  1096,
   1138,  1179,  1220,  1260,  1299,  1338,  1375,  1412,
   1448,  1483,  1517,  1551,  1583,  1615,  1645,  1674,
   1703,  1730,  1757,  1782,  1806,  1829,  1851,  1872,
   1892,  1911,  1928,  1945,  1960,  1974,  1987,  1998,
   2009,  2018,  2026,  2033,  2038,  2042,  2046,  2047,
   2048,  2047,  2046,  2042,  2038,  2033,  2026,  2018,
   2009,  1998,  1987,  1974,  1960,  1945,  1928,  1911,
   1892,  1872,  1851,  1829,  1806,  1782,  1757,  1730,
   1703,  1674,  1645,  1615,  1583,  1551,  1517,  1483,
   1448,  1412,  1375,  1338,  1299,  1260,  1220,  1179,
   1138,  1096,  1053,  1009,   965,   921,   876,   830,
    784,   737,   690,   642,   595,   546,   498,   449,
    400,   350,   301,   251,   201,   151,   100,    50,
      0,   -50,  -100,  -151,  -201,  -251,  -301,  -350,
   -400,  -449,  -498,  -546,  -595,  -642,  -690,  -737,
   -784,  -830,  -876,  -921,  -965, -1009, -1053, -1096,
  -1138, -1179, -1220, -1260, -1299, -1338, -1375, -1412,
  -1448, -1483, -1517, -1551, -1583, -1615, -1645, -1674,
  -1703, -1730, -1757, -1782, -1806, -1829, -1851, -1872,
  -1892, -1911, -1928, -1945, -1960, -1974, -1987, -1998,
  -2009, -2018, -2026, -2033, -2038, -2042, -2046, -2047,
  -2048, -2047, -2046, -2042, -2038, -2033, -2026, -2018,
  -2009, -1998, -1987, -1974, -1960, -1945, -1928, -1911,
  -1892, -1872, -1851, -1829, -1806, -1782, -1757, -1730,
  -1703, -1674, -1645, -1615, -1583, -1551, -1517, -1483,
  -1448, -1412, -1375, -1338, -1299, -1260, -1220, -1179,
  -1138, -1096, -1053, -1009,  -965,  -921,  -876,  -830,
   -784,  -737,  -690,  -642,  -595,  -546,  -498,  -449,
   -400,  -350,  -301,  -251,  -201,  -151,  -100,   -50};
*/

static inline void putcmd(unsigned char **p, unsigned char c, unsigned char d)
{
  *(*p)++=c;
  *(*p)++=d;
}

struct LoadMDLResources
{
	struct gmdsample **msmps;
	unsigned int *inssampnum;
};

static void FreeResources (struct LoadMDLResources *r)
{
	int j;
	if (r->msmps)
	{
		for (j=0; j<255; j++)
		{
			if (r->msmps[j])
				free(r->msmps[j]);
		}
		free(r->msmps);
		r->msmps=0;
	}
	if (r->inssampnum)
	{
		free(r->inssampnum);
		r->inssampnum=0;
	}
}

static int _mpLoadMDL(struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	uint32_t waste32;
	uint16_t waste16;
	uint8_t waste8;

	uint32_t blklen;
	uint16_t blktype;

	struct __attribute__((packed))
	{
		char name[32];
		char composer[20];
		uint16_t ordnum;
		uint16_t repstart;
		uint8_t mainvol;
		uint8_t speed;
		uint8_t bpm;
		uint8_t pan[32];
	} mdlhead;

	unsigned int i,j,k;

	uint8_t ordtab[256];
	uint8_t patnum;

	uint16_t ntracks;

	uint8_t **trackends;
	uint8_t **trackptrs;
	uint8_t *trackbuf;
	uint8_t (*patdata)[256][6];
	uint8_t *temptrack;
	int tpos;
	uint8_t smpsav;
	uint8_t packtype[255];

	uint8_t inssav;
	unsigned int smpnum;

	struct LoadMDLResources r;
	r.msmps = 0;
	r.inssampnum = 0;

	mpReset(m);

	if (ocpfilehandle_read_uint32_le (file, &waste32))
	{
		fprintf(stderr, __FILE__ ": fread() failed #1\n");
		return errFormStruc;
	}
	if (waste32!=0x4C444D44)
		return errFormSig;

	if (ocpfilehandle_read_uint8 (file, &waste8))
	{
		fprintf(stderr, __FILE__ ": fread() failed #2\n");
		return errFormStruc;
	}
	if ((waste8&0x10)!=0x10)
	{
		fprintf(stderr, "Sorry, the file version is too old (load and resave it in DigiTrakker please)\n");
		return errFormSig;
	}

	if (ocpfilehandle_read_uint16_le (file, &waste16))
	{
		fprintf(stderr, __FILE__ ": fread() failed #3\n");
		return errFormStruc;
	}
	if (waste16!=0x4E49)
		return errFormStruc;

	if (ocpfilehandle_read_uint32_le (file, &blklen))
	{
		fprintf(stderr, __FILE__ ": fread() failed #4\n");
		return errFormStruc;
	}

	if (file->read (file, &mdlhead, sizeof(mdlhead)) != sizeof(mdlhead))
	{
		fprintf(stderr, __FILE__ ": fread() failed #5\n");
		return errFormStruc;
	}
	mdlhead.ordnum = uint16_little (mdlhead.ordnum);
	mdlhead.repstart = uint16_little (mdlhead.repstart);

	for (i=0; i<32; i++)
		if (mdlhead.pan[i]&0x80)
			break;
	m->channum=i;
	memcpy(m->name, mdlhead.name, 31);
	m->name[31]=0;
	memcpy(m->composer, mdlhead.composer, 20);
	m->composer[20]=0;
	m->ordnum=mdlhead.ordnum;
	m->endord=m->ordnum;
	m->loopord=mdlhead.repstart;
	m->options=MOD_EXPOFREQ|MP_OFFSETDIV2;

	if (mdlhead.ordnum>256)
		return errFormSupp;

	if (file->read (file, ordtab, mdlhead.ordnum) != mdlhead.ordnum)
	{
		fprintf(stderr, __FILE__ ": fread() failed #6\n");
		return errFormStruc;
	}
	file->seek_cur (file, 8*m->channum);
	file->seek_cur (file, blklen - 8 * m->channum - 91 - mdlhead.ordnum);

	if (ocpfilehandle_read_uint16_le (file, &blktype))
	{
		fprintf(stderr, __FILE__ ": fread() failed #7\n");
		return errFormStruc;
	}
	if (ocpfilehandle_read_uint32_le (file, &blklen))
	{
		fprintf(stderr, __FILE__ ": fread() failed #8\n");
		return errFormStruc;
	}
	blklen = uint32_little (blklen);

	if (blktype==0x454D)
	{
		file->seek_cur (file, blklen);
		if (ocpfilehandle_read_uint16_le (file, &blktype))
		{
			fprintf(stderr, __FILE__ ": fread() failed #9\n");
			return errFormStruc;
		}
		if (ocpfilehandle_read_uint32_le (file, &blklen))
		{
			fprintf(stderr, __FILE__ ": fread() failed #10\n");
			return errFormStruc;
		}
/* songmessage; every line is closed with the CR-char (13). A
 * 0-byte stands at the end of the whole text.
 */
	}

	if (blktype!=0x4150)
		return errFormStruc;
	if (ocpfilehandle_read_uint8 (file, &patnum))
	{
		fprintf(stderr, __FILE__ ": fread() failed #11\n");
		return errFormStruc;
	}
	m->patnum=patnum+1;
	m->tracknum=patnum*(m->channum+1)+1;

	if (!mpAllocPatterns(m, m->patnum)||!mpAllocOrders(m, m->ordnum)||!mpAllocTracks(m, m->tracknum))
		return errAllocMem;

	for (j=0; j<patnum; j++)
	{
		uint8_t chnn;
		int i;
		if (ocpfilehandle_read_uint8 (file, &chnn))
		{
			fprintf(stderr, __FILE__ ": fread() failed #12\n");
			return errFormStruc;
		}

		if (ocpfilehandle_read_uint8 (file, &waste8))
		{
			fprintf(stderr, __FILE__ ": fread() failed #13\n");
			return errFormStruc;
		}
		m->patterns[j].patlen = waste8 + 1;

		if (file->read (file, m->patterns[j].name, 16) != 16)
		{
			fprintf(stderr, __FILE__ ": fread() failed #14\n");
			return errFormStruc;
		}
		m->patterns[j].name[16]=0;
		memset(m->patterns[j].tracks, 0, 32*2);
		if (file->read (file, m->patterns[j].tracks, 2 * chnn) != (2 * chnn))
		{
			fprintf(stderr, __FILE__ ": fread() failed #15\n");
			return errFormStruc;
		}
		for(i=0;i<chnn;i++)
			m->patterns[j].tracks[i] = uint16_little (m->patterns[j].tracks[i]);
	}

	if (ocpfilehandle_read_uint16_le (file, &waste16))
	{
		fprintf(stderr, __FILE__ ": fread() failed #16\n");
		return errFormStruc;
	}
	if (waste16!=0x5254)
		return errFormStruc;
	if (ocpfilehandle_read_uint32_le (file, &blklen))
	{
		fprintf(stderr, __FILE__ ": fread() failed #17\n");
		return errFormStruc;
	}

	if (ocpfilehandle_read_uint16_le (file, &ntracks))
	{
		fprintf(stderr, __FILE__ ": fread() failed #18\n");
		return errFormStruc;
	}

	trackends=malloc(sizeof(uint8_t *)*ntracks+1);
	trackptrs=malloc(sizeof(uint8_t *)*ntracks+1);
	trackbuf=malloc(sizeof(uint8_t)*(blklen-2-2*ntracks));

	patdata=malloc(sizeof(uint8_t)*m->channum*256*6);
	temptrack=malloc(sizeof(uint8_t)*3000);

	if (!trackends||!trackptrs||!trackbuf||!patdata||!temptrack)
		return errAllocMem;

	trackptrs[0]=trackbuf;
	trackends[0]=trackbuf;
	tpos=0;
	for (i=0; i<ntracks; i++)
	{
		uint16_t l;
		if (ocpfilehandle_read_uint16_le (file, &l))
		{
			fprintf(stderr, __FILE__ ": fread() failed #19\n");
			return errFormStruc;
		}
		trackptrs[1+i]=trackbuf+tpos;
		if (file->read (file, trackbuf+tpos, l) != l)
		{
			fprintf(stderr, __FILE__ ": fread() failed #20\n");
			return errFormStruc;
		}
		tpos+=l;
		trackends[1+i]=trackbuf+tpos;
	}

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=(ordtab[i]<patnum)?ordtab[i]:patnum;

	for (i=0; i<32; i++)
		m->patterns[patnum].tracks[i]=m->tracknum-1;
	m->patterns[patnum].gtrack=m->tracknum-1;
	m->patterns[patnum].patlen=64;

	for (j=0; j<patnum; j++)
	{
		uint8_t *tp;
		uint8_t *buf;
		int row;
		struct gmdtrack *trk;
		uint16_t len;

		memset(patdata, 0, m->channum*256*6);
		for (i=0; i<m->channum; i++)
		{
			uint8_t *trkptr=trackptrs[m->patterns[j].tracks[i]];
			uint8_t *endptr=trackends[m->patterns[j].tracks[i]];
			int row=0;

			while (trkptr<endptr)
			{
				uint8_t p=*trkptr++;
				switch (p&3)
				{
					case 0:
						row+=p>>2;
						break;
					case 1:
						for (k=0; k<((p>>2)+(unsigned)1); k++)
							memcpy(patdata[i][row+k], patdata[i][row-1], 6);
						row+=p>>2;
						break;
					case 2:
						memcpy(patdata[i][row], patdata[i][p>>2], 6);
						break;
					case 3:
						for (k=0; k<6; k++)
							if (p&(4<<k))
								patdata[i][row][k]=*trkptr++;
						break;
				}
				row++;
			}
		}

		for (i=0; i<m->channum; i++)
			m->patterns[j].tracks[i]=j*(m->channum+1)+i;
		m->patterns[j].gtrack=j*(m->channum+1)+m->channum;

		for (i=0; i<m->channum; i++)
		{
			uint8_t *tp=temptrack;
			uint8_t *buf=patdata[i][0];

			struct gmdtrack *trk;
			uint16_t len;

			int row;
			for (row=0; row<m->patterns[j].patlen; row++, buf+=6)
			{
				uint8_t *cp=tp+2;

				uint8_t command1=buf[3]&0xF;
				uint8_t command2=buf[3]>>4;
				uint8_t data1=buf[4];
				uint8_t data2=buf[5];
				int16_t ins=buf[1]-1;
				int16_t nte=buf[0];
				int16_t pan=-1;
				int16_t vol=buf[2];
				uint16_t ofs=0;
				if (!vol)
					vol=-1;

				if (command1==0xE)
				{
					command1=(data1&0xF0)|0xE;
					data1&=0xF;
					if (command1==0xFE)
						ofs=(data1<<8)|data2;
				}
				if (command2==0xE)
				{
					command2=(data2&0xF0)|0xE;
					data2&=0xF;
				}

				if (!row&&(j==ordtab[0]))
					putcmd(&cp, cmdPlayNote|cmdPlayPan, mdlhead.pan[i]*2);

				if (command1==0x8)
					pan=data1*2;

				if (command2==0x8)
					pan=data2*2;

				if ((command1==0x3)&&nte&&(nte!=255))
					nte|=128;

				if ((ins!=-1)||nte||(vol!=-1)||(pan!=-1))
				{
					unsigned char *act=cp;
					*cp++=cmdPlayNote;
					if (ins!=-1)
					{
						*act|=cmdPlayIns;
						*cp++=ins;
					}
					if (nte&&(nte!=255))
					{
						*act|=cmdPlayNte;
						*cp++=nte+11;
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
					if (command1==0xDE)
					{
						*act|=cmdPlayDelay;
						*cp++=data1;
					} else if (command2==0xDE)
					{
						*act|=cmdPlayDelay;
						*cp++=data2;
					}

					if (nte==255)
						putcmd(&cp, cmdKeyOff, 0);
				}
/* E8x - Set Sample Status
 * x=0 no loop, x=1 loop, unidirectional, x=3 loop, bidirectional
 */
				switch (command1)
				{
					case 0x1:
						if (!data1)
							putcmd(&cp, cmdSpecial, cmdContMixPitchSlideUp);
						else
							if (data1<0xE0)
								putcmd(&cp, cmdPitchSlideUp, data1);
							else
								if (data1<0xF0)
									putcmd(&cp, cmdRowPitchSlideUp, (data1&0xF)<<1);
								else
									putcmd(&cp, cmdRowPitchSlideUp, (data1&0xF)<<4);
						break;
					case 0x2:
						if (!data1)
							putcmd(&cp, cmdSpecial, cmdContMixPitchSlideDown);
						else
							if (data1<0xE0)
								putcmd(&cp, cmdPitchSlideDown, data1);
							else
								if (data1<0xF0)
									putcmd(&cp, cmdRowPitchSlideDown, (data1&0xF)<<1);
								else
									putcmd(&cp, cmdRowPitchSlideDown, (data1&0xF)<<4);
						break;
					case 0x3:
						putcmd(&cp, cmdPitchSlideToNote, data1);
						break;
					case 0x4:
						putcmd(&cp, cmdPitchVibrato, data1);
						break;
					case 0x5:
						putcmd(&cp, cmdArpeggio, data1);
						break;
					case 0x1E:
						putcmd(&cp, cmdRowPanSlide, -data1*2);
						break;
					case 0x2E:
						putcmd(&cp, cmdRowPanSlide, data1*2);
						break;
					case 0x3E:
						putcmd(&cp, cmdSpecial, data1?cmdGlissOn:cmdGlissOff);
						break;
					case 0x4E:
						if (data1<4)
							putcmd(&cp, cmdPitchVibratoSetWave, data1);
						break;
					case 0x7E:
						if (data1<4)
							putcmd(&cp, cmdVolVibratoSetWave, data1);
						break;
					case 0x9E:
						putcmd(&cp, cmdRetrig, data1);
						break;
					case 0xCE:
						putcmd(&cp, cmdNoteCut, data1);
						break;
					case 0xFE:
						if (ofs&0xF00)
							putcmd(&cp, cmdOffsetHigh, ofs>>8);
						putcmd(&cp, cmdOffset, ofs);
						break;
				}

				switch (command2)
				{
					case 0x1:
						if (data2<0xE0)
							putcmd(&cp, cmdVolSlideUp, data2);
						else
							if (data2<0xF0)
								putcmd(&cp, cmdRowVolSlideUp, data2&0xF);
							else
								putcmd(&cp, cmdRowVolSlideUp, (data2&0xF)<<2);
						break;
					case 0x2:
						if (data2<0xE0)
							putcmd(&cp, cmdVolSlideDown, data2);
						else
							if (data2<0xF0)
								putcmd(&cp, cmdRowVolSlideDown, data2&0xF);
							else
								putcmd(&cp, cmdRowVolSlideDown, (data2&0xF)<<2);
						break;
					case 0x4:
						putcmd(&cp, cmdVolVibrato, data2);
						break;
					case 0x5:
						putcmd(&cp, cmdTremor, data2);
						break;
					case 0x1E:
						putcmd(&cp, cmdRowPanSlide, -data2*2);
						break;
					case 0x2E:
						putcmd(&cp, cmdRowPanSlide, data2*2);
						break;
					case 0x3E:
						putcmd(&cp, cmdSpecial, data1?cmdGlissOn:cmdGlissOff);
						break;
					case 0x4E:
						if (data2<4)
							putcmd(&cp, cmdPitchVibratoSetWave, data2);
						break;
					case 0x7E:
						if (data2<4)
							putcmd(&cp, cmdVolVibratoSetWave, data2);
						break;
					case 0x9E:
						putcmd(&cp, cmdRetrig, data2);
						break;
					case 0xCE:
						putcmd(&cp, cmdNoteCut, data2);
						break;
				}

				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
				}
			}

			trk=&m->tracks[j*(m->channum+1)+i];
			len=tp-temptrack;

			if (!len)
				trk->ptr=trk->end=0;
			else {
				trk->ptr=malloc(sizeof(uint8_t)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
					return errAllocMem;
				memcpy(trk->ptr, temptrack, len);
			}
		}

		tp=temptrack;
		buf=**patdata;

		for (row=0; row<m->patterns[j].patlen; row++, buf+=6)
		{
			uint8_t *cp=tp+2;

			unsigned int q;

			if (!row&&(j==ordtab[0]))
			{
				if (mdlhead.speed!=6)
					putcmd(&cp, cmdTempo, mdlhead.speed);
				if (mdlhead.bpm!=125)
					putcmd(&cp, cmdSpeed, mdlhead.bpm);
				if (mdlhead.mainvol!=255)
					putcmd(&cp, cmdGlobVol, mdlhead.mainvol);
			}

			for (q=0; q<m->channum; q++)
			{
				uint8_t command1=buf[256*6*q+3]&0xF;
				uint8_t command2=buf[256*6*q+3]>>4;
				uint8_t data1=buf[256*6*q+4];
				uint8_t data2=buf[256*6*q+5];

				switch (command1)
				{
					case 0x7:
						if (data1)
							putcmd(&cp, cmdSpeed, data1);
						break;
					case 0xB:
						putcmd(&cp, cmdGoto, data1);
						break;
					case 0xD:
						putcmd(&cp, cmdBreak, (data1&0x0F)+(data1>>4)*10);
						break;
					case 0xE:
						switch (data1>>4)
						{
							case 0x6:
								putcmd(&cp, cmdSetChan, q);
								putcmd(&cp, cmdPatLoop, data1&0xF);
								break;
							case 0xE:
								putcmd(&cp, cmdPatDelay, data1&0xF);
								break;
							case 0xA:
								putcmd(&cp, cmdGlobVolSlide, data1&0xF);
								break;
							case 0xB:
								putcmd(&cp, cmdSetChan, q);
								putcmd(&cp, cmdGlobVolSlide, -(data1&0xF));
								break;
						}
						break;
					case 0xF:
						if (data1)
							putcmd(&cp, cmdTempo, data1);
						break;
					case 0xC:
						putcmd(&cp, cmdGlobVol, data1);
						break;
				}
				switch (command2)
				{
					case 0x7:
						if (data2)
							putcmd(&cp, cmdSpeed, data2);
						break;
					case 0xB:
						putcmd(&cp, cmdGoto, data2);
						break;
					case 0xD:
						putcmd(&cp, cmdBreak, (data2&0x0F)+(data2>>4)*10);
						break;
					case 0xE:
						switch (data2>>4)
						{
							case 0x6:
								putcmd(&cp, cmdSetChan, q);
								putcmd(&cp, cmdPatLoop, data2&0xF);
								break;
							case 0xE:
								putcmd(&cp, cmdPatDelay, data2&0xF);
								break;
							case 0xA:
								putcmd(&cp, cmdGlobVolSlide, data2&0xF);
								break;
							case 0xB:
								putcmd(&cp, cmdSetChan, q);
								putcmd(&cp, cmdGlobVolSlide, -(data2&0xF));
								break;
						}
						break;
					case 0xF:
						if (data2)
							putcmd(&cp, cmdTempo, data2);
						break;
					case 0xC:
						putcmd(&cp, cmdGlobVol, data2);
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

		trk=&m->tracks[j*(m->channum+1)+m->channum];
		len=tp-temptrack;

		if (!len)
			trk->ptr=trk->end=0;
		else {
			trk->ptr=malloc(sizeof(uint8_t)*len);
			trk->end=trk->ptr+len;
			if (!trk->ptr)
				return errAllocMem;
			memcpy(trk->ptr, temptrack, len);
		}
	}
	free(temptrack);
	free(trackends);
	free(trackptrs);
	free(trackbuf);
	free(patdata);

	if (ocpfilehandle_read_uint16_le (file, &waste16))
	{
		fprintf(stderr, __FILE__ ": fread() failed #21\n");
		return errFormStruc;
	}
	if (waste16!=0x4949)
		return errFormStruc;
	if (ocpfilehandle_read_uint32_le (file, &blklen))
	{
		fprintf(stderr, __FILE__ ": fread() failed #22\n");
		return errFormStruc;
	}

	inssav=0;
	if (ocpfilehandle_read_uint8 (file, &inssav))
	{
		fprintf(stderr, __FILE__ ": fread() failed #23\n");
		return errFormStruc;
	}

	m->instnum=255;
	m->modsampnum=0;
	m->envnum=192;

/*  envelope **envs=new envelope *[255]; */
	r.msmps=calloc(sizeof(struct gmdsample *), 255);
	r.inssampnum=calloc(sizeof(int), 255);
/*  int *insenvnum=new int [255]; */
	if (/*!envs||!insenvnum||*/!r.inssampnum||!r.msmps||!mpAllocInstruments(m, m->instnum))
	{
		FreeResources(&r);
		return errAllocMem;
	}

/*  memset(envs, 0, 4*255); */
/*  memset(insenvnum, 0, 4*255); */

	for (j=0; j<inssav; j++)
	{
		uint8_t insnum;
		struct gmdinstrument *ip;
		int note;

		if (ocpfilehandle_read_uint8 (file, &insnum))
		{
			fprintf(stderr, __FILE__ ": fread() failed #24\n");
			FreeResources(&r);
			return errFormStruc;
		}
		insnum--;

		ip=&m->instruments[insnum];

		if (ocpfilehandle_read_uint8 (file, &waste8))
		{
			fprintf(stderr, __FILE__ ": fread() failed #25\n");
			FreeResources(&r);
			return errFormStruc;
		}
		r.inssampnum[j] = waste8;
		if (file->read (file, ip->name, 32) != 32)
		{
			fprintf(stderr, __FILE__ ": fread() failed #26\n");
			FreeResources(&r);
			return errFormStruc;
		}
		ip->name[31]=0;
		r.msmps[j]=malloc(sizeof(struct gmdsample)*r.inssampnum[j]);
/*
		envs[insnum]=new envelope [r.inssampnum[j]];
*/
		if (!r.msmps[j]/*||!envs[insnum]*/)
		{
			FreeResources(&r);
			return errAllocMem;
		}

		note=0;
		for (i=0; i<r.inssampnum[j]; i++)
		{
			struct __attribute__((packed))
			{
				uint8_t smp;
				uint8_t highnote;
				uint8_t vol;
				uint8_t volenv;
				uint8_t pan;
				uint8_t panenv;
				uint16_t fadeout;
				uint8_t vibspd;
				uint8_t vibdep;
				uint8_t vibswp;
				uint8_t vibfrm;
				uint8_t res1;
				uint8_t pchenv;
			} mdlmsmp;
			struct gmdsample *sp;

			if (file->read (file, &mdlmsmp, sizeof(mdlmsmp)) != sizeof(mdlmsmp))
			{
				fprintf(stderr, __FILE__ ": fread() failed #27\n");
				FreeResources(&r);
				return errFormStruc;
			}
			mdlmsmp.fadeout = uint16_little (mdlmsmp.fadeout);
			if ((mdlmsmp.highnote+12)>128)
				mdlmsmp.highnote=128-12;
			while (note<(mdlmsmp.highnote+12))
				ip->samples[note++]=m->modsampnum;
			m->modsampnum++;

			sp=&r.msmps[j][i];
			*sp->name=0;
			sp->handle=mdlmsmp.smp-1;
			sp->normnote=0;
			sp->stdvol=(mdlmsmp.volenv&0x40)?mdlmsmp.vol:-1;
			sp->stdpan=(mdlmsmp.panenv&0x40)?mdlmsmp.pan*2:-1;
			sp->opt=0;
			sp->volfade=mdlmsmp.fadeout;
			sp->vibspeed=0;
			sp->vibdepth=mdlmsmp.vibdep*4;
			sp->vibrate=mdlmsmp.vibspd<<7;
			sp->vibsweep=0xFFFF/(mdlmsmp.vibswp+1);
			sp->vibtype=mdlmsmp.vibfrm;
			sp->pchint=4;
			sp->volenv=(mdlmsmp.volenv&0x80)?(mdlmsmp.volenv&0x3F):0xFFFF;
			sp->panenv=(mdlmsmp.panenv&0x80)?(64+(mdlmsmp.panenv&0x3F)):0xFFFF;
			sp->pchenv=(mdlmsmp.pchenv&0x80)?(128+(mdlmsmp.pchenv&0x3F)):0xFFFF;;
/*
      if (mdlmsmp.vibdep&&mdlmsmp.vibspd)
      {
        sp.vibenv=m.envnum++;

        envelope &ep=envs[insnum][insenvnum[j]++];
        ep.speed=0;
        ep.opt=0;
        ep.len=512;
        ep.sustain=-1;
        ep.loops=0;
        ep.loope=512;
        ep.env=new unsigned char [512];
        if (!ep.env)
          return errAllocMem;
        unsigned char ph=0;
        for (k=0; k<512; k++)
        {
          ph=k*mdlmsmp.vibspd/2;
          switch (mdlmsmp.vibfrm)
          {
          case 0:
            ep.env[k]=128+((mdlmsmp.vibdep*vibsintab[ph])>>10);
            break;
          case 1:
            ep.env[k]=128+((mdlmsmp.vibdep*(64-(ph&128)))>>5);
            break;
          case 2:
            ep.env[k]=128+((mdlmsmp.vibdep*(128-ph))>>6);
            break;
          case 3:
            ep.env[k]=128+((mdlmsmp.vibdep*(ph-128))>>6);
            break;
          }
        }
      }
*/
		}
	}

	m->sampnum=255;
	if (!mpAllocModSamples(m, m->modsampnum)||!mpAllocEnvelopes(m, m->envnum)||!mpAllocSamples(m, m->sampnum))
		return errAllocMem;

	smpnum=0;
	/*  int envnum=192; */
	for (j=0; j<255; j++)
	{
		memcpy(m->modsamples+smpnum, r.msmps[j], sizeof (*m->modsamples)*r.inssampnum[j]);
		smpnum+=r.inssampnum[j];
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *    for (i=0; i<insenvnum[j]; i++)
 *      memcpy(&m.envelopes[envnum++], &envs[j][i], sizeof (*m.envelopes));*/
/*    delete envs[j]; */
	}
	FreeResources(&r);
/*  delete envs;*/
/*  delete insenvnum;*/

	if (ocpfilehandle_read_uint16_le (file, &blktype))
	{
		fprintf(stderr, __FILE__ ": fread() failed #28\n");
		return errFormStruc;
	}
	if (ocpfilehandle_read_uint32_le (file, &blklen))
	{
		fprintf(stderr, __FILE__ ": fread() failed #29\n");
		return errFormStruc;
	}

	if (blktype==0x4556)
	{
		uint8_t envnum;

		if (ocpfilehandle_read_uint8 (file, &envnum))
		{
			fprintf(stderr, __FILE__ ": fread() failed #30\n");
			return errFormStruc;
		}
		for (i=0; i<envnum; i++)
		{
			struct __attribute__((packed))
			{
				uint8_t num;
				uint8_t env[15][2];
				uint8_t sus;
				uint8_t loop;
			} env;
			struct gmdenvelope *e;
			int k,l;

			if (file->read (file, &env, sizeof (env)) != sizeof (env))
			{
				fprintf(stderr, __FILE__ ": fread() failed #31\n");
				return errFormStruc;
			}
			if (env.env[0][0]!=1)
				continue;
			e=&m->envelopes[env.num];

			e->type=((env.sus&16)?mpEnvSLoop:0)|((env.sus&32)?mpEnvLoop:0);
			e->speed=0;
			l=-1;
			for (j=0; j<15; j++)
			{
				if (!env.env[j][0])
					break;
				l+=env.env[j][0];
				if ((env.loop&15)==j)
					e->loops=l;
				if ((env.loop>>4)==j)
					e->loope=l;
				if ((env.sus&15)==j)
				{
					e->sloops=l;
					e->sloope=l+1;
				}
			}
			if ((e->type&mpEnvSLoop)&&(e->type&mpEnvLoop)&&(e->sloope>e->loope))
			{
				e->sloops=e->loops;
				e->sloope=e->loope;
			}
			e->len=l;
			e->env=malloc(sizeof(uint8_t)*(l+1));
			if (!e->env)
				return errAllocMem;
			l=1;
			e->env[0]=env.env[0][1]<<2;
			for (j=0; j<15; j++)
			{
				if (!env.env[j+1][0])
					break;
				for (k=1; k<=env.env[j+1][0]; k++)
					e->env[l++]=4*env.env[j][1]+4*k*(env.env[j+1][1]-env.env[j][1])/env.env[j+1][0];
			}
		}

		if (ocpfilehandle_read_uint16_le (file, &blktype))
		{
			fprintf(stderr, __FILE__ ": fread() failed #32\n");
			return errFormStruc;
		}
		if (ocpfilehandle_read_uint32_le (file, &blklen))
		{
			fprintf(stderr, __FILE__ ": fread() failed #33\n");
			return errFormStruc;
		}
	}

	if (blktype==0x4550)
	{
		uint8_t envnum;
		if (ocpfilehandle_read_uint8 (file, &envnum))
		{
			fprintf(stderr, __FILE__ ": fread() failed #34\n");
			return errFormStruc;
		}
		for (i=0; i<envnum; i++)
		{
			struct __attribute__((packed))
			{
				uint8_t num;
				uint8_t env[15][2];
				uint8_t sus;
				uint8_t loop;
			} env;
			struct gmdenvelope *e;
			int k,l;

			if (file->read (file, &env, sizeof (env)) != sizeof (env))
			{
				fprintf(stderr, __FILE__ ": fread() failed #35\n");
				return errFormStruc;
			}
			if (env.env[0][0]!=1)
				continue;
			e=&m->envelopes[64+env.num];

			e->type=((env.sus&16)?mpEnvSLoop:0)|((env.sus&32)?mpEnvLoop:0);
			e->speed=0;
			l=-1;
			for (j=0; j<15; j++)
			{
				if (!env.env[j][0])
					break;
				l+=env.env[j][0];
				if ((env.loop&15)==j)
					e->loops=l;
				if ((env.loop>>4)==j)
					e->loope=l;
				if ((env.sus&15)==j)
				{
					e->sloops=l;
					e->sloope=l+1;
				}
			}
			if ((e->type&mpEnvSLoop)&&(e->type&mpEnvLoop)&&(e->sloope>e->loope))
			{
				e->sloops=e->loops;
				e->sloope=e->loope;
			}
			e->len=l;
			e->env=malloc(sizeof(uint8_t)*(l+1));
			if (!e->env)
				return errAllocMem;
			l=1;
			e->env[0]=env.env[0][1]<<2;
			for (j=0; j<15; j++)
			{
				if (!env.env[j+1][0])
					break;
				for (k=1; k<=env.env[j+1][0]; k++)
					e->env[l++]=4*env.env[j][1]+4*k*(env.env[j+1][1]-env.env[j][1])/env.env[j+1][0];
			}
		}

		if (ocpfilehandle_read_uint16_le (file, &blktype))
		{
			fprintf(stderr, __FILE__ ": fread() failed #36\n");
			return errFormStruc;
		}
		if (ocpfilehandle_read_uint32_le (file, &blklen))
		{
			fprintf(stderr, __FILE__ ": fread() failed #37\n");
			return errFormStruc;
		}
	}

	if (blktype==0x4546)
	{
		uint8_t envnum;
		if (ocpfilehandle_read_uint8 (file, &envnum))
		{
			fprintf(stderr, __FILE__ ": fread() failed #38\n");
			return errFormStruc;
		}

		for (i=0; i<envnum; i++)
		{
			struct __attribute__((packed))
			{
				uint8_t num;
				uint8_t env[15][2];
				uint8_t sus;
				uint8_t loop;
			} env;

			struct gmdenvelope *e;
			int k,l;

			if (file->read (file, &env, sizeof (env)) != sizeof (env))
			{
				fprintf(stderr, __FILE__ ": fread() failed #39\n");
				return errFormStruc;
			}

			if (env.env[0][0]!=1)
				continue;
			e=&m->envelopes[128+env.num];

			e->type=((env.sus&32)?mpEnvLoop:0)|((env.sus&16)?mpEnvSLoop:0);
			e->speed=0;
			l=-1;
			for (j=0; j<15; j++)
			{
				if (!env.env[j][0])
					break;
				l+=env.env[j][0];
				if ((env.loop&15)==j)
					e->loops=l;
				if ((env.loop>>4)==j)
					e->loope=l;
				if ((env.sus&15)==j)
				{
					e->sloops=l;
					e->sloope=l+1;
				}
			}
			if ((e->type&mpEnvSLoop)&&(e->type&mpEnvLoop)&&(e->sloope>e->loope))
			{
				e->sloops=e->loops;
				e->sloope=e->loope;
			}
			e->len=l;
			e->env=malloc(sizeof(uint8_t)*(l+1));
			if (!e->env)
				return errAllocMem;
			l=1;
			e->env[0]=env.env[0][1]<<2;
			for (j=0; j<15; j++)
			{
				if (!env.env[j+1][0])
					break;
				for (k=1; k<=env.env[j+1][0]; k++)
					e->env[l++]=4*env.env[j][1]+4*k*(env.env[j+1][1]-env.env[j][1])/env.env[j+1][0];
			}
		}

		if (ocpfilehandle_read_uint16_le (file, &blktype))
		{
			fprintf(stderr, __FILE__ ": fread() failed #40\n");
			return errFormStruc;
		}
		if (ocpfilehandle_read_uint32_le (file, &blklen))
		{
			fprintf(stderr, __FILE__ ": fread() failed #41\n");
			return errFormStruc;
		}
	}

	if (blktype!=0x5349)
		return errFormStruc;

	if (ocpfilehandle_read_uint8 (file, &smpsav))
	{
		fprintf(stderr, __FILE__ ": fread() failed #42\n");
		return errFormStruc;
	}
	memset(packtype, 0xFF, 255);

	for (i=0; i<smpsav; i++)
	{
		struct __attribute__((packed))
		{
			uint8_t num;
			char name[32];
			char filename[8];
			uint32_t rate;
			uint32_t len;
			uint32_t loopstart;
			uint32_t replen;
			uint8_t vol;
			uint8_t opt;
		} mdlsmp;

		struct sampleinfo *sip;

		if (file->read (file, &mdlsmp, sizeof (mdlsmp)) != sizeof (mdlsmp))
		{
			fprintf(stderr, __FILE__ ": fread() failed #43\n");
			return errFormStruc;
		}
		mdlsmp.rate = uint32_little (mdlsmp.rate);
		mdlsmp.len = uint32_little (mdlsmp.len);
		mdlsmp.loopstart = uint32_little (mdlsmp.loopstart);
		mdlsmp.replen = uint32_little (mdlsmp.replen);

		mdlsmp.name[31]=0;
		if (mdlsmp.opt&1)
		{
			mdlsmp.len>>=1;
			mdlsmp.loopstart>>=1;
			mdlsmp.replen>>=1;
		}

		for (j=0; j<m->modsampnum; j++)
			if (m->modsamples[j].handle==(mdlsmp.num-1))
				strcpy(m->modsamples[j].name, mdlsmp.name);

		sip=&m->samples[mdlsmp.num-1];

		sip->ptr=0;
		sip->length=mdlsmp.len;
		sip->loopstart=mdlsmp.loopstart;
		sip->loopend=mdlsmp.loopstart+mdlsmp.replen;
		sip->samprate=mdlsmp.rate;
		sip->type=((mdlsmp.opt&1)?mcpSamp16Bit:0)|(mdlsmp.replen?mcpSampLoop:0)|((mdlsmp.opt&2)?mcpSampBiDi:0);

		packtype[mdlsmp.num-1]=(mdlsmp.opt>>2)&3;
	}

	if (ocpfilehandle_read_uint16_le (file, &waste16))
	{
		fprintf(stderr, __FILE__ ": fread() failed #44\n");
		return errFormStruc;
	}
	if (waste16!=0x4153)
		return errFormStruc;
	if (ocpfilehandle_read_uint32_le (file, &blklen))
	{
		fprintf(stderr, __FILE__ ": fread() failed #45\n");
		return errFormStruc;
	}

	for (i=0; i<255; i++)
	{
		struct sampleinfo *sip;
		int bit16;
		uint32_t packlen;
		uint8_t *packbuf;
		uint8_t dlt;

		if (packtype[i]==255)
			continue;

		sip=&m->samples[i];
		bit16=!!(sip->type&mcpSamp16Bit);

		sip->ptr=malloc(sizeof(uint8_t)*((sip->length+8)<<bit16));
		if (!sip->ptr)
			return errAllocMem;

		if (packtype[i]==0)
		{
			if (file->read (file, sip->ptr, sip->length<<bit16) != (sip->length<<bit16))
			{
				fprintf(stderr, __FILE__ ": fread() failed #46\n");
				return errFormStruc;
			}
			continue;
		}

		if (ocpfilehandle_read_uint32_le (file, &packlen))
		{
			fprintf(stderr, __FILE__ ": fread() failed #47\n");
			return errFormStruc;
		}
		packbuf=malloc(sizeof(uint8_t)*(packlen+4));

		if (!packbuf)
			return errAllocMem;
		if (file->read (file, packbuf, packlen) != packlen)
		{
			fprintf(stderr, __FILE__ ": fread() failed #48\n");
			free(packbuf);
			return errFormStruc;
		}
		bitnum=8;
		ibuf=packbuf;
		bitlen = packlen;

		dlt=0;
		bit16=packtype[i]==2;
		for (j=0; j<sip->length; j++)
		{
			uint8_t lowbyte=lowbyte; /* supress warning.. safe to use below, since it depends on bit16 */
			uint8_t byte;
			uint8_t sign;
			if (bit16)
				lowbyte=readbits(8);

			sign=readbits(1);
			if (readbits(1))
				byte=readbits(3);
			else {
				byte=8;
				while (!readbits(1))
					byte+=16;
				byte+=readbits(4);
			}
			if (sign)
				byte=~byte;
			dlt+=byte;
			if (!bit16)
				((uint8_t *)sip->ptr)[j]=dlt;
			else
				((uint16_t *)sip->ptr)[j]=(dlt<<8)|lowbyte;
		}
		free(packbuf);
	}

	return errOk;
}

struct gmdloadstruct mpLoadMDL = { _mpLoadMDL };

struct linkinfostruct dllextinfo = {.name = "gmdlmdl", .desc = "OpenCP Module Loader: *.MDL (c) 1994-21 Niklas Beisert, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
