/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * XMPlay .MOD module loader
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 *    -added module flag "ismod" to handle some protracker quirks
 *    -enabled and added loaders for the auxiliary MOD formats (and removed
 *     them from playgmd)
 *    -added MODf file type for FastTracker-made MODs
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -pattern loading fixed, the old one also looked for patterns in
 *     orders which were not used (thanks to submissive's mod loader for
 *     helping debugging this)
 *    -set VBL timing as default for noisetracker and signature-less mods
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "xmplay.h"
#include "stuff/err.h"

static uint16_t modnotetab[85]=
{
	0xCFF, 0xC44, 0xB94, 0xAED, 0xA50, 0x9BC, 0x930, 0x8AC, 0x830, 0x7BA, 0x74B, 0x6E2,
	0x67F, 0x622, 0x5CA, 0x577, 0x528, 0x4DE, 0x498, 0x456, 0x418, 0x3DD, 0x3A5, 0x371,
	0x340, 0x311, 0x2E5, 0x2BB, 0x294, 0x26F, 0x24C, 0x22B, 0x20C, 0x1EE, 0x1D3, 0x1B9,
	0x1A0, 0x188, 0x172, 0x15E, 0x14A, 0x138, 0x126, 0x116, 0x106, 0x0F7, 0x0E9, 0x0DC,
	0x0D0, 0x0C4, 0x0B9, 0x0AF, 0x0A5, 0x09C, 0x093, 0x08B, 0x083, 0x07C, 0x075, 0x06E,
	0x068, 0x062, 0x05D, 0x057, 0x053, 0x04E, 0x04A, 0x045, 0x041, 0x03E, 0x03A, 0x037,
	0x034, 0x031, 0x02E, 0x02C, 0x029, 0x027, 0x025, 0x023, 0x021, 0x01F, 0x01D, 0x01C, 0
};

static inline uint32_t swapb2(uint16_t a)
{
#ifndef WORDS_BIGENDIAN
	return ((a&0xFF)<<9)|((a&0xFF00)>>7);
#else
	return ((uint32_t)a)<<1;
#endif
}

static int loadmod(struct xmodule *m, FILE *file, int chan, int sig, int opt)
{
	uint32_t l;
	unsigned int i;
	uint8_t orders[128];
	uint8_t ordn, loopp;
	uint16_t pn=0;
	uint16_t t;
	uint8_t *temppat;

/*
	fprintf(stderr, "loadmod enter, chan=%d sig=%d opt=%d\n", chan, sig, opt);*/

	m->envelopes=0;
	m->samples=0;
	m->instruments=0;
	m->sampleinfos=0;
	m->patlens=0;
	m->patterns=0;
	m->orders=0;
	m->nenv=0;
	m->linearfreq=0;
	m->ismod=!(opt&4);
	m->ft2_e60bug=0;

	fseek(file, 1080, SEEK_SET);
	if (fread(&l, sizeof(uint32_t), 1, file)!=1)
	{
		fprintf(stderr, __FILE__ ": warning: fread() failed #1\n");
		l=0;
	} else
		l = uint32_little (l);

	m->ninst=31;
	m->nchan=0;

#ifdef XM_LOAD_DEBUG
	fprintf(stderr, __FILE__ ": CHECKING FILE SIGNATURE\n");
	fprintf(stderr, __FILE__ ": signature: %c%c%c%c\n", (l>>24)&255, (l>>16)&255, (l>>8)&255, l&255);
#endif

	switch (l)
	{
		case 0x2E4B2E4D: /* M.K. */
		case 0x214B214D: /* M!K! */
		case 0x34544C46: /* FLT4 */
			m->nchan=4;
			m->ninst=31;
			break;
		case 0x2E542E4E: /* N.T. */
			m->nchan=4;
			m->ninst=15;
			opt|=2;
			break;
		case 0x31384443: m->nchan=8; break; /* CD81 */

		case 0x315A4454: m->nchan=1; break; /* TDZ1 */
		case 0x325A4454: m->nchan=2; break;
		case 0x335A4454: m->nchan=3; break;
		case 0x345A4454: m->nchan=4; break;
		case 0x355A4454: m->nchan=5; break;
		case 0x365A4454: m->nchan=6; break;
		case 0x375A4454: m->nchan=7; break;
		case 0x385A4454: m->nchan=8; break;
		case 0x395A4454: m->nchan=9; break;

		case 0x4E484331: m->nchan=1; break; /* 1CHN... */
		case 0x4E484332: m->nchan=2; break;
		case 0x4E484333: m->nchan=3; break;
		case 0x4E484334: m->nchan=4; break;
		case 0x4E484335: m->nchan=5; break;
		case 0x4E484336: m->nchan=6; break;
		case 0x4E484337: m->nchan=7; break;
		case 0x4E484338: m->nchan=8; break;
		case 0x4E484339: m->nchan=9; break;
		case 0x48433031: m->nchan=10; break; /* 10CH... */
		case 0x48433131: m->nchan=11; break;
		case 0x48433231: m->nchan=12; break;
		case 0x48433331: m->nchan=13; break;
		case 0x48433431: m->nchan=14; break;
		case 0x48433531: m->nchan=15; break;
		case 0x48433631: m->nchan=16; break;
		case 0x48433731: m->nchan=17; break;
		case 0x48433831: m->nchan=18; break;
		case 0x48433931: m->nchan=19; break;
		case 0x48433032: m->nchan=20; break;
		case 0x48433132: m->nchan=21; break;
		case 0x48433232: m->nchan=22; break;
		case 0x48433332: m->nchan=23; break;
		case 0x48433432: m->nchan=24; break;
		case 0x48433532: m->nchan=25; break;
		case 0x48433632: m->nchan=26; break;
		case 0x48433732: m->nchan=27; break;
		case 0x48433832: m->nchan=28; break;
		case 0x48433932: m->nchan=29; break;
		case 0x48433033: m->nchan=30; break;
		case 0x48433133: m->nchan=31; break;
		case 0x48433233: m->nchan=32; break;
		case 0x38544C46: /* FLT8 */
				 return errFormSupp;
				 /*    m->nchan=8; */
				 /*    break; */
		default:
				 if (sig==1)
					 return errFormSig;
				 m->ninst=(sig==2)?31:15;
				 opt|=2;
				 break;
	}

	if (chan)
		m->nchan=chan;

	if (!m->nchan)
		return errFormSig;

#ifdef XM_LOAD_DEBUG
	fprintf(stderr, __FILE__ ": channels=%d\n", m->nchan);
	fprintf(stderr, __FILE__ ": ninst=%d\n", m->ninst);
#endif

	m->nsampi=m->ninst;
	m->nsamp=m->ninst;
	m->instruments=malloc(sizeof(struct xmpinstrument)*m->ninst);
	m->samples=calloc(sizeof(struct xmpsample), m->ninst);
	m->sampleinfos=calloc(sizeof(struct sampleinfo), m->ninst);
	if (!m->instruments||!m->samples||!m->sampleinfos)
		return errAllocMem;

	fseek(file, 0, SEEK_SET);
	if (fread(m->name, 20, 1, file)!=1)
			fprintf(stderr, __FILE__ ": warning: fread() failed #2\n");

	m->name[20]=0;

	for (i=0; i<m->nchan; i++)
		m->panpos[i]=((i*3)&2)?0xFF:0x00;

#ifdef XM_LOAD_DEBUG
	fprintf(stderr, __FILE__ "\n");
	fprintf(stderr, __FILE__ ": LOADING INSTRUMENT INFORMATION\n");
#endif

	for (i=0; i<m->ninst; i++)
	{
		uint_fast32_t length, loopstart, looplength;
		struct __attribute__((packed)) {
			char name[22];
			uint16_t length;
			int8_t finetune;
			uint8_t volume;
			uint16_t loopstart;
			uint16_t looplength;
		} mi;
		struct xmpinstrument *ip=&m->instruments[i];
		struct xmpsample *sp=&m->samples[i];
		struct sampleinfo *sip=&m->sampleinfos[i];
		unsigned int j;

		if (fread(&mi, sizeof(mi), 1, file)!=1)
			fprintf(stderr, __FILE__ ": warning: fread() failed #3\n");

		length=swapb2(mi.length);
		loopstart=swapb2(mi.loopstart);
		looplength=swapb2(mi.looplength);
#ifdef XM_LOAD_DEBUG
		fprintf(stderr, __FILE__ ": [%d]\n", i);
		fprintf(stderr, __FILE__ ": name: %s\n", mi.name);
		fprintf(stderr, __FILE__ ": length: %d, finetune: %d, volume: %d, loopstart: %d, looplength: %d\n", (int)mi.length, (int)mi.finetune, (int)mi.volume, (int)mi.loopstart, (int)mi.looplength);
#endif
		if (length<4)
			length=0;
		if (looplength<4)
			looplength=0;
		if (!looplength||(loopstart>=length))
			looplength=0;
		else
			if ((loopstart+looplength)>length)
				looplength=length-loopstart;
		if (mi.finetune&0x08)
			mi.finetune|=0xF0;

		memcpy(ip->name, mi.name, 22);
		for (j=21; (signed)j>=0; j--)
			if (((unsigned)(ip->name[j]))>=0x20) /* on gnu c, char is signed */
				break;
		ip->name[j+1]=0;
		memset(ip->samples, -1, 256);
		*sp->name=0;
		sp->handle=0xFFFF;
		sp->stdpan=-1;
		sp->opt=0;
		sp->normnote=-mi.finetune*32;
		sp->normtrans=0;
		sp->stdvol=(mi.volume>0x3F)?0xFF:(mi.volume<<2);
		sp->volenv=0xFFFF;
		sp->panenv=0xFFFF;
		sp->pchenv=0xFFFF;
		sp->volfade=0;
		sp->vibspeed=0;
		sp->vibrate=0;
		for (j=0; j<128; j++)
			ip->samples[j]=i;
		if (!length)
			continue;

		sp->handle=i; /* this used to be above the !length check */

		sip->length=length;
		sip->loopstart=loopstart;
		sip->loopend=loopstart+looplength;
		sip->samprate=8363;
		sip->type=looplength?mcpSampLoop:0;
	}


	if (fread(&ordn, 1, 1, file)!=1)
		fprintf(stderr, "xmlmod.c: warning: fread() failed #4\n");
	if (!ordn)
	{
		fprintf (stderr, "xmlmod.c: error, order count == 0\n");
		return errFormSig;
	}
	if (fread(&loopp, 1, 1, file)!=1)
		fprintf(stderr, "xmlmod.c: warning: fread() failed #5\n");
	if (fread(orders, 128, 1, file)!=1)
		fprintf(stderr, "xmlmod.c: warning: fread() failed #6\n");

#ifdef XM_LOAD_DEBUG
	fprintf(stderr, __FILE__ ": \n");
	fprintf(stderr, __FILE__ ": LOADING ORDER DATA\n");
	fprintf(stderr, __FILE__ ": ordn: %d\n", (int)ordn);
	fprintf(stderr, __FILE__ ": loopp: %d\n", (int)loopp);
	{
		int i;
		fprintf(stderr, "orders: {");
		for (i=0;i<128;i++)
			fprintf(stderr, "%s%d", i?", ":"", orders[i]);
		fprintf(stderr, "}\n");
	}
#endif

	if (loopp>=ordn)
		loopp=0;

	for (t=0; t<ordn; t++)
		if (pn<orders[t])
			pn=orders[t];

	{ /* scan rest of orders aswell, but be a bit more carefull */
		int scan = 1;
		for (t=0; t<ordn; t++)
			if (orders[t]>=0x80)
				scan=0;
		if (scan)
			for (t=0; t<128; t++)
				if (pn<orders[t])
					pn=orders[t];
	}


#ifdef XM_LOAD_DEBUG
	fprintf(stderr, __FILE__ ": Highest pattern found: %d\n", pn);
#endif
	pn++;

	m->nord=ordn;
	m->loopord=loopp;

	m->npat=pn;

	m->initempo=6;
	m->inibpm=125;

#ifdef XM_LOAD_DEBUG
	fprintf(stderr, __FILE__ ": current file offset: %d\n", (int)ftell(file));
	fprintf(stderr, __FILE__ ": skip signature? %d\n", sig);
#endif
	if (sig)
		fseek(file, 4, SEEK_CUR);

	m->orders=malloc(sizeof(uint16_t)*m->nord);
	m->patlens=malloc(sizeof(uint16_t)*m->npat);
	m->patterns=(uint8_t (**)[5])calloc(sizeof(void *), m->npat);
	temppat=malloc(sizeof(uint8_t)*4*64*m->nchan);
	if (!m->orders||!m->patlens||!m->patterns||!temppat)
		return errAllocMem;

	for (i=0; i<m->nord; i++)
		m->orders[i]=orders[i];

/*
	memset(m->patterns, 0, sizeof(*m->patterns)*m->npat);*/

	for (i=0; i<m->npat; i++)
	{
		m->patlens[i]=64;
		m->patterns[i]=malloc(sizeof(uint8_t)*64*m->nchan*5);
		if (!m->patterns[i])
		{
			free(temppat);
			return errAllocMem;
		}
	}

	for (i=0; i<pn; i++)
	{
		uint8_t *dp=(uint8_t *)(m->patterns[i]);
		uint8_t *sp=temppat;
		unsigned int j;

		if (fread(temppat, 256*m->nchan, 1, file)!=1)
			fprintf(stderr, "xmlmod.c: warning: fread() failed #7\n");

		for (j=0; j<(64*m->nchan); j++)
		{
			uint16_t nvalue=((uint16_t)(sp[0]&0xF)<<8)+sp[1];
			dp[0]=0;
			if (nvalue)
			{
				int k;
				for (k=0; k<85; k++)
					if (modnotetab[k]<=nvalue)
						break;
				dp[0]=k+13;
			}
			dp[1]=(sp[2]>>4)|(sp[0]&0x10);
			dp[2]=0;
			dp[3]=sp[2]&0xF;
			dp[4]=sp[3];

			if (dp[3]==0xE)
			{
				dp[3]=36+(dp[4]>>4);
				dp[4]&=0xF;
			}
			if (opt&1 && dp[3]==0x08) /* DMP panning */
			{
				uint8_t pan=dp[4];
				if (pan==164)
					pan=0xC0;
				if (pan>0x80)
					pan=0x100-pan;
				pan=(pan==0x80)?0xFF:(pan<<1);
				dp[4]=pan;
			}
			if (opt&2 && dp[3]==0x0f) /* old "set tempo" */
			{
				dp[3]=xmpCmdMODtTempo;
			}


			if ((opt&8) && (dp[3]==15) && (dp[4]==0)) /* barbarian.mod and some few others has some broken "end" commands here and there */
				dp[3]=0;
/*
			fprintf(stderr, "dp[3]=%d dp[4]=%d\n", dp[3], dp[4]); */

			sp+=4;
			dp+=5;
		}
	}
	free(temppat);
#ifdef XM_LOAD_DEBUG
	fprintf(stderr, __FILE__ "\n");
	fprintf(stderr, __FILE__ ": LOADING SAMPLE DATA\n");
#endif
	for (i=0; i<m->ninst; i++)
	{
/*
		struct xmpinstrument *ip=&m->instruments[i];*/
		struct xmpsample *sp=&m->samples[i];
		struct sampleinfo *sip=&m->sampleinfos[i];
		size_t result;
#ifdef XM_LOAD_DEBUG
		fprintf(stderr, __FILE__ ": [%d]\n", i);
		fprintf(stderr, __FILE__ ": sp->handle=%04x (0xffff implies not to load data)\n", sp->handle);
#endif
		if (sp->handle==0xFFFF) /* TODO, can this EVER occure??.. It can now, but if it behaves correct is unknown */
			continue;
		sip->ptr=calloc(sizeof(uint8_t)*sip->length+8, 1);
#ifdef XM_LOAD_DEBUG
		fprintf(stderr, __FILE__ ": sip->ptr=%p\n", sip->ptr);
#endif
		if (!sip->ptr)
			return errAllocMem;
		if ((result=fread(sip->ptr, 1, sip->length, file))!=sip->length)
			fprintf(stderr, "xmlmod.c: warning: fread() failed #8 (%d of %d)\n", (int)result, (unsigned int)sip->length);
		sp->handle=i;
	}

	return errOk;
}

int __attribute__ ((visibility ("internal"))) xmpLoadMOD(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 0, 1, 8);
}

int __attribute__ ((visibility ("internal"))) xmpLoadMODt(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 0, 1, 2);
}

int __attribute__ ((visibility ("internal"))) xmpLoadMODd(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 0, 1, 1);
}

int __attribute__ ((visibility ("internal"))) xmpLoadM31(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 4, 2, 0);
}

int __attribute__ ((visibility ("internal"))) xmpLoadM15(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 4, 0, 0);
}

int __attribute__ ((visibility ("internal"))) xmpLoadM15t(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 4, 0, 2);
}

int __attribute__ ((visibility ("internal"))) xmpLoadWOW(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 8, 1, 0);
}

int __attribute__ ((visibility ("internal"))) xmpLoadMODf(struct xmodule *m, FILE *file)
{
	return loadmod(m, file, 0, 1, 4);
}
