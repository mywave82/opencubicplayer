/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Player system variables / auxiliary routines
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
#include "types.h"
#include "mchasm.h"
#include "player.h"
#include "stuff/imsrtns.h"

struct ocpfilehandle_t;

unsigned int plrRate;
int plrOpt;
int (*plrPlay)(void **buf, unsigned int *len, struct ocpfilehandle_t *source_file);
void (*plrStop)(void);
void (*plrSetOptions)(uint32_t rate, int opt);
int (*plrGetBufPos)(void);
int (*plrGetPlayPos)(void);
void (*plrAdvanceTo)(unsigned int pos);
uint32_t (*plrGetTimer)(void);
void (*plrIdle)(void);
#ifdef PLR_DEBUG
char *(*plrDebug)(void)=0;
#endif

static int stereo;
static int bit16;
static int reversestereo;
static int signedout;
static uint32_t samprate;

static uint8_t *plrbuf;
static unsigned long buflen;


void plrGetRealMasterVolume(int *l, int *r)
{
	uint32_t len=samprate/20;
	int32_t p;
	int32_t pass2;
	mixAddAbsfn fn;
	unsigned long v;

	if (len>buflen)
		len=buflen;
	p=plrGetPlayPos()>>(stereo+bit16);

	pass2=len-(uint32_t)buflen+p;

	if (stereo)
	{
		fn=bit16?(signedout?mixAddAbs16SS:mixAddAbs16S):(signedout?mixAddAbs8SS:mixAddAbs8S);

		if (pass2>0)
			v=fn(plrbuf+(p<<(1+bit16)), len-pass2)+fn(plrbuf, pass2);
		else
			v=fn(plrbuf+(p<<(1+bit16)), len);

		v=v*128/(len*16384);
		*l=(v>255)?255:v;

		if (pass2>0)
			v=fn(plrbuf+(p<<(1+bit16))+(1<<bit16), len-pass2)+fn(plrbuf+(1<<bit16), pass2);
		else
			v=fn(plrbuf+(p<<(1+bit16))+(1<<bit16), len);
		v=v*128/(len*16384);
		*r=(v>255)?255:v;
	} else {
		fn=bit16?(signedout?mixAddAbs16MS:mixAddAbs16M):(signedout?mixAddAbs8MS:mixAddAbs8M);

		if (pass2>0)
			v=fn(plrbuf+(p<<bit16), len-pass2)+fn(plrbuf, pass2);
		else
			v=fn(plrbuf+(p<<bit16), len);
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

void plrGetMasterSample(int16_t *buf, uint32_t len, uint32_t rate, int opt)
{
	uint32_t step=umuldiv(samprate, 0x10000, rate);
	unsigned int maxlen;
	int stereoout;
	uint32_t bp;
	int32_t pass2;
	mixGetMasterSamplefn fn;

	if (step<0x1000)
		step=0x1000;
	if (step>0x800000)
		step=0x800000;

	maxlen=imuldiv(buflen, 0x10000, step);
	stereoout=(opt&plrGetSampleStereo)?1:0;
	if (len>maxlen)
	{
		memset(buf+(maxlen<<stereoout), 0, (len-maxlen)<<(1+stereoout));
		len=maxlen;
	}

	bp=plrGetPlayPos()>>(stereo+bit16);

	pass2=len-imuldiv((uint32_t)buflen-bp,0x10000,step);

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
		else
			if (reversestereo)
				fn=signedout?mixGetMasterSampleSS8SR:mixGetMasterSampleSU8SR;
			else
				fn=signedout?mixGetMasterSampleSS8S:mixGetMasterSampleSU8S;
	else if (!stereoout)
		fn=signedout?mixGetMasterSampleMS8M:mixGetMasterSampleMU8M;
	else
		fn=signedout?mixGetMasterSampleMS8S:mixGetMasterSampleMU8S;

	if (pass2>0)
	{
		fn(buf, plrbuf+(bp<<(stereo+bit16)), len-pass2, step);
		fn(buf+((len-pass2)<<stereoout), plrbuf, pass2, step);
	} else
		fn(buf, plrbuf+(bp<<(stereo+bit16)), len, step);
}


int plrOpenPlayer(void **buf, uint32_t *len, uint32_t bufl, struct ocpfilehandle_t *source_file)
{
	unsigned int dmalen;

	if (!plrPlay)
		return 0;

	dmalen=umuldiv(plrRate<<(!!(plrOpt&PLR_STEREO)+!!(plrOpt&PLR_16BIT)), bufl, 32500)&~15;

	plrbuf=0;
	if (!plrPlay((void **)((void *)&plrbuf), &dmalen, source_file)) /* to remove warning :-) */
		return 0;

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	samprate=plrRate;

	buflen=dmalen>>(stereo+bit16);
	*buf=plrbuf;
	*len=buflen;

	return 1;
}

void plrClosePlayer(void)
{
	plrStop();
}
