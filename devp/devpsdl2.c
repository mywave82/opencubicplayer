/* OpenCP Module Player
 * copyright (c) 2011-'22 François Revol <revol@free.fr>
 *
 * SDL Player device
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
 *  -fr110319   François Revol <revol@free.fr>
 *    -copied from devpcoreaudio
 */

#include "config.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_audio.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "stuff/timer.h"
#include "stuff/imsrtns.h"

#ifdef SDL2_DEBUG
 #define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
 #define PRINT(...) do {} while(0)
#endif

extern struct sounddevice plrSDL2;

/* stolen from devpcoreaudio.c */

static void *playbuf=0;
static int buflen;
volatile static int kernpos, cachepos, bufpos; /* in bytes */
static int delay; /* in samples */
#if SDL_VERSION_ATLEAST(2,0,18)
static volatile uint64_t lastCallbackTime;
#else
static volatile uint32_t lastCallbackTime;
#endif
/* kernpos = kernel write header
 * bufpos = the write header given out of this driver */

/*  playbuf     kernpos  cachepos   bufpos      buflen
 *    |           | kernlen |          |          |
 *    |           |   cachelen         |          |
 *
 *  on flush, we update all variables> *  on getbufpos we return kernpos-(1 sample) as safe point to buffer up to
 *  on getplaypos we use last known kernpos if locked, else update kernpos
 */


volatile static int cachelen, kernlen; /* to make life easier */

volatile static uint32_t playpos; /* how many samples have we done totally */

/* Avoid deadlocks due to signals catched when in the critical section */
#define SDL_LockAudio() \
	sigset_t _orgmask; \
	sigset_t _mask; \
	sigemptyset(&_mask); \
	sigaddset(&_mask, SIGALRM); \
	sigprocmask(SIG_BLOCK, &_mask, &_orgmask); \
	SDL_LockAudio()

#define SDL_UnlockAudio() \
	SDL_UnlockAudio(); \
	sigprocmask(SIG_SETMASK, &_orgmask, NULL)


void theRenderProc(void *userdata, Uint8 *stream, int len)
{
	int i, i2;
	int done = 0;

	PRINT("%s(,,%d)\n", __FUNCTION__, len);

	memset(stream, 0, len);

	SDL_LockAudio();

#if SDL_VERSION_ATLEAST(2,0,18)
	lastCallbackTime = SDL_GetTicks64();
#else
	lastCallbackTime = SDL_GetTicks();
#endif

	i = cachelen;/* >>2  *stereo + 16it */
	if (i > len)
		i = len;

	kernlen = done = i;
	cachelen -= i;
	cachepos = kernpos;
	playpos += i<<(2/*stereo+bit16*/);

	if ((i+kernpos)>buflen)
	{
		i2 = ( i + kernpos ) % buflen;
		i = i - i2;
	} else {
		i2 = 0;
	}

	memcpy(stream, playbuf+kernpos, i);
	if (i2)
		memcpy(stream+i, playbuf, i2);

	kernpos = (kernpos+i+i2)%buflen;

	SDL_UnlockAudio();

	if (done < len)
		PRINT("%s: got %d of %d\n", __FUNCTION__, done, len);
}


/* stolen from devposs */
static int sdl2GetBufPos(void)
{
	int retval;
	PRINT("%s()\n", __FUNCTION__);

	/* this thing is utterly broken */

	SDL_LockAudio();
	if (kernpos==bufpos)
	{
		if (cachelen|kernlen)
		{
			retval=kernpos;
			SDL_UnlockAudio();
			return retval;
		}
	}
	retval=(kernpos+buflen-4 /* 1 sample = 4 bytes*/)%buflen;
	SDL_UnlockAudio();
	return retval;
}

static int sdl2GetPlayPos(void)
{
	int retval;
#if SDL_VERSION_ATLEAST(2,0,18)
	uint64_t curTime;
#else
	uint32_t curTime;
#endif

	PRINT("%s()\n", __FUNCTION__);

	SDL_LockAudio();
	retval=cachepos;

#if SDL_VERSION_ATLEAST(2,0,18)
	curTime = SDL_GetTicks64();
#else
	curTime = SDL_GetTicks();
#endif

	SDL_UnlockAudio();

	retval += plrRate * 4 * (curTime - lastCallbackTime) / 1000;
	retval %= buflen;
	return retval;
}

static void sdl2Idle(void)
{
	PRINT("%s()\n", __FUNCTION__);
}

static void sdl2AdvanceTo(unsigned int pos)
{
	PRINT("%s(%u)\n", __FUNCTION__, pos);
	SDL_LockAudio();

	cachelen+=(pos-bufpos+buflen)%buflen;
	bufpos=pos;

	SDL_UnlockAudio();
}

static uint32_t sdl2GetTimer(void)
{
	long retval;
	PRINT("%s()\n", __FUNCTION__);

	SDL_LockAudio();

	retval=playpos-kernlen;
	if (retval < delay)
		retval = 0;
	else
		retval -= delay;

	SDL_UnlockAudio();

	return imuldiv(retval, 65536>>(2/*stereo+bit16*/), plrRate);
}

static void sdl2Stop(void)
{
	PRINT("%s()\n", __FUNCTION__);
	/* TODO, forceflush */

	SDL_PauseAudio(1);

	if (playbuf)
	{
		free(playbuf);
		playbuf=0;
	}

	plrGetBufPos=0;
	plrGetPlayPos=0;
	plrIdle=0;
	plrAdvanceTo=0;
	plrGetTimer=0;

	SDL_CloseAudio();
}

static int sdl2Play(void **buf, unsigned int *len, struct ocpfilehandle_t *source_file)
{
	SDL_AudioSpec desired, obtained;
	int status;
	PRINT("%s(,&%d)\n", __FUNCTION__, *len);

	if ((*len)<(plrRate&~3))
		*len=plrRate&~3;
	if ((*len)>(plrRate*4))
		*len=plrRate*4;
	playbuf=*buf=malloc(*len);

	memset(*buf, 0x80008000, (*len)>>2);
	buflen = *len;

	cachepos=0;
	kernpos=0;
	bufpos=0;
	cachelen=0;
	kernlen=0;

	playpos=0;

	plrGetBufPos=sdl2GetBufPos;
	plrGetPlayPos=sdl2GetPlayPos;
	plrIdle=sdl2Idle;
	plrAdvanceTo=sdl2AdvanceTo;
	plrGetTimer=sdl2GetTimer;

	SDL_memset (&desired, 0, sizeof (desired));
	desired.freq = plrRate;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = plrRate / 8; /**len;*/
	desired.callback = theRenderProc;
	desired.userdata = NULL;

#if SDL_VERSION_ATLEAST(2,0,18)
	lastCallbackTime = SDL_GetTicks64();
#else
	lastCallbackTime = SDL_GetTicks();
#endif

	status=SDL_OpenAudio(&desired, &obtained);
	if (status < 0)
	{
		fprintf(stderr, "[SDL2] SDL_OpenAudio returned %d (%s)\n", (int)status, SDL_GetError());
		free(*buf);
		*buf = playbuf = 0;
		plrGetBufPos = 0;
		plrGetPlayPos = 0;
		plrIdle = 0;
		plrAdvanceTo = 0;
		plrGetTimer = 0;
		return 0;
	}
	delay = obtained.samples;
	/*plrRate = sdlSpec.freq;*/
	SDL_PauseAudio(0);
	return 1;
}

static void sdl2SetOptions(unsigned int rate, int opt)
{
	PRINT("%s(%u, %d)\n", __FUNCTION__, rate, opt);
	plrRate=rate; /* fixed */
	plrOpt=PLR_STEREO_16BIT_SIGNED; /* fixed fixed fixed */
}

static int sdl2Init(const struct deviceinfo *c)
{
	PRINT("%s()\n", __FUNCTION__);
	if (SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		fprintf(stderr, "[SDL2] SDL_InitSubSystem (SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
		SDL_ClearError();
		return 0;
	}

#ifdef SDL2_DEBUG
	fprintf(stderr, "[SDL2] Audio drivers:\n");
	{
		int i, n;
		const char *current_driver = SDL_GetCurrentAudioDriver ();

		if (!current_driver) current_driver = ""; /* should never happen */

		n = SDL_GetNumAudioDrivers ();
		for (i=0; i < n; ++i)
		{
			const char *iter = SDL_GetAudioDriver (i);
			fprintf (stderr, "   %s %s\n", strcmp (iter, current_driver) ? "          " : "(selected)", iter);
		}

		n = SDL_GetNumAudioDevices (0);
		if (n > 0)
		{
			fprintf (stderr, "[SDL2] Audio devices:\n");
			for (i=0; i < n; i++)
			{
				fprintf (stderr, "   Audio device %d: %s\n", i, SDL_GetAudioDeviceName (i, 0));
			}
		}
	}
#else
	fprintf(stderr, "[SDL2] Using audio driver %s\n", SDL_GetCurrentAudioDriver());
#endif
	plrSetOptions=sdl2SetOptions;
	plrPlay=sdl2Play;
	plrStop=sdl2Stop;
	return 1;
}

static void sdl2Close(void)
{
	PRINT("%s()\n", __FUNCTION__);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	plrSetOptions=0;
	plrPlay=0;
	plrStop=0;
}

static int sdl2Detect(struct deviceinfo *card)
{
	PRINT("%s()\n", __FUNCTION__);

	/* ao is now created, the above is needed only ONCE */
	card->devtype=&plrSDL2;
	card->port=0;
	card->port2=0;
	card->subtype=-1;
	card->mem=0;
	card->chan=2;

	return 1;
}


struct sounddevice plrSDL2={SS_PLAYER, 0, "SDL2 Player", sdl2Detect, sdl2Init, sdl2Close, 0};

char *dllinfo="driver plrSDL2";
struct linkinfostruct dllextinfo = {.name = "devpsdl2", .desc = "OpenCP Player Device: SDL2 (c) 2011-'22 François Revol", .ver = DLLVERSION, .size = 0};
