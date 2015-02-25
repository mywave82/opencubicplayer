/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMDPlay loader for Velvet Studio modules
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
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *
 * ENVELOPES & SUSTAIN!!!
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

static inline void putcmd(unsigned char **p, unsigned char c, unsigned char d)
{
	*(*p)++=c;
	*(*p)++=d;
}

static const unsigned char envsin[513]=
{

	  0,  1,  2,  2,  3,  4,  5,  5,  6,  7,  8,  8,  9, 10, 11, 12, 12, 13, 14, 15, 16, 16, 17, 18, 19, 19, 20,
	 21, 22, 22, 23, 24, 25, 26, 26, 27, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 39, 39, 40, 41,
	 42, 43, 43, 44, 45, 46, 46, 47, 48, 49, 49, 50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 58, 58, 59, 60, 61, 61,
	 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 70, 70, 71, 72, 73, 74, 74, 75, 76, 76, 77, 78, 79, 79, 80, 81, 82,
	 82, 83, 84, 84, 85, 86, 87, 87, 88, 89, 90, 90, 91, 92, 93, 93, 94, 95, 96, 96, 97, 98, 98, 99,100,100,101,
	102,103,103,104,105,106,106,107,108,108,109,110,110,111,112,113,113,114,115,115,116,117,117,118,119,119,120,
	121,121,122,123,123,124,125,126,126,127,127,128,129,130,130,131,132,132,133,134,134,135,136,136,137,138,138,
	139,139,140,141,141,142,143,143,144,145,145,146,146,147,148,148,149,150,150,151,152,152,153,153,154,155,155,
	156,156,157,158,158,159,160,160,161,161,162,163,163,164,164,165,166,166,167,167,168,168,169,170,170,171,171,
	172,173,173,174,174,175,175,176,177,177,178,178,179,179,180,180,181,181,182,182,183,184,184,185,185,186,186,
	187,187,188,188,189,190,190,191,191,192,192,193,193,194,194,195,195,196,196,197,197,198,198,199,199,200,200,
	201,201,201,202,202,203,203,204,204,205,205,206,206,207,207,207,208,208,209,209,210,210,211,211,211,212,212,
	213,213,214,214,214,215,215,216,216,216,217,217,218,218,219,219,219,220,220,221,221,221,222,222,223,223,223,
	224,224,224,225,225,225,226,226,227,227,227,228,228,228,229,229,229,230,230,230,231,231,231,232,232,232,233,
	233,233,234,234,234,234,235,235,235,236,236,236,237,237,237,237,238,238,238,239,239,239,239,240,240,240,240,
	241,241,241,241,242,242,242,242,243,243,243,243,244,244,244,244,244,245,245,245,245,246,246,246,246,246,247,
	247,247,247,247,248,248,248,248,248,248,249,249,249,249,249,249,250,250,250,250,250,250,250,251,251,251,251,
	251,251,251,252,252,252,252,252,252,252,252,253,253,253,253,253,253,253,253,253,253,253,254,254,254,254,254,
	254,254,254,254,254,254,254,254,254,254,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};


static int _mpLoadAMS(struct gmdmodule *m, FILE *file)
{
	int retval;

	unsigned char sig[8];
	uint16_t filever;

	struct __attribute__((packed))
	{
		uint8_t ins;
		uint16_t pat;
		uint16_t pos;
		uint16_t bpm;
		uint8_t speed;
		uint8_t defchn;
		uint8_t defcmd;
		uint8_t defrow;
		uint16_t flags;
	} hdr;

	uint16_t *ordlist=0;
	struct sampleinfo **smps=0;
	struct gmdsample **msmps=0;
	unsigned int *instsampnum;

	unsigned int i,j,t;

	uint8_t namelen;
	unsigned char shadowedby[256];
	uint32_t packlen;

	unsigned char *temptrack=0;
	unsigned int buflen;
	unsigned char *buffer=0;

	int sampnum;

	mpReset(m);

	if (fread(&sig, 8, 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #1\n");
	if (!memcmp(sig, "Extreme", 7))
		return errFormOldVer;
	if (memcmp(sig, "AMShdr\x1A", 7))
		return errFormSig;

	if (fread(m->name, sig[7], 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #2\n");
	m->name[sig[7]]=0;

	if (fread(&filever, sizeof(filever), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #3\n");
	filever = uint16_little (filever);
	if ((filever!=0x201)&&(filever!=0x202))
		return errFormOldVer;


	if (filever==0x201)
	{
		struct __attribute__((packed))
		{
			uint8_t ins;
			uint16_t pat;
			uint16_t pos;
			uint8_t bpm;
			uint8_t speed;
			uint8_t flags;
			/*  1         Flags:mfsspphh
			 *  ||||||\\- Pack byte header
			 *  ||||\\--- Pack byte patterns
			 *  ||\\----- Pack byte samples
			 *  \\------- MIDI channels are used in tune. */
		} oldhdr;

		if (fread(&oldhdr, sizeof(oldhdr), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #4\n");
		hdr.ins = oldhdr.ins;
		hdr.pat = uint16_little (oldhdr.pat);
		hdr.pos = uint16_little (oldhdr.pos);
		hdr.bpm = oldhdr.bpm<<8;
		hdr.speed = oldhdr.speed;
		hdr.flags = (oldhdr.flags&0xC0)|0x20;
	} else {
		if (fread(&hdr, sizeof(hdr), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #5\n");

		hdr.pat = uint16_little (hdr.pat);
		hdr.pos = uint16_little (hdr.pos);
		hdr.bpm = uint16_little (hdr.bpm);
		hdr.flags = uint16_little (hdr.flags);
	}

	m->options=((hdr.flags&0x40)?MOD_EXPOFREQ:0)|MOD_EXPOPITCHENV;

	m->channum=32;
	m->instnum=hdr.ins;
	m->envnum=hdr.ins*3;
	m->patnum=hdr.pat+1;
	m->ordnum=hdr.pos;
	m->endord=hdr.pos;
	m->tracknum=33*hdr.pat+1;
	m->loopord=0;

	if (!(ordlist=malloc(sizeof(uint16_t)*hdr.pos)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	if (!(smps=malloc(sizeof(struct sampleinfo *)*m->instnum)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	memset(smps, 0, (sizeof(struct sampleinfo *)*m->instnum));
	if (!(msmps=malloc(sizeof(struct gmdsample *)*m->instnum)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	memset(msmps, 0, sizeof(struct gmdsample *)*m->instnum);
	if (!(instsampnum=malloc(sizeof(int)*m->instnum)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	if (!mpAllocInstruments(m, m->instnum)||!mpAllocPatterns(m, m->patnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocEnvelopes(m, m->envnum)||!mpAllocOrders(m, m->ordnum))
	{
		retval=errAllocMem;
		goto safeout;
	}

	m->sampnum=0;
	m->modsampnum=0;
	for (i=0; i<m->instnum; i++)
	{
		struct gmdinstrument *ip=&m->instruments[i];
		uint8_t smpnum;

		uint8_t samptab[120];
		struct __attribute__((packed))
		{
			uint8_t speed;
			uint8_t sustain;
			uint8_t loopstart;
			uint8_t loopend;
			uint8_t points;
			uint8_t data[64][3];
		} envs[3];
		uint16_t envflags;

		/* uint8_t vibsweep; */
		uint8_t shadowinst;
		uint16_t volfade;

		uint8_t pchint;

		smps[i]=0;
		msmps[i]=0;
		shadowedby[i]=0;

		if (fread(&namelen, sizeof(namelen), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #6\n");
		if (namelen>=(sizeof(ip->name)-1))
		{
			fprintf(stderr, "AMS: Instrument name too long\n");
			retval=errFormStruc;
			goto safeout;
		}
		if (fread(ip->name, namelen, 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #7\n");
		ip->name[namelen]=0;

		if (fread(&smpnum, sizeof(smpnum), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #8\n");
		instsampnum[i]=smpnum;

		if (!smpnum)
			continue;

		msmps[i]=malloc(sizeof(struct gmdsample)*smpnum);
		smps[i]=malloc(sizeof(struct sampleinfo)*smpnum);
		if (!smps[i]||!msmps[i])
		{
			retval=errAllocMem;
			goto safeout;
		}

		memset(msmps[i], 0, sizeof(**msmps)*smpnum);
		memset(smps[i], 0, sizeof(**smps)*smpnum);

		if (fread(samptab, sizeof(samptab), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #9\n");
		for (j=0; j<3; j++)
		{
			if (fread(&envs[j], 5, 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #10\n");
			if (envs[j].points>64)
			{
				fprintf(stderr, "AMS: Too many points in envelope\n");
				retval=errFormStruc;
				goto safeout;
			}
			if (fread(envs[j].data, envs[j].points*3, 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #11\n");
		}

		/* vibsweep=0; */

		if (fread(&shadowinst, sizeof(shadowinst), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #12\n");
		if (filever==0x201)
		{
			/* TODO? vibsweep=shadowinst; */
			shadowinst=0;
		}
		shadowedby[i]=shadowinst;

		if (fread(&volfade, sizeof(volfade), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #13\n");
		volfade = uint16_little (volfade);
		if (fread(&envflags, sizeof(envflags), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #14\n");
		envflags = uint16_little (envflags);

		pchint=(volfade>>12)&3;
		volfade&=0xFFF;

		for (t=0; t<envs[0].points; t++)
			envs[0].data[t][2]<<=1;

		for (j=0; j<3; j++)
			if (envflags&(4<<(3*j)))
			{
				uint32_t envlen=0;
				uint8_t *env;
				uint32_t k, p, h;
				int32_t sus;
				int32_t lst;
				int32_t lend;

				for (t=1; t<envs[j].points; t++)
					envlen+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];

				env=malloc(sizeof(uint8_t)*(envlen+1));
				if (!env)
				{
					retval=errAllocMem;
					goto safeout;
				}

				p=0;
				h=envs[j].data[0][2];
				for (t=1; t<envs[j].points; t++)
				{
					uint32_t l=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
					uint32_t dh=envs[j].data[t][2]-h;
					switch (envs[j].data[t][1]&6)
					{
						case 0:
							for (k=0; k<l; k++)
								env[p++]=h+dh*k/l;
							break;
						case 2:
							for (k=0; k<l; k++)
								env[p++]=h+dh*envsin[512*k/l]/256;
							break;
						case 4:
							for (k=0; k<l; k++)
								env[p++]=h+dh*(255-envsin[512-512*k/l])/256;
							break;
					}
					h+=dh;
				}
				env[p]=h;

				sus=-1;
				lst=-1;
				lend=-1;

				if (envflags&(2<<(3*j)))
				{
					sus=0;
					for (t=1; t<envs[j].sustain; t++)
						sus+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
				}
				if (envflags&(1<<(3*j)))
				{
					lst=0;
					lend=0;
					for (t=1; t<envs[j].loopstart; t++)
						lst+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
					for (t=1; t<envs[j].loopend; t++)
						lend+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
				}

				m->envelopes[i*3+j].env=env;
				m->envelopes[i*3+j].len=envlen;
				m->envelopes[i*3+j].type=0;
				m->envelopes[i*3+j].speed=envs[j].speed;

				if (sus!=-1)
				{
					m->envelopes[i*3+j].sloops=sus;
					m->envelopes[i*3+j].sloope=sus+1;
					m->envelopes[i*3+j].type=mpEnvSLoop;
				}
				if (lst!=-1)
				{
					if (envflags&(0x200<<j))
					{
						if (lend<sus)
						{
							m->envelopes[i*3+j].sloops=lst;
							m->envelopes[i*3+j].sloope=lend;
							m->envelopes[i*3+j].type=mpEnvSLoop;
						}
					} else {
						m->envelopes[i*3+j].loops=lst;
						m->envelopes[i*3+j].loope=lend;
						m->envelopes[i*3+j].type=mpEnvLoop;
					}
				}
			}
		memset(ip->samples, -1, 128*2);

		for (j=0; j<smpnum; j++, m->sampnum++, m->modsampnum++)
		{
			struct gmdsample *sp=&msmps[i][j];
			struct sampleinfo *sip=&smps[i][j];

			struct __attribute__((packed))
			{
				uint32_t loopstart;
				uint32_t loopend;
				uint16_t samprate;
				uint8_t panfine;
				uint16_t rate;
				int8_t relnote;
				uint8_t vol;
				uint8_t flags; /* bit 6: direction */
			} amssmp;


			int k;
			for (k=0; k<116; k++)
				if (samptab[k]==j)
					ip->samples[k+12]=m->modsampnum;

			sp->handle=0xFFFF;
			sp->volenv=0xFFFF;
			sp->panenv=0xFFFF;
			sp->pchenv=0xFFFF;
			sp->volfade=0xFFFF;

			if (fread(&namelen, sizeof(namelen), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #15\n");
			if (namelen>=(sizeof(sp->name)-1))
			{
				fprintf(stderr, "AMS: Sample name too long\n");
				retval=errFormStruc;
				goto safeout;
			}
			if (fread(sp->name, namelen, 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #16\n");
			sp->name[namelen]=0;
			if (fread(&sip->length, sizeof(sip->length), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #17\n");

			sip->length = uint32_little (sip->length);

			if (!sip->length)
				continue;

			if (fread(&amssmp, sizeof(amssmp), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #18\n");
			amssmp.loopstart = uint32_little (amssmp.loopstart);
			amssmp.loopend   = uint32_little (amssmp.loopend);
			amssmp.samprate  = uint16_little (amssmp.samprate);
			amssmp.rate      = uint16_little (amssmp.rate);

			sp->stdpan=(amssmp.panfine&0xF0)?((amssmp.panfine>>4)*0x11):-1;
			sp->stdvol=amssmp.vol*2;
			sp->normnote=-amssmp.relnote*256-((signed char)(amssmp.panfine<<4))*2;
			sp->opt=(amssmp.flags&0x04)?MP_OFFSETDIV2:0;

			sp->volfade=volfade;
			sp->pchint=pchint;
			sp->volenv=m->envelopes[3*i+0].env?(3*(signed)i+0):-1;
			sp->panenv=m->envelopes[3*i+1].env?(3*(signed)i+1):-1;
			sp->pchenv=m->envelopes[3*i+2].env?(3*(signed)i+2):-1;

			sip->loopstart=amssmp.loopstart;
			sip->loopend=amssmp.loopend;
			sip->samprate=amssmp.rate;
			sip->type=((amssmp.flags&0x04)?mcpSamp16Bit:0)|((amssmp.flags&0x08)?mcpSampLoop:0)|((amssmp.flags&0x10)?mcpSampBiDi:0);
		}
	}

	if (fread(&namelen, sizeof(unsigned char), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #19\n");
	if (namelen>=((sizeof(m->composer)-1)))
	{
		fprintf(stderr, "AMS: Composer name too long\n");
		retval=errFormStruc;
		goto safeout;
	}
	if (fread(m->composer, namelen, 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #20\n");
	m->composer[namelen]=0;
	for (i=0; i<32; i++)
	{
		if (fread(&namelen, sizeof(unsigned char), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #21\n");
		fseek(file, namelen, SEEK_CUR);
	}

	if (fread(&packlen, sizeof(uint32_t), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #22\n");
	packlen = uint32_little (packlen);
	fseek(file, packlen-4, SEEK_CUR);

	if (fread(ordlist, 2*hdr.pos, 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #23\n");

	for (i=0; i<m->ordnum; i++)
		m->orders[i]=(ordlist[i]<hdr.pat)?ordlist[i]:hdr.pat;

	for (i=0; i<32; i++)
		m->patterns[hdr.pat].tracks[i]=m->tracknum-1;
	m->patterns[hdr.pat].gtrack=m->tracknum-1;
	m->patterns[hdr.pat].patlen=64;

	temptrack=malloc(sizeof(unsigned char)*4000);
	buflen=0;
	buffer=0;
	if (!temptrack)
	{
		retval=errAllocMem;
		goto safeout;
	}

	m->channum=1;

	for (t=0; t<hdr.pat; t++)
	{
		uint32_t patlen;
		uint8_t maxrow;
		uint8_t chan;
		/* uint8_t maxcmd; */
		char patname[11];
		struct gmdpattern *pp;

		uint8_t *tp;
		uint8_t *buf;
		uint8_t *cp;
		uint16_t row=0;
		uint8_t another=1;

		struct gmdtrack *trk;
		uint16_t len;


		if (fread(&patlen, sizeof(uint32_t), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #24\n");
		patlen = uint32_little(patlen);
		if (fread(&maxrow, sizeof(uint8_t), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #25\n");
		if (fread(&chan, sizeof(uint8_t), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #26\n");
		if (fread(&namelen, sizeof(uint8_t), 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #27\n");

		if (namelen>=(sizeof(patname)-1))
		{
			fprintf(stderr, "AMS: Pattern name too long\n");
			retval=errFormStruc;
			goto safeout;
		}

		patlen-=3+namelen;

		if (fread(patname, namelen, 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #28\n");
		patname[namelen]=0;
		/* maxcmd=chan>>5; */
		chan&=0x1F;
		chan++;
		if (chan>m->channum)
			m->channum=chan;

		pp=&m->patterns[t];
		for (i=0; i<32; i++)
			pp->tracks[i]=t*33+i;
		pp->gtrack=t*33+32;
		pp->patlen=maxrow+1;
		strcpy(pp->name, patname);

		if (patlen>buflen)
		{
			buflen=patlen;
			free(buffer);
			buffer=malloc(sizeof(unsigned char)*buflen);
			if (!buffer)
			{
				retval=errAllocMem;
				goto safeout;
			}
		}
		if (fread(buffer, patlen, 1, file) != 1)
			fprintf(stderr, __FILE__ ": warning, read failed #29\n");

		for (i=0; i<chan; i++)
		{
			uint8_t *tp=temptrack;
			uint8_t *buf=buffer;

			unsigned int row=0;
			uint8_t another=1;

			uint8_t curchan;
			uint8_t anothercmd;
			uint8_t nte;
			uint8_t ins;
			uint8_t noteporta;
			int16_t delaynote;
			uint8_t cmds[7][2];
			uint8_t cmdnum;
			int16_t vol;
			int16_t pan;
			struct gmdtrack *trk;
			uint16_t len;

			while (row<=maxrow)
			{
				unsigned char *cp;

				if (!another||(*buf==0xFF))
				{
					if (another)
						buf++;
					row++;
					another=1;
					continue;
				}
				another=!(*buf&0x80);
				curchan=*buf&0x1F;
				anothercmd=1;
				nte=0;
				ins=0;
				noteporta=0;
				delaynote=-1;
				cmdnum=0;
				vol=-1;
				pan=-1;
/*
				if ((ordlist[0]==t)&&!row)
					pan=(i&1)?0xC0:0x40;
*/
				if (!(*buf++&0x40))
				{
					anothercmd=*buf&0x80;
					nte=*buf++&0x7F;
					ins=*buf++;
				}
				while (anothercmd)
				{
					anothercmd=*buf&0x80;
					cmds[cmdnum][0]=*buf++&0x7F;
					if (!(cmds[cmdnum][0]&0x40))
						switch (cmds[cmdnum][0])
						{
							case 0x08:
								pan=(*buf++&0x0F)*0x11;
								break;
							case 0x0C:
								vol=*buf++*2;
								break;
							case 0x03: case 0x05: case 0x15:
								noteporta=1;
								cmds[cmdnum++][1]=*buf++;
								break;
							case 0x0E:
								if ((*buf&0xF0)==0xD0)
									delaynote=*buf&0x0F;
								cmds[cmdnum++][1]=*buf++;
								break;
							default:
								cmds[cmdnum++][1]=*buf++;
						} else
							vol=(cmds[cmdnum][0]&0x3f)*4;
				}
				if (curchan!=i)
					continue;

				cp=tp+2;

				if ((ordlist[0]==t)&&!row)
					putcmd(&cp, cmdPlayNote|cmdPlayPan, (i&1)?0xC0:0x40);

				if (ins||nte||(vol!=-1)||(pan!=-1))
				{
					uint8_t *act=cp;

					*cp++=cmdPlayNote;
					if (ins)
					{
						*act|=cmdPlayIns;
						*cp++=ins-1;
					}
					if (nte>=2)
					{
						*act|=cmdPlayNte;
						*cp++=(nte+10)|(noteporta?0x80:0);
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
					if (delaynote!=-1)
					{
						*act|=cmdPlayDelay;
						*cp++=delaynote;
					}
					if (nte==1)
						putcmd(&cp, cmdKeyOff, 0);
				}

				for (j=0; j<cmdnum; j++)
				{
					uint8_t data=cmds[j][1];
					switch (cmds[j][0])
					{
						case 0x0:
							if (data)
								putcmd(&cp, cmdArpeggio, data);
							break;
						case 0x1:
							putcmd(&cp, cmdPitchSlideUp, data);
							break;
						case 0x2:
							putcmd(&cp, cmdPitchSlideDown, data);
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
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
							break;
						case 0x6:
							putcmd(&cp, cmdPitchVibrato, 0);
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
							else
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
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
							break;
						case 0xE:
							cmds[j][0]=data>>4;
							data&=0x0F;
							switch (cmds[j][0])
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
										putcmd(&cp, cmdPitchVibratoSetWave, data);
									break;
								case 0x7:
									if (data<4)
										putcmd(&cp, cmdVolVibratoSetWave, data);
									break;
								case 0x8:
									if (!(data&0x0F))
										putcmd(&cp, cmdSetLoop, 0);
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
							putcmd(&cp, cmdRowPitchSlideUp, data<<2);
							break;
						case 0x12:
							putcmd(&cp, cmdRowPitchSlideDown, data<<2);
							break;
						case 0x13:
							putcmd(&cp, cmdRetrig, data);
							break;
						case 0x15:
							putcmd(&cp, cmdPitchSlideToNote, 0);
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
							break;
						case 0x16:
							putcmd(&cp, cmdPitchVibrato, 0);
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
							break;
						case 0x18:
							if (!data)
								putcmd(&cp, cmdPanSlide, 0);
							else if (data&0xF0)
								putcmd(&cp, cmdPanSlide, data>>4);
							else
								putcmd(&cp, cmdPanSlide, -(data&0xF));
							/*
							if ((data&0x0F)&&(data&0xF0))
								break;
							if (data&0xF0)
								data=-(data>>4)*4;
							else
								data=data*4;
							putcmd(cp, cmdPanSlide, data);
							break;
							*/
						case 0x1A:
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else
								if (data&0xF0)
									putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
								else
									putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
							break;
						case 0x1E:
							cmds[j][0]=data>>4;
							data&=0x0F;
							switch (cmds[j][0])
							{
								case 0x1:
									putcmd(&cp, cmdRowPitchSlideUp, data<<4);
									break;
								case 0x2:
									putcmd(&cp, cmdRowPitchSlideDown, data<<4);
									break;
								case 0xA:
									putcmd(&cp, cmdRowVolSlideUp, data<<1);
									break;
								case 0xB:
									putcmd(&cp, cmdRowVolSlideDown, data<<1);
									break;
							}
							break;
						case 0x1C:
							putcmd(&cp, cmdChannelVol, (data<=0x7F)?(data<<1):0xFF);
							break;
						case 0x20:
							putcmd(&cp, cmdKeyOff, data);
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

			trk=&m->tracks[t*33+i];
			len=tp-temptrack;

			if (!len)
				trk->ptr=trk->end=0;
			else {
				trk->ptr=malloc(sizeof(unsigned char)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
				{
					retval=errAllocMem;
					goto safeout;
				}
				memcpy(trk->ptr, temptrack, len);
			}
		}

		tp=temptrack;
		buf=buffer;

		cp=tp+2;

		if (ordlist[0]==t)
		{
			if (hdr.bpm&0xFF00)
				putcmd(&cp, cmdSpeed, hdr.bpm>>8);
			if (hdr.bpm&0xFF)
				putcmd(&cp, cmdFineSpeed, hdr.bpm);
			putcmd(&cp, cmdTempo, hdr.speed);
		}

		row=0;
		another=1;
		while (row<=maxrow)
		{
			uint8_t curchan;
			uint8_t anothercmd;
			uint8_t cmds[7][2];
			uint8_t cmdnum;

			if (!another||(*buf==0xFF))
			{
				if (another)
					buf++;
				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
					cp=tp+2;
				}
				row++;
				another=1;
				continue;
			}
			another=!(*buf&0x80);

			curchan=*buf&0x1F;
			anothercmd=1;
			cmdnum=0;
			if (!(*buf++&0x40))
			{
				anothercmd=*buf&0x80;
				buf+=2;
			}
			while (anothercmd)
			{
				anothercmd=*buf&0x80;
				cmds[cmdnum][0]=*buf++&0x7F;
				if (!(cmds[cmdnum][0]&0x40))
					cmds[cmdnum++][1]=*buf++;
			}

			if (curchan>=chan)
				continue;

			for (j=0; j<cmdnum; j++)
			{
				uint8_t data=cmds[j][1];
				switch (cmds[j][0])
				{
					case 0xB:
						putcmd(&cp, cmdGoto, data);
						break;
					case 0xD:
						putcmd(&cp, cmdBreak, (data&0x0F)+(data>>4)*10);
						break;
					case 0x1D:
						putcmd(&cp, cmdBreak, data);
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
					case 0x1F:
						if (data<10)
							putcmd(&cp, cmdFineSpeed, data);
						break;
					case 0x2A:
						if ((data&0x0F)&&(data&0xF0))
							break;
						putcmd(&cp, cmdSetChan, curchan);
						if (data&0xF0)
							putcmd(&cp, cmdGlobVolSlide, (data>>4)<<2);
						else
							putcmd(&cp, cmdGlobVolSlide, -(data<<2));
					case 0x2C:
						putcmd(&cp, cmdGlobVol, data*2);
						break;
				}
			}
		}

		trk=&m->tracks[t*33+32];
		len=tp-temptrack;

		if (!len)
			trk->ptr=trk->end=0;
		else {
			trk->ptr=malloc(sizeof(unsigned char)*len);
			trk->end=trk->ptr+len;
			if (!trk->ptr)
			{
				retval=errAllocMem;
				goto safeout;
			}
			memcpy(trk->ptr, temptrack, len);
		}
	}
	free(ordlist);
	free(temptrack);
	free(buffer);
	ordlist=0;
	temptrack=0;
	buffer=0;

	if (!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum))
	{
		retval=errAllocMem;
		goto safeout;
	}

	m->sampnum=0;
	m->modsampnum=0;

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];     NOT USED */
		for (j=0; j<instsampnum[i]; j++)
		{
			m->modsamples[m->modsampnum++]=msmps[i][j];
			m->samples[m->sampnum++]=smps[i][j];
		}
		free(msmps[i]);
		free(smps[i]);
	}

	free(smps);
	smps=0;
	free(msmps);
	msmps=0;

	sampnum=0;

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];  NOT USED */
		if (shadowedby[i])
		{
			sampnum+=instsampnum[i];
			continue;
		}
		for (j=0; j<instsampnum[i]; j++)
		{
			struct sampleinfo *sip=&m->samples[sampnum++];
			uint32_t packlena,
			         packlenb;
			uint8_t packbyte;

			uint8_t *packb;
			uint8_t *smpp;

			unsigned int p1,p2;

			uint8_t bitsel;
			int8_t cursmp;

			if (!sip->length)
				continue;

			if (fread(&packlena, sizeof(uint32_t), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #30\n");
			packlena = uint32_little (packlena);
			if (fread(&packlenb, sizeof(uint32_t), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #31\n");
			packlenb = uint32_little (packlenb);
			if (fread(&packbyte, sizeof(uint8_t), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #32\n");

			packb=malloc(sizeof(unsigned char)*(((packlenb>packlena)?packlenb:packlena)+16));
			smpp=malloc(sizeof(unsigned char)*(packlena+16));
			if (!smpp||!packb)
			{
				retval=errAllocMem;
				goto safeout;
			}
			if (fread(packb, packlenb, 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #33\n");

			p1=p2=0;

			while (p2<packlena)
				if (packb[p1]!=packbyte)
					smpp[p2++]=packb[p1++];
				else if (!packb[++p1])
					smpp[p2++]=packb[p1++-1];
				else {
					memset(smpp+p2, packb[p1+1], packb[p1]);
					p2+=packb[p1];
					p1+=2;
				}
			memset(packb, 0, packlena);

			p1=0;
			bitsel=0x80;

			for (p2=0; p2<packlena; p2++)
			{
				uint8_t cur=smpp[p2];

				for (t=0; t<8; t++)
				{
					packb[p1++]|=cur&bitsel;
					cur=(cur<<1)|((cur&0x80)>>7);
					if (p1==packlena)
					{
						p1=0;
						cur=(cur>>1)|((cur&1)<<7);
						bitsel>>=1;
					}
				}
			}

			cursmp=0;
			p1=0;
			for (p2=0; p2<packlena; p2++)
			{
				uint8_t cur=packb[p2];
				cursmp+=-((cur&0x80)?((-cur)|0x80):cur);
				smpp[p1++]=cursmp;
			}

			free(packb);

			sip->ptr=smpp;
			m->modsamples[sampnum-1].handle=sampnum-1;
		}
	}

/*
    for (i=0; i<m.instnum; i++)
      if (shadowedby[i])
      {
        for (j=0; j<m.instruments[i].sampnum; j++)
        {
          m.modsamples[m.instruments[i].samples[j]].handle=m.modsamples[m.instruments[shadowedby[i]-1].samples[j]].handle;
        }
      }
*/

	free(instsampnum);
	instsampnum=0;

	return errOk;

safeout:
	if (ordlist)
		free(ordlist);
	if (temptrack)
		free(temptrack);
	if (buffer)
		free(buffer);

	for (i=0; i<m->instnum; i++)
	{
		if (msmps)
			if (msmps[i])
				free(msmps[i]);
		if (smps)
			if (smps[i])
				free(smps[i]);
	}

	if (smps)
		free(smps);
	if (msmps)
		free(msmps);

	mpFree(m);

	return retval;
}

struct gmdloadstruct mpLoadAMS = { _mpLoadAMS };

struct linkinfostruct dllextinfo = {"gmdlams", "OpenCP Module Loader: *.669 (c) 1994-09 Niklas Beisert", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
