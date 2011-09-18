/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * SIDPlay - SID file player based on Michael Schwendt's SIDPlay routines
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
 *  -kb980717  Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 *  -ss04????  Stian Skjelstad <stian@nixia.no>
 *    -ported the assembler to gcc
 *  -ss040908  Stian Skjelstad <stian@nixia.no>
 *    -made assembler optimize safe
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
extern "C"
{
#include "stuff/poll.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/deviplay.h"
#include "dev/mixclip.h"
#include "cpiface/cpiface.h"
#include "boot/psetting.h"
}

#include "sid.h"
#include <sidplay/emucfg.h>
#include <sidplay/sidtune.h>
#include "opstruct.h"

static emuEngine *myEmuEngine;
static emuConfig *myEmuConfig;

static sidTune *mySidTune;
static sidTuneInfo *mySidTuneInfo;

extern bool sidEmuFastForwardReplay(int percent);

extern sidOperator optr1, optr2, optr3;

static unsigned char stereo;
static unsigned char bit16;
static unsigned char signedout;
static unsigned long samprate;
static unsigned char reversestereo;
static unsigned char srnd;

static uint16_t *buf16;
static uint32_t bufpos;
static uint32_t buflen;
static void *plrbuf;


static uint16_t *cliptabl;
static uint16_t *cliptabr;
static uint32_t amplify;
static uint32_t voll,volr;

static char active;
static char inpause;

static char sidpmute[4];
static int16_t v4outl, v4outr;

static volatile int clipbusy=0;

static void calccliptab(int32_t ampl, int32_t ampr)
{
	clipbusy++;

	if (!stereo)
	{
		ampl=(abs(ampl)+abs(ampr))>>1;
		ampr=0;
	}

	mixCalcClipTab(cliptabl, abs(ampl));
	mixCalcClipTab(cliptabr, abs(ampr));

	int i;
	if (signedout)
		for (i=0; i<256; i++)
		{
			cliptabl[i+512]^=0x8000;
			cliptabr[i+512]^=0x8000;
		}

	clipbusy--;
}




static void timerproc()
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	uint32_t bufplayed=plrGetBufPos()>>(stereo+bit16);
	uint32_t bufdelta;
	uint32_t pass2;

	if (bufplayed==bufpos)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}
	int quietlen=0;
	bufdelta=(buflen+bufplayed-bufpos)%buflen;

	if (inpause)
		quietlen=bufdelta;

	bufdelta-=quietlen;

	if (bufdelta)
	{
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;

		plrClearBuf(buf16, bufdelta*2, 1);

    //sidplay nach buf16

		sidEmuFillBuffer(*myEmuEngine,*mySidTune,buf16,bufdelta<<(stereo+1));

		if (stereo && srnd)
			for (uint32_t i=0; i<bufdelta; i++)
				buf16[2*i]^=0xFFFF;

		if (bit16)
		{
			if (stereo)
			{
				mixClipAlt2((uint16_t *)plrbuf+bufpos*2, buf16, bufdelta-pass2, cliptabl);
				mixClipAlt2((uint16_t *)plrbuf+bufpos*2+1, buf16+1, bufdelta-pass2, cliptabr);
				if (pass2)
				{
					mixClipAlt2((uint16_t *)plrbuf, buf16+2*(bufdelta-pass2), pass2, cliptabl);
					mixClipAlt2((uint16_t *)plrbuf+1, buf16+2*(bufdelta-pass2)+1, pass2, cliptabr);
				}
			} else {
				mixClipAlt((uint16_t *)plrbuf+bufpos, buf16, bufdelta-pass2, cliptabl);
				if (pass2)
					mixClipAlt((uint16_t *)plrbuf, buf16+bufdelta-pass2, pass2, cliptabl);
			}
		} else {
			if (stereo)
			{
				mixClipAlt2(buf16, buf16, bufdelta, cliptabl);
				mixClipAlt2(buf16+1, buf16+1, bufdelta, cliptabr);
			} else
				mixClipAlt(buf16, buf16, bufdelta, cliptabl);
			plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t *)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	bufdelta=quietlen;
	if (bufdelta)
	{
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		if (bit16)
		{
			plrClearBuf((unsigned short*)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, !signedout);
			if (pass2)
				plrClearBuf((unsigned short*)plrbuf, pass2<<stereo, !signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, !signedout);
			plr16to8((unsigned char*)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((unsigned char*)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

	if (plrIdle)
		plrIdle();

	clipbusy--;
}


static void updateconf()
{
	clipbusy++;
	myEmuEngine->setConfig(*myEmuConfig);
	clipbusy--;
}


void __attribute__ ((visibility ("internal"))) sidpIdle(void)
{
	timerproc();
}

unsigned char __attribute__ ((visibility ("internal"))) sidpOpenPlayer(FILE *f)
{
	if (!plrPlay)
		return 0;

	fseek(f, 0, SEEK_END);
	const int length=ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char *buf=new unsigned char[length];
	if (fread(buf, length, 1, f)!=1)
	{
		fprintf(stderr, __FILE__": fread failed #1\n");
		return 0;
	}
	mySidTune = new sidTune(buf, length);
	delete [] buf;

	cliptabl=new unsigned short[1793];
	cliptabr=new unsigned short[1793];

	if (!cliptabl||!cliptabr)
	{
		delete[] cliptabl;
		delete[] cliptabr;
		delete mySidTune;
		return 0;
	}

	myEmuEngine = new emuEngine;
	myEmuConfig = new emuConfig;
	mySidTuneInfo = new sidTuneInfo;
	if (!mySidTune || !mySidTuneInfo)
	{
		delete mySidTune;
		delete mySidTuneInfo;
		delete[] cliptabl;
		delete[] cliptabr;
		delete myEmuEngine;
		delete myEmuConfig;
		return 0;
	}

	int playrate=cfGetProfileInt("commandline_s", "r", cfGetProfileInt2(cfSoundSec, "sound", "mixrate", 44100, 10), 10);
	if (playrate<66)
	{
		if (playrate%11)
			playrate*=1000;
		else
			playrate=playrate*11025/11;
	}

	plrSetOptions(playrate, PLR_STEREO|PLR_16BIT);
	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize))
		return 0;

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);
	samprate=plrRate;
	srnd=0;

	myEmuEngine->getConfig(*myEmuConfig);

	myEmuConfig->frequency=samprate;
	myEmuConfig->bitsPerSample=SIDEMU_16BIT;
	myEmuConfig->sampleFormat=SIDEMU_UNSIGNED_PCM;
	myEmuConfig->channels=stereo?SIDEMU_STEREO:SIDEMU_MONO;
	myEmuConfig->sidChips=1;

	myEmuConfig->volumeControl=SIDEMU_FULLPANNING;
	myEmuConfig->autoPanning=SIDEMU_CENTEREDAUTOPANNING;

	myEmuConfig->mos8580=0;
	myEmuConfig->measuredVolume=0;
	myEmuConfig->emulateFilter=1;
	myEmuConfig->filterFs=SIDEMU_DEFAULTFILTERFS;
	myEmuConfig->filterFm=SIDEMU_DEFAULTFILTERFM;
	myEmuConfig->filterFt=SIDEMU_DEFAULTFILTERFT;
	myEmuConfig->memoryMode=MPU_BANK_SWITCHING;
	myEmuConfig->clockSpeed=SIDTUNE_CLOCK_PAL;
	myEmuConfig->forceSongSpeed=0;
	myEmuConfig->digiPlayerScans=10;

	myEmuEngine->setConfig(*myEmuConfig);

	memset(sidpmute,0,4);

	inpause=0;
	amplify=65536;
	voll=256;
	volr=256;
	calccliptab((amplify*voll)>>8, (amplify*volr)>>8);

	buf16=new uint16_t [buflen*2];

	if (!buf16)
	{
		plrClosePlayer();
		delete mySidTune;
		delete mySidTuneInfo;
		delete[] cliptabl;
		delete[] cliptabr;
		delete myEmuEngine;
		delete myEmuConfig;
		return 0;
	}

	bufpos=0;

	mySidTune->getInfo(*mySidTuneInfo);
	sidEmuInitializeSong(*myEmuEngine,*mySidTune,mySidTuneInfo->startSong);
	sidEmuFastForwardReplay(100);
	mySidTune->getInfo(*mySidTuneInfo);

	// construct song message
	{
		int i,j;
		const int msgLen=50;
		static char* msg[msgLen];
		for(i=0; i<msgLen; i++)
			msg[i]=0;
		i=0;
		for(j=0; j<mySidTuneInfo->numberOfInfoStrings && i<msgLen; j++)
			msg[i++]=mySidTuneInfo->infoString[j];
		for(j=0; j<mySidTuneInfo->numberOfCommentStrings && i<msgLen; j++)
			msg[i++]=mySidTuneInfo->commentString[j];
		if(i<msgLen)
			msg[i++]=(char*)(mySidTuneInfo->formatString);
		if(i<msgLen)
			msg[i++]=(char*)(mySidTuneInfo->speedString);
		plUseMessage(msg);
	}

	if (!pollInit(timerproc))
	{
		plrClosePlayer();
		return 0;
	}

	active=1;

	return 1;
}

void __attribute__ ((visibility ("internal"))) sidpClosePlayer(void)
{
	active=0;

	pollClose();

	plrClosePlayer();

	delete myEmuEngine;
	delete myEmuConfig;
	delete mySidTune;
	delete mySidTuneInfo;

	delete[] buf16;
	delete[] cliptabl;
	delete[] cliptabr;
}


void __attribute__ ((visibility ("internal"))) sidpPause(unsigned char p)
{
	inpause=p;
}

void __attribute__ ((visibility ("internal"))) sidpSetAmplify(unsigned long amp)
{
	amplify=amp;
	calccliptab((amplify*voll)>>8, (amplify*volr)>>8);
}

void __attribute__ ((visibility ("internal"))) sidpSetVolume(unsigned char vol, signed char bal, signed char pan, unsigned char opt)
{
	pan=pan;
	voll=vol*4;
	volr=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
	sidpSetAmplify(amplify);
	srnd=opt;
}

void __attribute__ ((visibility ("internal"))) sidpGetGlobInfo(sidTuneInfo &si)
{
	mySidTune->getInfo(si);
}

void __attribute__ ((visibility ("internal"))) sidpStartSong(char sng)
{
	if (sng<1)
		sng=1;
	if (sng>mySidTuneInfo->songs)
		sng=mySidTuneInfo->songs;
	while (clipbusy)
	{
	}
	clipbusy++;
	sidEmuInitializeSong(*myEmuEngine,*mySidTune,sng);
	mySidTune->getInfo(*mySidTuneInfo);
	clipbusy--;
}

void __attribute__ ((visibility ("internal"))) sidpToggleVideo(void)
{
	int &cs=myEmuConfig->clockSpeed;
	cs=(cs==SIDTUNE_CLOCK_PAL)?SIDTUNE_CLOCK_NTSC:SIDTUNE_CLOCK_PAL;
	updateconf();
}

char __attribute__ ((visibility ("internal"))) sidpGetVideo(void)
{
	int &cs=myEmuConfig->clockSpeed;
	return (cs==SIDTUNE_CLOCK_PAL);
}

char __attribute__ ((visibility ("internal"))) sidpGetFilter(void)
{
	return myEmuConfig->emulateFilter;
}


void __attribute__ ((visibility ("internal"))) sidpToggleFilter(void)
{
	myEmuConfig->emulateFilter^=1;
	updateconf();
}


char __attribute__ ((visibility ("internal"))) sidpGetSIDVersion(void)
{
	return myEmuConfig->mos8580;
}


void __attribute__ ((visibility ("internal"))) sidpToggleSIDVersion(void)
{
	myEmuConfig->mos8580^=1;
	updateconf();
}

void __attribute__ ((visibility ("internal"))) sidpMute(int i, int m)
{
	sidpmute[i]=m;
}


/*extern ubyte filterType;*/

void __attribute__ ((visibility ("internal"))) sidpGetChanInfo(int i, sidChanInfo &ci)
{
	sidOperator ch;
	switch (i)
	{
		case 0: ch=optr1; break;
		case 1: ch=optr2; break;
		/* case 2: */
		default: ch=optr3; break;
	};
	ci.freq=ch.SIDfreq;
	ci.ad=ch.SIDAD;
	ci.sr=ch.SIDSR;
	ci.pulse=ch.SIDpulseWidth&0xfff;
	ci.wave=ch.SIDctrl;
	ci.filtenabled=ch.filtEnabled;
	ci.filttype=0; // filterType;       TODO TODO
	ci.leftvol =ch.enveVol*ch.gainLeft>>16;
	ci.rightvol=ch.enveVol*ch.gainRight>>16;
	long pulsemul;
	switch (ch.SIDctrl & 0xf0)
	{
		case 0x10:
			ci.leftvol*=192;
			ci.rightvol*=192;
			break;
		case 0x20:
			ci.leftvol*=224;
			ci.rightvol*=224;
			break;
		case 0x30:
			ci.leftvol*=208;
			ci.rightvol*=208;
			break;
		case 0x40:
			pulsemul=2*(ci.pulse>>4);
			if (ci.pulse & 0x800)
				pulsemul=511-pulsemul;
			ci.leftvol*=pulsemul;
			ci.rightvol*=pulsemul;
			break;
		case 0x50:
			pulsemul=255-(ci.pulse>>4);
			ci.leftvol*=pulsemul;
			ci.rightvol*=pulsemul;
			break;
		case 0x60:
			pulsemul=255-(ci.pulse>>4);
			ci.leftvol*=pulsemul;
			ci.rightvol*=pulsemul;
			break;
		case 0x70:
			ci.leftvol*=224;
			ci.rightvol*=224;
			break;
		case 0x80:
			ci.leftvol*=240;
			ci.rightvol*=240;
			break;
		default:
			ci.leftvol=ci.rightvol=0;
	}
	ci.leftvol=ci.leftvol>>8;
	ci.rightvol=ci.rightvol>>8;
}


void __attribute__ ((visibility ("internal"))) sidpGetDigiInfo(sidDigiInfo &di)
{
	short vv=abs(v4outl)>>7;
	if (vv>di.l)
		di.l=vv;
	else
		if (di.l>4)
			di.l-=4;
		else
			di.l=0;
	vv=abs(v4outr)>>7;
	if (vv>di.r)
		di.r=vv;
	else
		if (di.r>4)
			di.r-=4;
		else
			di.r=0;
}
