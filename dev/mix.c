/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Display mixer routines/vars/defs
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include "types.h"
#include "mcp.h"
#include "cpiface/cpiface.h"
#include "stuff/imsrtns.h"
#include "mix.h"
#include "mixasm.h"

#define MIXBUFLEN 2048

static void (*mixGetMixChannel)(unsigned int ch, struct mixchannel *chn, uint32_t rate);

static struct mixchannel *channels;

static int channum;
static uint32_t (*voltabs)[2][256];
static int16_t (*amptab)[256];
static int32_t clipmax;
static int32_t *mixbuf;
static uint32_t amplify;


static void mixCalcIntrpolTab(void)
	/* Used by mixInit
	 */
{
	int i,j;
	for (i=0; i<16; i++)
		for (j=0; j<256; j++)
		{
			mixIntrpolTab[i][j][1]=(i*(signed char)j)>>4;
			mixIntrpolTab[i][j][0]=j-mixIntrpolTab[i][j][1];
		}
	for (i=0; i<32; i++)
		for (j=0; j<256; j++)
		{
			mixIntrpolTab2[i][j][1]=(i*(signed char)j)<<3;
			mixIntrpolTab2[i][j][0]=((signed char)j<<8)-mixIntrpolTab2[i][j][1];
		}
}

static void calcamptab(int32_t amp)
	/* Used by mixSetAmplify
	 *         mixInit
	 */
{
	int i;

	if (!amptab)
		return;
	amp>>=4;

	for (i=0; i<256; i++)
	{
		amptab[0][i]=(amp*i)>>12;
		amptab[1][i]=(amp*i)>>4;
		amptab[2][i]=(amp*(signed char)i)<<4;
	}

	clipmax=amp?(0x07FFF000/amp):0x7FFFFFFF;
}

static void mixgetmixch(int ch, struct mixchannel *chn, int rate)
{
	mixGetMixChannel(ch, chn, rate);

	if (!(chn->status&MIX_PLAYING))
		return;
	if (chn->pos>=chn->length)
	{
		chn->status&=~MIX_PLAYING;
		return;
	}
	if (chn->status&MIX_PLAY16BIT)
		chn->samp=(void*)((unsigned long)chn->samp>>1);
	if (chn->status&MIX_PLAYFLOAT)
		chn->samp=(void*)((unsigned long)chn->samp>>2);
	chn->replen=(chn->status&MIX_LOOPED)?(chn->loopend-chn->loopstart):0;
}

static void putchn(struct mixchannel *chn, unsigned int len, int opt)
{
	if (!(chn->status&MIX_PLAYING)||(chn->status&MIX_MUTE))
		return;
	if (opt&mcpGetSampleHQ)
		chn->status|=MIX_MAX|MIX_INTERPOLATE;
	if (!(chn->status&MIX_PLAYFLOAT))
	{
		int voll=chn->vol.vols[0];
		int volr=chn->vol.vols[1];
		if (!(opt&mcpGetSampleStereo))
		{
			voll=(voll+volr)>>1;
			volr=0;
		}
		if (voll<0)
			voll=0;
		if (voll>64)
			voll=64;
		if (volr<0)
			volr=0;
		if (volr>64)
			volr=64;
		if (!voll&&!volr)
			return;
		chn->vol.voltabs[0]=(uint32_t *)voltabs[voll];
		chn->vol.voltabs[1]=(uint32_t *)voltabs[volr];
	}
	mixPlayChannel(mixbuf, len, chn, opt&mcpGetSampleStereo);
}

void mixGetMasterSample(int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	int stereo=(opt&mcpGetSampleStereo)?1:0;
	int i;
	for (i=0; i<channum; i++)
		mixgetmixch(i, &channels[i], rate);

	if (len>((unsigned)MIXBUFLEN>>stereo))
	{
		memset(s+MIXBUFLEN, 0, ((len<<stereo)-MIXBUFLEN)<<1);
		len=MIXBUFLEN>>stereo;
	}
	bzero (mixbuf, (len<<stereo)<<2);
	for (i=0; i<channum; i++)
		putchn(&channels[i], len, opt);
	mixClip(s, mixbuf, len<<stereo, amptab, clipmax);
}

static int mixMixChanSamples (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int *ch, unsigned int n, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	int stereo=(opt&mcpGetSampleStereo)?1:0;
	int ret;
	unsigned int i;

	if (!n)
	{
		memset(s, 0, len<<(1+stereo));
		return 0;
	}

	if (len>MIXBUFLEN)
	{
		memset(s+(MIXBUFLEN<<stereo), 0, ((len<<stereo)-MIXBUFLEN)<<1);
		len=MIXBUFLEN>>stereo;
	}
	ret=3;
	for (i=0; i<n; i++)
		mixgetmixch(ch[i], &channels[i], rate);
	bzero (mixbuf, (len<<stereo)<<2);
	for (i=0; i<n; i++)
	{
		if (!(channels[i].status&MIX_PLAYING))
			continue;
		ret&=~2;
		if (!(channels[i].status&MIX_MUTE))
			ret=0;
		channels[i].status&=~MIX_MUTE;

		putchn(&channels[i], len, opt);

	}
	len<<=stereo;

	for (i=0; i<len; i++)
		s[i]=mixbuf[i]>>8;
	return ret;
}

int mixGetChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	return mixMixChanSamples (cpifaceSession, &ch, 1, s, len, rate, opt);
}

void mixGetRealVolume(int ch, int *l, int *r)
{
	struct mixchannel chn;
	mixgetmixch(ch, &chn, 44100);
	chn.status&=~MIX_MUTE;
	if (chn.status&MIX_PLAYING)
	{
		uint32_t v=mixAddAbs(&chn, 256);
		uint32_t i;
		if (chn.status&MIX_PLAYFLOAT)
		{
			i=((int)(v*(chn.vol.volfs[0]*64.0)))>>16;
			*l=(i>255)?255:i;
			i=((int)(v*(chn.vol.volfs[1]*64.0)))>>16;
			*r=(i>255)?255:i;
		} else {
			i=(v*chn.vol.vols[0])>>16;
			*l=(i>255)?255:i;
			i=(v*chn.vol.vols[1])>>16;
			*r=(i>255)?255:i;
		}
	} else
		*l=*r=0;
}

void mixGetRealMasterVolume(int *l, int *r)
{
	int i;

	for (i=0; i<channum; i++)
		mixgetmixch(i, &channels[i], 44100);

	*l=*r=0;
	for (i=0; i<channum; i++)
	{
		uint32_t v;
		if ((channels[i].status&MIX_MUTE)||!(channels[i].status&MIX_PLAYING))
			continue;
		v=mixAddAbs(&channels[i], 256);
		(*l)+=(((v*channels[i].vol.vols[0])>>16)*amplify)>>18;
		(*r)+=(((v*channels[i].vol.vols[1])>>16)*amplify)>>18;
	}
	*l=(*l>255)?255:*l;
	*r=(*r>255)?255:*r;
}

void mixSetAmplify(int amp)
{
	amplify=amp*8;
	calcamptab((amplify*channum)>>11);
}

int mixInit(void (*getchan)(unsigned int ch, struct mixchannel *chn, uint32_t rate), int masterchan, unsigned int chn, int amp, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i,j;

	mixGetMixChannel=getchan;
	mixbuf=malloc(MIXBUFLEN*sizeof(int32_t));
	mixIntrpolTab=malloc(16*sizeof(*mixIntrpolTab));/*new signed char [16][256][2];*/
	mixIntrpolTab2=malloc(32*sizeof(*mixIntrpolTab2));/*new short [32][256][2];*/
	voltabs=malloc (65*sizeof(*voltabs));/*new long [65][2][256];*/
	channels=malloc(sizeof(struct mixchannel)*(16+chn)); /*   new mixchannel[chn+16]; */
	if (!mixbuf||!voltabs||!mixIntrpolTab2||!mixIntrpolTab||!channels)
		return 0;
	amptab=0;

	if (masterchan)
	{
		amptab=malloc(3*sizeof(*amptab)); /*new short [3][256];*/
		if (!amptab)
			return 0;
	}
	mixCalcIntrpolTab();
	amplify=amp*8;

	cpifaceSession->mcpGetRealVolume = mixGetRealVolume;
	cpifaceSession->mcpGetChanSample = mixGetChanSample;
	cpifaceSession->mcpMixChanSamples = mixMixChanSamples;
	if (masterchan)
	{ /* override devp */
		cpifaceSession->GetRealMasterVolume = mixGetRealMasterVolume;
		cpifaceSession->GetMasterSample = mixGetMasterSample;
	}

	channum=chn;
	for (i=0; i<=64; i++)
		for (j=0; j<256; j++)
		{
			voltabs[i][0][j]=(((i*0xFFFFFF/channum)>>6)*(signed char)j)>>8;
			voltabs[i][1][j]=(((i*0xFFFFFF/channum)>>14)*j)>>8;
		}
	calcamptab((amplify*channum)>>11);

	return 1;
}

void mixClose(void)
{
	free(channels);
	free(mixbuf);
	free(voltabs);
	free(amptab);
	free(mixIntrpolTab);
	free(mixIntrpolTab2);
}
