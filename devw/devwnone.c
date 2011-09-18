/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Wavetable Device: No Sound
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
 *    -added _dllinfo record
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dev/imsdev.h"
#include "dev/mcp.h"
#include "stuff/timer.h"
#include "dev/mix.h"
/*#include "dma.h"*/
#include "stuff/imsrtns.h"

#define TIMERRATE 17100
#define MAXCHAN 256

#define NONE_PLAYING 1
#define NONE_MUTE 2
#define NONE_LOOPED 4
#define NONE_PINGPONGLOOP 8
#define NONE_PLAY16BIT 16

static const unsigned long samprate=44100;

extern struct sounddevice mcpNone;

struct channel
{
	void *samp;
	uint32_t length;
	uint32_t loopstart;
	uint32_t loopend;
	uint32_t replen;
	int32_t step;
	uint32_t pos;
	uint16_t fpos;
	uint8_t status;
	int8_t vol[2];
	uint8_t orgvol[2];
	uint16_t orgrate;
	int32_t orgfrq;
	int32_t orgdiv;
	uint8_t direct;
	uint8_t volopt;
	uint8_t orgloop;
	int orgvolx;
	int orgpan;
};

static int pause;

static struct sampleinfo *samples;
static int samplenum;

static unsigned long amplify;
static unsigned char transform[2][2];
static unsigned long relpitch;
static int filter;

static int channelnum;
static struct channel *channels;

static void (*playerproc)(void);
static unsigned long tickwidth;
static unsigned long tickplayed;
static unsigned long orgspeed;
static unsigned short relspeed;
static unsigned long newtickwidth;
static unsigned long cmdtimerpos;

static int mastervol;
static int masterpan;
static int masterbal;

#include "dwnone_asminc.c"

static void calcstep(struct channel *c)
{
	if (!(c->status&NONE_PLAYING))
		return;
	c->step=imuldiv(imuldiv(c->orgrate, ((c->step>=0)^c->direct)?c->orgfrq:-c->orgfrq, c->orgdiv)<<8, relpitch, samprate);
	c->direct=(c->orgfrq<0)^(c->orgdiv<0);
}

static void calcspeed(void)
{
	if (channelnum)
		newtickwidth=imuldiv(256*256, samprate, orgspeed*relspeed);
}

static void transformvol(struct channel *ch)
{
	int v;
	v=transform[0][0]*ch->orgvol[0]+transform[0][1]*ch->orgvol[1];
	ch->vol[0]=(v>4096)?64:(v<-4096)?-64:((v+32)>>6);

	v=transform[1][0]*ch->orgvol[0]+transform[1][1]*ch->orgvol[1];
	ch->vol[1]=(v>4096)?64:(v<-4096)?-64:((v+32)>>6);
}

static void calcvol(struct channel *chn)
{
	if (chn->orgpan<0)
	{
		chn->orgvol[1]=(chn->orgvolx*(0x80+chn->orgpan))>>10;
		chn->orgvol[0]=(chn->orgvolx>>2)-chn->orgvol[1];
	} else {
		chn->orgvol[0]=(chn->orgvolx*(0x80-chn->orgpan))>>10;
		chn->orgvol[1]=(chn->orgvolx>>2)-chn->orgvol[0];
	}
	transformvol(chn);
}

static void playchannels(unsigned short len)
{
	int i;

	if (!len)
		return;
	for (i=0; i<channelnum; i++)
	{
		struct channel *c=&channels[i];
		if (c->status&NONE_PLAYING)
			nonePlayChannel(len, c);
	}
}


static void timerproc(void)
{
	unsigned int bufdelta=samprate*TIMERRATE/1193046;

	if (channelnum&&!pause)
	{
		while ((tickwidth-tickplayed)<=bufdelta)
		{
			playchannels(tickwidth-tickplayed);
			bufdelta-=tickwidth-tickplayed;
			tickplayed=0;
			playerproc();
			cmdtimerpos+=tickwidth;
			tickwidth=newtickwidth;
		}
		playchannels(bufdelta);
		tickplayed+=bufdelta;
	}
}


static void calcvols(void)
{
	signed char vols[2][2];
	int i;

	vols[0][0]=0x20+(masterpan>>1);
	vols[0][1]=0x20-(masterpan>>1);
	vols[1][0]=0x20-(masterpan>>1);
	vols[1][1]=0x20+(masterpan>>1);

	if (masterbal>0)
	{
		vols[0][0]=((signed short)vols[0][0]*(0x40-masterbal))>>6;
		vols[0][1]=((signed short)vols[0][1]*(0x40-masterbal))>>6;
	} else {
		vols[1][0]=((signed short)vols[1][0]*(0x40+masterbal))>>6;
		vols[1][1]=((signed short)vols[1][1]*(0x40+masterbal))>>6;
	}

	vols[0][0]=((signed short)vols[0][0]*mastervol)>>6;
	vols[0][1]=((signed short)vols[0][1]*mastervol)>>6;
	vols[1][0]=((signed short)vols[1][0]*mastervol)>>6;
	vols[1][1]=((signed short)vols[1][1]*mastervol)>>6;

	memcpy(transform, vols, 4);
	for (i=0; i<channelnum; i++)
		transformvol(&channels[i]);
}

static void calcsteps(void)
{
	int i;
	for (i=0; i<channelnum; i++)
		calcstep(&channels[i]);
}

static void SetInstr(struct channel *chn, unsigned short samp)
{
	struct sampleinfo *s=&samples[samp];
	chn->status&=~(NONE_PLAYING|NONE_LOOPED|NONE_PINGPONGLOOP|NONE_PLAY16BIT);
	chn->samp=s->ptr;
	if (s->type&mcpSamp16Bit)
		chn->status|=NONE_PLAY16BIT;
	if (s->type&mcpSampLoop)
		chn->status|=NONE_LOOPED;
	if (s->type&mcpSampBiDi)
		chn->status|=NONE_PINGPONGLOOP;
	chn->length=s->length;
	chn->loopstart=s->loopstart;
	chn->loopend=s->loopend;
	chn->replen=(chn->status&NONE_LOOPED)?(s->loopend-s->loopstart):0;
	chn->orgloop=chn->status&NONE_LOOPED;
	chn->orgrate=s->samprate;
	chn->step=0;
	chn->pos=0;
	chn->fpos=0;
	chn->orgvol[0]=0;
	chn->orgvol[1]=0;
	chn->vol[0]=0;
	chn->vol[1]=0;
}

static void SET(int ch, int opt, int val)
{
	int tmp;
	switch (opt)
	{
		case mcpGSpeed:
			orgspeed=val;
			calcspeed();
			break;
		case mcpCInstrument:
			SetInstr(&channels[ch], val);
			break;
		case mcpCMute:
			if (val)
				channels[ch].status|=NONE_MUTE;
			else
				channels[ch].status&=~NONE_MUTE;
			break;
		case mcpCStatus:
			if (!val)
				channels[ch].status&=~NONE_PLAYING;
			break;
		case mcpCReset:
			tmp=channels[ch].status&NONE_MUTE;
			memset(&channels[ch], 0, sizeof(struct channel));
			channels[ch].status=tmp;
			break;
		case mcpCVolume:
			channels[ch].orgvolx=(val>0xF8)?0x100:(val<0)?0:(val+3);
			calcvol(&channels[ch]);
			break;
		case mcpCPanning:
			channels[ch].orgpan=(val>0x78)?0x80:(val<-0x78)?-0x80:val;
			calcvol(&channels[ch]);
			break;
		case mcpMasterAmplify:
			amplify=val;
			if (channelnum)
				mixSetAmplify(amplify);
			break;
		case mcpMasterPause:
			pause=val;
			break;
		case mcpCPosition:
			channels[ch].status&=~NONE_PLAYING;
			if ((unsigned)val>=channels[ch].length)
			{
				if (channels[ch].status&NONE_LOOPED)
					val=channels[ch].loopstart;
				else
					break;
			}

			channels[ch].step=0;
			channels[ch].direct=0;
			calcstep(&channels[ch]);
			channels[ch].pos=val;
			channels[ch].fpos=0;
			channels[ch].status|=NONE_PLAYING;
			break;
		case mcpCPitch:
			channels[ch].orgfrq=8363;
			channels[ch].orgdiv=mcpGetFreq8363(-val);calcstep(&channels[ch]);
			break;
		case mcpCPitchFix:
			channels[ch].orgfrq=val;
			channels[ch].orgdiv=0x10000;
			calcstep(&channels[ch]);
			break;
		case mcpCPitch6848:
			channels[ch].orgfrq=6848;
			channels[ch].orgdiv=val;
			calcstep(&channels[ch]);
			break;
		case mcpMasterVolume:
			mastervol=val;
			calcvols();
			break;
		case mcpMasterPanning:
			masterpan=val;
			calcvols();
			break;
		case mcpMasterBalance:
			masterbal=val;
			calcvols();
			break;
		case mcpMasterSpeed:
			relspeed=(val<16)?16:val;
			calcspeed();
			break;
		case mcpMasterPitch:
			relpitch=val;
			calcsteps();
			break;
		case mcpMasterFilter:
			filter=val;
			break;
	}
}

static int GET(int ch, int opt)
{
	switch (opt)
	{
		case mcpCStatus:
			return !!(channels[ch].status&NONE_PLAYING);
		case mcpCMute:
			return !!(channels[ch].status&NONE_MUTE);
		case mcpGTimer:
			return tmGetTimer();
		case mcpGCmdTimer:
			return umuldiv(cmdtimerpos, 65536, samprate);
	}
	return 0;
}


static void GetMixChannel(unsigned int ch, struct mixchannel *chn, uint32_t rate)
{
	struct channel *c=&channels[ch];
	chn->samp=c->samp;
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
	if (c->status&NONE_MUTE)
		chn->status|=MIX_MUTE;
	if (c->status&NONE_PLAY16BIT)
		chn->status|=MIX_PLAY16BIT;
	if (c->status&NONE_LOOPED)
		chn->status|=MIX_LOOPED;
	if (c->status&NONE_PINGPONGLOOP)
		chn->status|=MIX_PINGPONGLOOP;
	if (c->status&NONE_PLAYING)
		chn->status|=MIX_PLAYING;
	if (filter)
		chn->status|=MIX_INTERPOLATE;
}



static int LoadSamples(struct sampleinfo *sil, int n)
{
	if (!mcpReduceSamples(sil, n, 0x40000000, mcpRedToMono))
		return 0;

	samples=sil;
	samplenum=n;

	return 1;
}


static int OpenPlayer(int chan, void (*proc)(void))
{
	if (chan>MAXCHAN)
		chan=MAXCHAN;

	if (!(channels=malloc(sizeof(struct channel)*chan)))
	{
		/*delete channels; <- that was actually a BUG! you can't free NULL */
		return 0;
	}


	playerproc=proc;

	if (!mixInit(GetMixChannel, 1, chan, amplify))
		return 0;

	memset(channels, 0, sizeof(struct channel)*chan);

	calcvols();
	pause=0;
	orgspeed=12800;
	calcspeed();
	tickwidth=newtickwidth;
	tickplayed=0;
	cmdtimerpos=0;

	channelnum=chan;
	tmInit(timerproc, TIMERRATE);

	mcpNChan=chan;

	return 1;
}

static void ClosePlayer(void)
{
	mcpNChan=0;
	tmClose();
	channelnum=0;
	mixClose();
}

static int Init(const struct deviceinfo *c)
{
	amplify=65535;
	relspeed=256;
	relpitch=256;
	filter=0;
	mastervol=64;
	masterpan=64;
	masterbal=0;

	channelnum=0;

	mcpLoadSamples=LoadSamples;
	mcpOpenPlayer=OpenPlayer;
	mcpClosePlayer=ClosePlayer;
	mcpSet=SET;
	mcpGet=GET;

	return 1;
}

static void Close(void)
{
	mcpOpenPlayer=0;
}


static int Detect(struct deviceinfo *c)
{
	c->devtype=&mcpNone;
	c->port=-1;
	c->port2=-1;
/*
	c->irq=-1;
	c->irq2=-1;
	c->dma=-1;
	c->dma2=-1;
*/
	c->subtype=-1;
	c->chan=(MAXCHAN>99)?99:MAXCHAN;
	c->mem=0;
	return 1;
}

struct sounddevice mcpNone={SS_WAVETABLE, 0, "None", Detect, Init, Close, 0};
char *dllinfo = "driver mcpNone";

struct linkinfostruct dllextinfo = {"devwnone", "OpenCP Wavetable Device: None (c) 1994-09 Niklas Beisert, Tammo Hinrichs", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
