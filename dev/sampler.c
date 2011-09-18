/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * sampler system variables / auxiliary routines
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
 *  -ss040907   Stian Skjelstad <stian@nixia.no>
 *    -minor buffer cleanups.. Let drivers allocate their own memory
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "sampler.h"
#include "stuff/imsrtns.h"
#include "mchasm.h"

int smpRate;
int smpOpt;
int smpBufSize;
int (*smpSample)(unsigned char **buf, int *len);
void (*smpStop)();
void (*smpSetOptions)(int rate, int opt);
void (*smpSetSource)(int src);
int (*smpGetBufPos)(void);

static int stereo;
static int bit16;
static int reversestereo;
static int signedout;
static uint32_t samprate;

/*static __segment dmabufsel;*/
static unsigned char *smpbuf;
static unsigned long buflen;

void smpGetRealMasterVolume(int *l, int *r)
{
	uint16_t len=samprate/20;
	uint16_t p;
	int32_t pass2;
/*
	unsigned long (*fn)(const void *ch, unsigned long len);*/
	mixAddAbsfn fn;
	uint32_t v;

	if (len>buflen)
		len=buflen;
	p=((smpGetBufPos()>>(stereo+bit16))+buflen-len)%buflen;
	pass2=len-buflen+p;

	if (stereo)
	{
		fn=bit16?(signedout?mixAddAbs16SS:mixAddAbs16S):(signedout?mixAddAbs8SS:mixAddAbs8S);

		if (pass2>0)
			v=fn(smpbuf+(p<<(1+bit16)), len-pass2)+fn(smpbuf, pass2);
		else
			v=fn(smpbuf+(p<<(1+bit16)), len);
		v=v*128/(len*16384);
		*l=(v>255)?255:v;

	if (pass2>0)
		v=fn(smpbuf+(p<<(1+bit16))+(1<<bit16), len-pass2)+fn(smpbuf+(1<<bit16), pass2);
	else
		v=fn(smpbuf+(p<<(1+bit16))+(1<<bit16), len);
	v=v*128/(len*16384);
	*r=(v>255)?255:v;
	} else {
		fn=bit16?(signedout?mixAddAbs16MS:mixAddAbs16M):(signedout?mixAddAbs8MS:mixAddAbs8M);

		if (pass2>0)
			v=fn(smpbuf+(p<<bit16), len-pass2)+fn(smpbuf, pass2);
		else
			v=fn(smpbuf+(p<<bit16), len);
		v=v*128/(len*16384);
		*r=*l=(v>255)?255:v;
	}
	if (reversestereo)
	{
		int t=*r;
		*r=*l;
		*l=t;
	}
}

void smpGetMasterSample(int16_t *buf, unsigned int len, uint32_t rate, int opt)
{
	uint32_t step=umuldiv(samprate, 0x10000, rate);
	uint32_t maxlen;
	int stereoout;
	uint32_t bp;
	int32_t pass2;
/*
	void (*fn)(short *buf2, const void *buf, int len, long step);*/
	mixGetMasterSamplefn fn;

	if (step<0x1000)
		step=0x1000;
	if (step>0x800000)
		step=0x800000;

	maxlen=umuldiv(buflen, 0x10000, step);
	stereoout=(opt&smpGetSampleStereo)?1:0;
	if (len>maxlen)
	{
		memset((int32_t *)buf+(maxlen<<stereoout), 0, (len-maxlen)<<(1+stereoout));
		len=maxlen;
	}

	bp=((smpGetBufPos()>>(stereo+bit16))+buflen-imuldiv(len,step,0x10000))%buflen;
	pass2=len-imuldiv(buflen-bp,0x10000,step);
	if (bit16)
		if (stereo)
			if (!stereoout)
				fn=signedout?mixGetMasterSampleSS16M:mixGetMasterSampleSU16M;
			else if (reversestereo)
				fn=signedout?mixGetMasterSampleSS16SR:mixGetMasterSampleSU16SR;
		else
			fn=signedout?mixGetMasterSampleSS16S:mixGetMasterSampleSU16S;
		else if (!stereoout)
			fn=signedout?mixGetMasterSampleMS16M:mixGetMasterSampleMU16M;
		else
			fn=signedout?mixGetMasterSampleMS16S:mixGetMasterSampleMU16S;
	else if (stereo)
		if (!stereoout)
			fn=signedout?mixGetMasterSampleSS8M:mixGetMasterSampleSU8M;
		else if (reversestereo)
			fn=signedout?mixGetMasterSampleSS8SR:mixGetMasterSampleSU8SR;
		else
			fn=signedout?mixGetMasterSampleSS8S:mixGetMasterSampleSU8S;
	else if (!stereoout)
		fn=signedout?mixGetMasterSampleMS8M:mixGetMasterSampleMU8M;
	else
		fn=signedout?mixGetMasterSampleMS8S:mixGetMasterSampleMU8S;

	if (pass2>0)
	{
		fn(buf, smpbuf+(bp<<(stereo+bit16)), len-pass2, step);
		fn(buf+((len-pass2)<<stereoout), smpbuf, pass2, step);
	} else
		fn(buf, smpbuf+(bp<<(stereo+bit16)), len, step);
}


int smpOpenSampler(void **buf, int *len, int bufl)
{
	int dmalen;

	if (!smpSample)
		return 0;

	dmalen=umuldiv(smpRate<<(!!(smpOpt&SMP_STEREO)+!!(smpOpt&SMP_16BIT)), bufl, 65536)&~15;

	smpbuf=0;
	if (!smpSample(&smpbuf, &dmalen))
		return 0;

	stereo=!!(smpOpt&SMP_STEREO);
	bit16=!!(smpOpt&SMP_16BIT);
	reversestereo=!!(smpOpt&SMP_REVERSESTEREO);
	signedout=!!(smpOpt&SMP_SIGNEDOUT);
	samprate=smpRate;

	buflen=dmalen>>(stereo+bit16);

	*buf=smpbuf;
	*len=buflen;

	return 1;
}

void smpCloseSampler(void)
{
	smpStop();
/*
	dmaFree(dmabufsel);*/
}
