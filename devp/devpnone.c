/** OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * NoSound Player device
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
 */

#include "config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#define DEVPNONE_BUFRATE 44100
#define DEVPNONE_BUFLEN (DEVPNONE_BUFRATE/4) /* 250 ms */
#define DEVPNONE_BUFSIZE (DEVPNONE_BUFLEN<<2) /* stereo + 16bit */

static const struct plrDriverAPI_t *plrDriverAPI;

static void *devpNoneBuffer;
static struct ringbuffer_t *devpNoneRingBuffer;
static struct timespec devpNoneBasetime;
static int devpNonePauseSamples;
static int devpNoneInPause;

static const struct plrDriver_t plrNone;

unsigned int devpNoneIdle (void)
{
	struct timespec now;
	uint_fast32_t rel;
	unsigned int bufpos;

	clock_gettime (CLOCK_MONOTONIC, &now);

	if (now.tv_nsec >= devpNoneBasetime.tv_nsec)
	{
		rel = now.tv_nsec - devpNoneBasetime.tv_nsec;
	} else {
		rel = 1000000000ll + now.tv_nsec - devpNoneBasetime.tv_nsec;
	}
	
	/* rel is now a number between 0 and 999999999 */

	/* find the ideal tail bufpos */
	rel<<=2; /* prescale, to increase precision for the below division */
	bufpos = rel / 90702; /* this is close enough to give us 44100 samples per second */
	bufpos %= DEVPNONE_BUFLEN; /* our bufferlen is 250ms, which a second can divided up to */
	if (bufpos >= DEVPNONE_BUFLEN)
	{
		bufpos = DEVPNONE_BUFLEN - 1;
	}

	/* consume tail based on the time */
	{
		int pos1, length1, pos2, length2;
		int eat;

		plrDriverAPI->ringbufferAPI->get_tail_samples (devpNoneRingBuffer, &pos1, &length1, &pos2, &length2);

		if (length2 == 0) /* no wrap available */
		{
			if (bufpos < pos1)
			{ /* we are out of sync, eat it all!! */
				eat = length1;
			} else {
				eat = bufpos- pos1;
				if (eat > length1)
				{ /* should not be reachable unless we lost sync */
					eat = length1;
				}
			}
		} else {
			if (bufpos > pos1)
			{
				eat = bufpos - pos1;
				/* no need to check against length1, since buffer wraps */
			} else {
				eat = length1;
				if (bufpos < length2)
				{
					eat += bufpos;
				} else {
					eat += length2;
				}
			}
		}

		plrDriverAPI->ringbufferAPI->tail_consume_samples (devpNoneRingBuffer, eat);
		if (eat > devpNonePauseSamples)
		{
			devpNonePauseSamples = 0;
		} else {
			devpNonePauseSamples -= eat;
		}
	}

	/* do we need to insert pause-samples? */
	if (devpNoneInPause)
	{
		int pos1, length1, pos2, length2;
		plrDriverAPI->ringbufferAPI->get_head_bytes (devpNoneRingBuffer, &pos1, &length1, &pos2, &length2);
		memset ((char *)devpNoneBuffer+pos1, 0, length1);
		if (length2)
		{
			memset ((char *)devpNoneBuffer+pos2, 0, length2);
		}
		plrDriverAPI->ringbufferAPI->head_add_pause_bytes (devpNoneRingBuffer, length1 + length2);
		devpNonePauseSamples += (length1 + length2) >> 2; /* stereo + 16bit */
	}

	/* Find the current buffer depth */
	{
		int pos1, length1, pos2, length2;

		plrDriverAPI->ringbufferAPI->get_tail_samples (devpNoneRingBuffer, &pos1, &length1, &pos2, &length2);

		return length1 + length2 - devpNonePauseSamples;
	}
}

static void devpNonePeekBuffer (void **buf1, unsigned int *buf1length, void **buf2, unsigned int *buf2length)
{
	int pos1, length1, pos2, length2;

	plrDriverAPI->ringbufferAPI->get_tail_samples (devpNoneRingBuffer, &pos1, &length1, &pos2, &length2);

	if (length1)
	{
		*buf1 = (char *)devpNoneBuffer + (pos1 << 2); /* stereo + 16bit */
		*buf1length = length1;
		if (length2)
		{
			*buf2 = (char *)devpNoneBuffer + (pos2 << 2); /* stereo + 16bit */
			*buf2length = length2;
		} else {
			*buf2 = 0;
			*buf2length = 0;
		}
	} else {
		*buf1 = 0;
		*buf1length = 0;
		*buf2 = 0;
		*buf2length = 0;
	}
}

static int devpNonePlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	devpNoneInPause = 0;
	devpNonePauseSamples = 0;

	*rate = DEVPNONE_BUFRATE;
	*format = PLR_STEREO_16BIT_SIGNED;

	if (!(devpNoneBuffer=calloc(DEVPNONE_BUFSIZE, 1)))
	{
		return 0;
	}
	if (!(devpNoneRingBuffer = plrDriverAPI->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, DEVPNONE_BUFLEN)))
	{
		free (devpNoneBuffer); devpNoneBuffer = 0;
		return 0;
	}

	clock_gettime (CLOCK_MONOTONIC, &devpNoneBasetime);

	cpifaceSession->GetMasterSample = plrDriverAPI->GetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrDriverAPI->GetRealMasterVolume;
	cpifaceSession->plrActive = 1;

	return 1;
}

static void devpNoneGetBuffer (void **buf, unsigned int *samples)
{
	int pos1, length1;
	assert (devpNoneRingBuffer);
	
	plrDriverAPI->ringbufferAPI->get_head_samples (devpNoneRingBuffer, &pos1, &length1, 0, 0);

	*samples = length1;
	*buf = devpNoneBuffer + (pos1<<2); /* stereo + bit16 */
}

static uint32_t devpNoneGetRate (void)
{
	return DEVPNONE_BUFRATE;
}

static void devpNoneOnBufferCallback (int samplesuntil, void (*callback)(void *arg, int samples_ago), void *arg)
{
	assert (devpNoneRingBuffer);
	plrDriverAPI->ringbufferAPI->add_tail_callback_samples (devpNoneRingBuffer, samplesuntil, callback, arg);
}

static void  devpNoneCommitBuffer (unsigned int samples)
{
	assert (devpNoneRingBuffer);
	plrDriverAPI->ringbufferAPI->head_add_samples (devpNoneRingBuffer, samples);
}

static void devpNonePause(int pause)
{
	assert (devpNoneBuffer);
	devpNoneInPause = pause;
}

static void devpNoneStop (struct cpifaceSessionAPI_t *cpifaceSession)
{
	free(devpNoneBuffer); devpNoneBuffer=0;
	if (devpNoneRingBuffer)
	{
		plrDriverAPI->ringbufferAPI->reset (devpNoneRingBuffer);
		plrDriverAPI->ringbufferAPI->free (devpNoneRingBuffer);
		devpNoneRingBuffer = 0;
	}
	cpifaceSession->plrActive = 0;
}

static void devpNoneGetStats (uint64_t *committed, uint64_t *processed)
{
	plrDriverAPI->ringbufferAPI->get_stats (devpNoneRingBuffer, committed, processed);
}

static const struct plrDevAPI_t devpNone = {
	devpNoneIdle,
	devpNonePeekBuffer,
	devpNonePlay,
	devpNoneGetBuffer,
	devpNoneGetRate,
	devpNoneOnBufferCallback,
	devpNoneCommitBuffer,
	devpNonePause,
	devpNoneStop,
	0,
	0, /* ProcessKey */
	devpNoneGetStats
};

static const struct plrDevAPI_t *qpInit (const struct plrDriver_t *driver, const struct plrDriverAPI_t *DriverAPI)
{
	plrDriverAPI = DriverAPI;

	return &devpNone;
}

static void qpClose (const struct plrDriver_t *driver)
{
}

static int qpDetect (const struct plrDriver_t *driver)
{
	return 1;
}

static int devpNonePluginInit (struct PluginInitAPI_t *API)
{
	API->plrRegisterDriver (&plrNone);

	return errOk;
}

static void devpNonePluginClose (struct PluginCloseAPI_t *API)
{
	API->plrUnregisterDriver (&plrNone);
}

static const struct plrDriver_t plrNone =
{
	"devpNone",
	"Super High Quality Quiet Player",
	qpDetect,
	qpInit,
	qpClose
};

DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "devpnone", .desc = "OpenCP Player Device: None (c) 1994-'25 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99, .PluginInit = devpNonePluginInit, .PluginClose = devpNonePluginClose};
