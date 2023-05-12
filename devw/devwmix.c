/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Wavetable Device: Software Mixer for sample stream output via devp
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
 *  -kb980525   Tammo Hinrichs <kb@nwn.de>
 *    - restructured volume calculations to avoid those nasty
 *      rounding errors
 *    - changed behaviour on loop change a bit (may cause problems with
 *      some .ULTs but fixes many .ITs instead ;)
 *    - extended volume table to 256 values, thus consuming more memory,
 *      but definitely increasing the output quality ;)
 *    - added _dllinfo record
 *  -ryg990504  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -fixed sum really stupid memory leak
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviwave.h"
#include "dev/mcp.h"
#include "dev/mix.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/postproc.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "dwmix.h"
#include "dwmixa.h"
#include "dwmixqa.h"

#define MIXBUFLEN 4096
#define MAXCHAN 255

static const struct mcpDriver_t mcpMixer;
static const struct mcpDriver_t mcpMixerQ;

static const struct mixAPI_t *mix;

#define MIX_MAX_POSTPROC 10
static const struct PostProcIntegerRegStruct *postproc[MIX_MAX_POSTPROC];
static int                                    postprocs;

static int quality;
static int resample;

static int _pause;
static long playsamps;

static struct sampleinfo *samples;
static int samplenum;

static int16_t (*amptab)[256]; /* signedness is not fixed here */
static unsigned long clipmax;
static volatile int clipbusy;
#define BARRIER __asm__ __volatile__ ("":);

static unsigned long amplify;
static unsigned short transform[2][2];
static int volopt;
static unsigned long relpitch;
static int interpolation;

static uint32_t samprate;

static int channelnum;
static struct channel *channels;
static int32_t fadedown[2];

static int32_t (*voltabsr)[256];
static int16_t (*voltabsq)[2][256];

static uint8_t (*interpoltabr)[256][2];
static int16_t (*interpoltabq)[32][256][2];
static int16_t (*interpoltabq2)[16][256][4];

static int16_t *scalebuf=0;
static int32_t *buf32;

static void (*playerproc)(struct cpifaceSessionAPI_t *cpifaceSession);
static unsigned long tickwidth;
static unsigned long tickplayed;
static unsigned long orgspeed;
static unsigned short relspeed;
static unsigned long newtickwidth;
static unsigned long cmdtimerpos;

static int mastervol;
static int masterbal;
static int masterpan;
static int mastersrnd;
static int masterrvb;
static int masterchr;

static uint32_t IdleCache; /* To prevent devpDisk lockup */

static int devwMixProcKey (uint16_t key);

static void calcinterpoltabr(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (i=0; i<16; i++)
		for (j=0; j<256; j++)
		{
			interpoltabr[i][j][1]=(i*(signed char)j)>>4;
			interpoltabr[i][j][0]=(signed char)j-interpoltabr[i][j][1];
		}
}

static void calcinterpoltabq(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (i=0; i<32; i++)
		for (j=0; j<256; j++)
		{
			interpoltabq[0][i][j][1]=(i*(signed char)j)<<3;
			interpoltabq[0][i][j][0]=(((signed char)j)<<8)-interpoltabq[0][i][j][1];
			interpoltabq[1][i][j][1]=(i*j)>>5;
			interpoltabq[1][i][j][0]=j-interpoltabq[1][i][j][1];
		}
	for (i=0; i<16; i++)
		for (j=0; j<256; j++)
		{
			interpoltabq2[0][i][j][0]=((16-i)*(16-i)*(signed char)j)>>1;
			interpoltabq2[0][i][j][2]=(i*i*(signed char)j)>>1;
			interpoltabq2[0][i][j][1]=(((signed char)j)<<8)-interpoltabq2[0][i][j][0]-interpoltabq2[0][i][j][2];
			interpoltabq2[1][i][j][0]=((16-i)*(16-i)*j)>>9;
			interpoltabq2[1][i][j][2]=(i*i*j)>>9;
			interpoltabq2[1][i][j][1]=j-interpoltabq2[1][i][j][0]-interpoltabq2[1][i][j][2];
		}
}

static void calcvoltabsr(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (i=0; i<=512; i++)
		for (j=0; j<256; j++)
			voltabsr[i][j]=(i-256)*(signed char)j;
}

static void calcvoltabsq(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (j=0; j<=512; j++)
	{
		long amp=j-256;
		for (i=0; i<256; i++)
		{
			int v=amp*(signed char)i;
			voltabsq[j][0][i]=(v==0x8000)?0x7FFF:v;
			voltabsq[j][1][i]=(amp*i)>>8;
		}
	}
}

static void calcamptab(signed long amp)
	/* Used by SET
	 *         OpenPlayer
	 */
{
	int i;

	clipbusy++;

	BARRIER

	amp=3*amp/16;

	for (i=0; i<256; i++)
	{
		amptab[0][i]=(amp*i)>>12;
		amptab[1][i]=(amp*i)>>4;
		amptab[2][i]=(amp*(signed char)i)<<4;
	}

	if(amp)
		clipmax=0x07FFF000/amp;
	else
		clipmax=0x07FFF000;

#if 0
	if (!signedout)
		for (i=0; i<256; i++)
			amptab[0][i]^=0x8000;
#endif

	BARRIER

	clipbusy--;
}

static void calcstep(struct channel *c)
	/* Used by calcsteps
	 *         SET
	 */
{
	if (!(c->status&MIXRQ_PLAYING))
		return;
	if (c->orgdiv)
		c->step=imuldiv(imuldiv((c->step>=0)?c->orgfrq:-c->orgfrq, c->orgrate, c->orgdiv)<<8, relpitch, samprate);
	else
		c->step=0;
	c->status&=~MIXRQ_INTERPOLATE;
	if (!quality)
	{
		if (interpolation>1)
			c->status|=MIXRQ_INTERPOLATE;
		if (interpolation==1)
			if (abs(c->step)<=(3<<15))
		c->status|=MIXRQ_INTERPOLATE;
	} else {
		if (interpolation>1)
			c->status|=MIXRQ_INTERPOLATEMAX|MIXRQ_INTERPOLATE;
		if (interpolation==1)
		{
			c->status|=MIXRQ_INTERPOLATE;
			c->status&=~MIXRQ_INTERPOLATEMAX;
		}
	}
}

static void calcsteps(void)
	/* Used by SET */
{
	int i;
	for (i=0; i<channelnum; i++)
		calcstep(&channels[i]);
}

static void calcspeed(void)
{
	/* Used by SET
	 *         OpenPlayer
	 */
	if (channelnum)
		newtickwidth=imuldiv(256*256*256, samprate, orgspeed*relspeed);
}

static void transformvol(struct channel *ch)
	/* Used by calcvol
	 *         calcvols
	 *         SET
	 */
{
	int32_t v;

	v=transform[0][0]*ch->orgvol[0]+transform[0][1]*ch->orgvol[1];
	ch->vol[0]=(v>0x10000)?256:(v<-0x10000)?-256:((v+192)>>8);

	v=transform[1][0]*ch->orgvol[0]+transform[1][1]*ch->orgvol[1];
	if (volopt^ch->volopt)
		v=-v;
	ch->vol[1]=(v>0x10000)?256:(v<-0x10000)?-256:((v+192)>>8);

	if (ch->status&MIXRQ_MUTE)
	{
		ch->dstvols[0]=ch->dstvols[1]=0;
		return;
	}
	ch->dstvols[0]=ch->vol[0];
	ch->dstvols[1]=ch->vol[1];
}

static void calcvol(struct channel *chn)
	/* Used by calcvols
	 *         SET
	 */
{
	chn->orgvol[1]=((int32_t)chn->orgvolx*(0x80L+chn->orgpan))>>8;
	chn->orgvol[0]=((int32_t)chn->orgvolx*(0x80L-chn->orgpan))>>8;
	/* werte: 0-0x100; */
	transformvol(chn);
}


static void calcvols(void)
	/* Used by SET
	 *         OpenPlayer
	 */
{
	int16_t vols[2][2];
	int i;

	vols[0][0]=vols[1][1]=(mastervol*(0x40+masterpan))>>6;
	vols[0][1]=vols[1][0]=(mastervol*(0x40-masterpan))>>6;
	/* werte: 0-0x100 */

	if (masterbal>0)
	{
		vols[0][0]=(vols[0][0]*(0x40-masterbal))>>6;
		vols[0][1]=(vols[0][1]*(0x40-masterbal))>>6;
	} else if (masterbal<0)
	{
		vols[1][0]=(vols[1][0]*(0x40+masterbal))>>6;
		vols[1][1]=(vols[1][1]*(0x40+masterbal))>>6;
	}

	volopt=mastersrnd;
	transform[0][0]=vols[0][0];
	transform[0][1]=vols[0][1];
	transform[1][0]=vols[1][0];
	transform[1][1]=vols[1][1];
	for (i=0; i<channelnum; i++)
		transformvol(&channels[i]);
}

static void fadechanq(int *fade, struct channel *c)
	/* Used by stopchan
	 *         playchannelq
	 */
{
	int s;
	if (c->status&MIXRQ_PLAY16BIT)
		s=c->realsamp.bit16[c->pos];
	else
		s=(c->realsamp.bit8[c->pos])<<8;
	fade[0]+=(c->curvols[0]*s)>>8;
	fade[1]+=(c->curvols[1]*s)>>8;
	c->curvols[0]=c->curvols[1]=0;
}

static void stopchan(struct channel *c)
{
	/* Used by SET
	 */
	if (!(c->status&MIXRQ_PLAYING))
		return;
	if (!quality)
		mixrFadeChannel(fadedown, c);
	else
		fadechanq(fadedown, c);
	c->status&=~MIXRQ_PLAYING;
}

static void amplifyfadeq(uint32_t pos, uint32_t cl, int32_t *curvol, int32_t dstvol)
	/* Used by playchannelq
	 */
{
	uint32_t l=abs(dstvol-*curvol);

	if (l>cl)
		l=cl;
	if (dstvol<*curvol)
	{
		mixqAmplifyChannelDown(buf32+pos, scalebuf, l, *curvol, 4 << 1 /* stereo */);
		*curvol-=l;
	} else if (dstvol>*curvol)
	{
		mixqAmplifyChannelUp(buf32+pos, scalebuf, l, *curvol,  4 << 1 /* stereo */);
		*curvol+=l;
	}
	cl-=l;
	if (*curvol&&cl)
		mixqAmplifyChannel(buf32+pos+(l << 1 /* stereo */ ), scalebuf+l, cl, *curvol, 4 << 1 /* stereo */);
}

static void playchannelq(int ch, uint32_t len)
{
	/* Used by mixer
	 */
	struct channel *c=&channels[ch];
	if (c->status&MIXRQ_PLAYING)
	{
		int quiet=!c->curvols[0]&&!c->curvols[1]&&!c->dstvols[0]&&!c->dstvols[1];
		mixqPlayChannel(scalebuf, len, c, quiet);
		if (quiet)
			return;

		amplifyfadeq(0, len, &c->curvols[0], c->dstvols[0]);
		amplifyfadeq(1, len, &c->curvols[1], c->dstvols[1]);

		if (!(c->status&MIXRQ_PLAYING))
			fadechanq(fadedown, c);
	}
}

static void devwMixIdle  (struct cpifaceSessionAPI_t *cpifaceSession)
{
	/* mixer */
	int i;

	if (!channelnum)
		return;

	BARRIER

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	BARRIER

	if (_pause)
	{
		cpifaceSession->plrDevAPI->Pause (1);

	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		cpifaceSession->plrDevAPI->Pause (0);

		cpifaceSession->plrDevAPI->GetBuffer (&targetbuf, &targetlength);

		while (targetlength)
		{
			if (targetlength > MIXBUFLEN)
			{
				targetlength = MIXBUFLEN;
			}
			if (targetlength > ((tickwidth-tickplayed)>>8))
			{
				targetlength=(tickwidth-tickplayed)>>8;
			}

			mixrFade(buf32, fadedown, targetlength);
			if (!quality)
			{
				for (i=0; i<channelnum; i++)
					mixrPlayChannel(buf32, fadedown, targetlength, &channels[i]);
			} else {
				for (i=0; i<channelnum; i++)
					playchannelq(i, targetlength);
			}

			for (i=0; i < postprocs; i++)
			{
				postproc[i]->Process (cpifaceSession, buf32, targetlength, samprate);
			}

			mixrClip((char*)targetbuf, buf32, targetlength << 1 /* stereo */, amptab, clipmax);

			tickplayed+=targetlength<<8;
			if (!((tickwidth-tickplayed)>>8))
			{
				tickplayed-=tickwidth;
				playerproc (cpifaceSession);
#warning use plrDevAPI API to track this by buffer instead of delivery (cmdtimerpos)
				cmdtimerpos+=tickwidth;
				tickwidth=newtickwidth;
			}

			playsamps+=targetlength;

			cpifaceSession->plrDevAPI->CommitBuffer (targetlength);

			cpifaceSession->plrDevAPI->GetBuffer (&targetbuf, &targetlength);

			if (_pause)
			{
				break;
			}
		}
	}

	cpifaceSession->plrDevAPI->Idle();

	BARRIER

	clipbusy--;
}

static void devwMixSET (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	/* Refered by OpenPlayer
	 */
	struct channel *chn;

	if (ch>=channelnum)
		ch=channelnum-1;
	if (ch<0)
		ch=0;
	chn=&channels[ch];
	switch (opt)
	{
		case mcpCReset:
			{
				int reswasmute;
				stopchan(chn);
				reswasmute=chn->status&MIXRQ_MUTE;
				memset(chn, 0, sizeof(struct channel));
				chn->status=reswasmute;
				break;
			}
		case mcpCInstrument:
			stopchan(chn);
			if ((val<0)||(val>=samplenum))
				break;
			{
				struct sampleinfo *samp=&samples[val];
				chn->samptype=samp->type;
				chn->length=samp->length;
				chn->orgrate=samp->samprate;
				chn->samp=samp->ptr;
				chn->realsamp.bit8=chn->samp;
				chn->orgloopstart=samp->loopstart;
				chn->orgloopend=samp->loopend;
				chn->orgsloopstart=samp->sloopstart;
				chn->orgsloopend=samp->sloopend;

				chn->status&=~(MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP|MIXRQ_PLAY16BIT|MIXRQ_PLAYSTEREO);
			}
			if (chn->samptype&mcpSamp16Bit)
			{
				chn->status|=MIXRQ_PLAY16BIT;
			}
			if (chn->samptype&mcpSampStereo)
			{
				chn->status|=MIXRQ_PLAYSTEREO;
			}
			if (chn->samptype&mcpSampSLoop)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgsloopstart;
				chn->loopend=chn->orgsloopend;
				if (chn->samptype&mcpSampSBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			} else if (chn->samptype&mcpSampLoop)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgloopstart;
				chn->loopend=chn->orgloopend;
				if (chn->samptype&mcpSampBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			}
			chn->replen=(chn->status&MIXRQ_LOOPED)?(chn->loopend-chn->loopstart):0;
			chn->step=0;
			chn->pos=0;
			chn->fpos=0;
			break;
		case mcpCStatus:
			if (!val)
				stopchan(chn);
			else {
				if (chn->pos>=chn->length)
					break;
				chn->status|=MIXRQ_PLAYING;
				calcstep(chn);
			}
			break;
		case mcpCMute:
			if (val)
				chn->status|=MIXRQ_MUTE;
			else
				chn->status&=~MIXRQ_MUTE;
			transformvol(chn);
				break;
		case mcpCVolume:
			chn->orgvolx=(val>0x100)?0x100:(val<0)?0:val;
			calcvol(chn);
			break;
		case mcpCPanning:
			chn->orgpan=(val>0x80)?0x80:(val<-0x80)?-0x80:val;
			calcvol(chn);
			break;
		case mcpCSurround:
			chn->volopt=val?1:0;
			transformvol(chn);
			break;
		case mcpCDirect:
			if (val==0)
				chn->step=abs(chn->step);
			else
				if (val==1)
					chn->step=-abs(chn->step);
				else
					chn->step=-chn->step;
			break;
		case mcpCLoop:
			chn->status&=~(MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
			if ((val==1)&&!(chn->samptype&mcpSampSLoop))
				val=2;
			if ((val==2)&&!(chn->samptype&mcpSampLoop))
				val=0;
			if (val==1)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgsloopstart;
				chn->loopend=chn->orgsloopend;
				if (chn->samptype&mcpSampSBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			}
			if (val==2)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgloopstart;
				chn->loopend=chn->orgloopend;
				if (chn->samptype&mcpSampBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			}
			chn->replen=(chn->status&MIXRQ_LOOPED)?(chn->loopend-chn->loopstart):0;
			if (chn->replen)
			{
				if (((chn->pos<chn->loopstart)&&(chn->step<0))||((chn->pos>=chn->loopend)&&(chn->step>0)))
					chn->step=-chn->step;
			} else if (chn->step<0)
				chn->step=-chn->step;
			break;

		case mcpCPosition:
			{
				int poswasplaying;
				poswasplaying=chn->status&MIXRQ_PLAYING;
				stopchan(chn);
				if (val<0)
					val=0;
				if ((unsigned)val>=chn->length)
				val=chn->length-1;
				chn->pos=val;
				chn->fpos=0;
				chn->status|=poswasplaying;
				break;
			}
		case mcpCPitch:
			chn->orgfrq=8363;
			chn->orgdiv=cpifaceSession->mcpAPI->GetFreq8363(-val);
			calcstep(chn);
			break;
		case mcpCPitchFix:
			chn->orgfrq=val;
			chn->orgdiv=0x10000;
			calcstep(chn);
			break;
		case mcpCPitch6848:
			chn->orgfrq=6848;
			chn->orgdiv=val;
			calcstep(chn);
			break;

		case mcpGSpeed:
			orgspeed=val;
			calcspeed();
			break;

		case mcpMasterVolume:
			if (val>=0 && val<=64)
				mastervol=(val<0)?0:(val>63)?63:val;
			calcvols();
			break;

		case mcpMasterPanning:
			if (val>=-64 && val<=64)
				masterpan=(val<-64)?-64:(val>64)?64:val;
			calcvols();
			break;
		case mcpMasterBalance:
			if (val>=-64 && val<=64)
				masterbal=(val<-64)?-64:(val>64)?64:val;
			calcvols();
			break;
		case mcpMasterSurround:
			mastersrnd=val?1:0;
			calcvols();
			break;
		case mcpMasterChorus:
			masterchr=(val>=64)?64:(val<0)?0:val;
			break;
		case mcpMasterReverb:
			masterrvb=(val>=64)?64:(val<0)?0:val;
			break;
		case mcpMasterSpeed:
			relspeed=(val<16)?16:val;
			calcspeed();
			break;
		case mcpMasterPitch:
			relpitch=(val<4)?4:val;
			calcsteps();
			break;
		case mcpMasterFilter:
			interpolation=val;
			break;
		case mcpMasterAmplify:
			amplify=val;
			if (channelnum)
			{
				calcamptab(amplify);
				mix->mixSetAmplify (cpifaceSession, amplify);
			}
			break;
		case mcpMasterPause:
			_pause=val;
			break;
		break;
	}
}

static int devwMixGET (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	/* Refered by OpenPlayer
	 */
	struct channel *chn;

	if (ch>=channelnum)
		ch=channelnum-1;
	if (ch<0)
		ch=0;
	chn=&channels[ch];
	switch (opt)
	{
		case mcpCStatus:
			return !!(chn->status&MIXRQ_PLAYING);
		case mcpCMute:
			return !!(chn->status&MIXRQ_MUTE);
		case mcpGTimer:
			return imuldiv(playsamps - IdleCache, 65536, samprate);
		case mcpGCmdTimer:
			return umuldiv(cmdtimerpos, 256, samprate);
		case mcpMasterReverb:
			return masterrvb;
		case mcpMasterChorus:
			return masterchr;
	}
	return 0;
}

static void devwMixGetVolRegs (struct cpifaceSessionAPI_t *cpifaceSession, void (*Callback)(struct cpifaceSessionAPI_t *cpifaceSession, const struct ocpvolregstruct *x))
{
	int i;
	for (i=0; i < postprocs; i++)
	{
		if (postproc[i]->VolRegs)
		{
			Callback (cpifaceSession, postproc[i]->VolRegs);
		}
	}
}

static void GetMixChannel(unsigned int ch, struct mixchannel *chn, uint32_t rate)
	/* Refered to by OpenPlayer to mixInit */
{
	struct channel *c=&channels[ch];

	chn->realsamp.fmt=c->samp;
	chn->length=c->length;
	chn->loopstart=c->loopstart;
	chn->loopend=c->loopend;
	chn->fpos=c->fpos;
	chn->pos=c->pos;
	chn->vol.vols[0]=abs(c->vol[0]);
	chn->vol.vols[1]=abs(c->vol[1]);
	chn->step=imuldiv(c->step, samprate, (signed)rate);
	chn->status=0;
	if (c->status&MIXRQ_MUTE)
		chn->status|=MIX_MUTE;
	if (c->status&MIXRQ_PLAY16BIT)
		chn->status|=MIX_PLAY16BIT;
	if (c->status&MIXRQ_LOOPED)
		chn->status|=MIX_LOOPED;
	if (c->status&MIXRQ_PINGPONGLOOP)
		chn->status|=MIX_PINGPONGLOOP;
	if (c->status&MIXRQ_PLAYING)
		chn->status|=MIX_PLAYING;
	if (c->status&MIXRQ_INTERPOLATE)
		chn->status|=MIX_INTERPOLATE;
}

static int devwMixLoadSamples (struct cpifaceSessionAPI_t *cpifaceSession, struct sampleinfo *sil, int n)
{
#if 0
	int i;
	if (!cpifaceSession->mcpAPI->ReduceSamples(sil, n, 0x40000000, mcpRedToMono))
		return 0;

	samples=malloc(sizeof(*sil)*n);
	memcpy(samples, sil, sizeof(*sil)*n);

	for (i=0;i<n;i++)
	{
		samples[i].ptr=malloc(samples[i].length+pagesize()*10);
		samples[i].ptr= (char *)(((int) samples[i].ptr + pagesize()*4) & ~(pagesize()-1) );
		memcpy(samples[i].ptr, sil[i].ptr, samples[i].length);
		mprotect((void *)((int)(samples[i].ptr) & ~(pagesize()-1)), samples[i].length, PROT_READ);
	}
	samplenum=n;

#else
	if (!cpifaceSession->mcpAPI->ReduceSamples(sil, n, 0x40000000, mcpRedToMono))
		return 0;

	samples=sil;
	samplenum=n;
#endif
	return 1;
}

static int devwMixOpenPlayer(int chan, void (*proc)(struct cpifaceSessionAPI_t *cpifaceSession), struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	uint32_t currentrate;
	enum plrRequestFormat format;
	int i;

	fadedown[0]=fadedown[1]=0;
	IdleCache=playsamps=0;
	if (chan>MAXCHAN)
	{
		chan=MAXCHAN;
	}

	if (!cpifaceSession->plrDevAPI)
	{
		return 0;
	}

	playerproc=proc;

	if (!quality)
	{
		scalebuf=0;
		voltabsq=0;
		interpoltabq=0;
		interpoltabq2=0;
		if (!(voltabsr=malloc(sizeof(uint32_t)*513*256))) /*new long [513][256];*/
		{
			goto error_out;
		}
		if (!(interpoltabr=malloc(sizeof(uint8_t)*16*256*2))) /*new unsigned char [16][256][2];*/
		{
			goto error_out;
		}
	} else {
		voltabsr=0;
		interpoltabr=0;
		if (!(scalebuf=malloc(sizeof(int16_t)*MIXBUFLEN))) /* new short [MIXBUFLEN];*/
		{
			goto error_out;
		}

		if (!(voltabsq=malloc(sizeof(uint16_t)*513*2*256))) /*new short [513][2][256];*/
		{
			goto error_out;
		}
		if (!(interpoltabq=malloc(sizeof(uint16_t)*2*32*256*2))) /*new unsigned short [2][32][256][2];*/
		{
			goto error_out;
		}
		if (!(interpoltabq2=malloc(sizeof(uint16_t)*2*16*256*4))) /*new unsigned short [2][16][256][4];*/
		{
			goto error_out;
		}
	}
	if (!(buf32=malloc(sizeof(uint32_t)*(MIXBUFLEN<<1)))) /*new long [MIXBUFLEN<<1];*/
	{
		goto error_out;
	}
	if (!(amptab=malloc(sizeof(int16_t)*3*256+sizeof(int32_t)))) /* PADDING since assembler indexes some bytes beyond tab and ignores upper bits */ /*new short [3][256];*/
	{
		goto error_out;
	}

	if (!(channels=calloc(sizeof(struct channel), chan))) /*new channel[chan];*/
	{
		goto error_out;
	}

	currentrate = cpifaceSession->mcpAPI->MixProcRate / chan;
	samprate = (currentrate > cpifaceSession->mcpAPI->MixMaxRate) ? cpifaceSession->mcpAPI->MixMaxRate : currentrate;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&samprate, &format, source_file, cpifaceSession))
	{
		goto error_out;
	}

	if (!mix->mixInit (cpifaceSession, GetMixChannel, resample, chan, amplify))
	{
		goto error_out_plrDevAPI_Play;
	}

	calcvols();

	if (!quality)
	{
		mixrSetupAddresses(&voltabsr[256], interpoltabr);
		calcinterpoltabr();
		calcvoltabsr();
	} else {
		mixqSetupAddresses(&voltabsq[256], interpoltabq, interpoltabq2);
		calcinterpoltabq();
		calcvoltabsq();
	}

	_pause=0;
	orgspeed=12800;

	channelnum=chan;
	cpifaceSession->PhysicalChannelCount = chan;
	cpifaceSession->mcpGet = devwMixGET;
	cpifaceSession->mcpGetVolRegs = devwMixGetVolRegs;

	cpifaceSession->mcpSet = devwMixSET;

	calcamptab(amplify);
	calcspeed();
	/* playerproc();*/  /* some timing is wrong here!*/
	tickwidth=newtickwidth;
	tickplayed=0;
	cmdtimerpos=0;

	for (i=0; i < postprocs; i++)
	{
		postproc[i]->Init(samprate);
	}

	cpifaceSession->mcpActive = 1;

	return 1;

	// mix->mixClose (cpifaceSession);
error_out_plrDevAPI_Play:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);
error_out:
	free (amptab);        amptab = 0;
	free (voltabsr);      voltabsr = 0;
	free (interpoltabr);  interpoltabr = 0;
	free (scalebuf);      scalebuf = 0;
	free (voltabsq);      voltabsq = 0;
	free (interpoltabq);  interpoltabq = 0;
	free (interpoltabq2); interpoltabq2 = 0;
	free (buf32);         buf32 = 0;
	free (channels);      channels = 0;

	return 0;
}

static void devwMixClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;

	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}

	channelnum=0;

	mix->mixClose (cpifaceSession);

	for (i=0; i < postprocs; i++)
	{
		postproc[i]->Close();
	}

	if (voltabsr) free(voltabsr);
	if (interpoltabr) free(interpoltabr);
	if (scalebuf) free(scalebuf);
	if (voltabsq) free(voltabsq);
	if (interpoltabq) free(interpoltabq);
	if (interpoltabq2) free(interpoltabq2);

	free(channels);
	free(amptab);
	free(buf32);
	scalebuf=0;

	voltabsr=NULL;
	interpoltabr=NULL;
	scalebuf=NULL;
	voltabsq=NULL;
	interpoltabq=NULL;
	interpoltabq2=NULL;

	cpifaceSession->mcpActive = 0;
}

static const struct mcpDevAPI_t devwMix =
{
	devwMixOpenPlayer,
	devwMixLoadSamples,
	devwMixIdle,
	devwMixClosePlayer,
	devwMixProcKey
};

static void mixrRegisterPostProc (const struct PostProcIntegerRegStruct *post)
{
	if (postprocs >= MIX_MAX_POSTPROC)
	{
		return;
	}
	postproc[postprocs] = post;
	postprocs++;
}

static const struct mcpDevAPI_t *wmixInit (const struct mcpDriver_t *driver, const struct configAPI_t *config, const struct mixAPI_t *mixAPI)
{
	char regname[50];
	const char *regs;

	mix = mixAPI;

	amplify=65535;
	relspeed=256;
	relpitch=256;
	interpolation=0;
	mastervol=64;
	masterbal=0;
	masterpan=0;
	mastersrnd=0;
	channelnum=0;

	quality = (driver == &mcpMixerQ);
	resample = config->GetProfileBool(driver->name, "mixresample", 0, 0);

	fprintf(stderr, "[%s] %s C version (resample=%d)\n", driver->name, quality?"dwmixaq.c":"dwmixa.c", resample);

	regs = config->GetProfileString(driver->name, "postprocs", "");
	while (config->GetSpaceListEntry(regname, &regs, 49))
	{
		const struct PostProcIntegerRegStruct *postproc = mixAPI->mcpFindPostProcInteger (regname);
		if (postproc)
		{
			fprintf(stderr, "[%s] registering post processing plugin %s\n", driver->name, regname);
			mixrRegisterPostProc (postproc);
		}
	}

	return &devwMix;
}

static void wmixClose (const struct mcpDriver_t *driver)
{
	postprocs = 0;
}


static int wmixDetect (const struct mcpDriver_t *driver)
{
	return 1;
}

static int devwMixProcKey (uint16_t key)
{
	int i;
	for (i=0; i < postprocs; i++)
	{
		int r = postproc[i]->ProcessKey (key);
		if (r)
		{
			return r;
		}
	}
	return 0;
}

static int devwMixFPluginInit (struct PluginInitAPI_t *API)
{
	API->mcpRegisterDriver (&mcpMixer);
	API->mcpRegisterDriver (&mcpMixerQ);

	return errOk;
}

static void devwMixFPluginClose (struct PluginCloseAPI_t *API)
{
	API->mcpUnregisterDriver (&mcpMixer);
	API->mcpUnregisterDriver (&mcpMixerQ);
}

static const struct mcpDriver_t mcpMixer =
{
	"devwMix",
	"Integer Mixer",
	wmixDetect,
	wmixInit,
	wmixClose
};

static const struct mcpDriver_t mcpMixerQ =
{
	"devwMixQ",
	"Integer Quality Mixer",
	wmixDetect,
	wmixInit,
	wmixClose
};

DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "devwmix", .desc = "OpenCP Wavetable Device: Mixer (c) 1994-'23 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99, .PluginInit = devwMixFPluginInit, .PluginClose = devwMixFPluginClose};
