/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Player device for WAV output
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
 *    -added _dllinfo record
 *  -kb981118   Tammo Hinrichs <opencp@gmx.net>
 *    -reduced max buffer size to 32k to avoid problems with the
 *     wave/mp3 player
 *  -doj20020901 Dirk Jagdmann <doj@cubic.org>
 *    -removed pwProcessKey()
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/devigen.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "stuff/imsrtns.h"

extern struct sounddevice plrDiskWriter;

static void *devpDiskBuffer;
static struct ringbuffer_t *devpDiskRingBuffer;
static char *devpDiskShadowBuffer;
static int devpDiskFileHandle = -1;
static unsigned char *devpDiskCache;
static unsigned long devpDiskCachelen;
static unsigned long devpDiskCachePos;
static volatile char busy;
static uint32_t devpDiskRate;
static unsigned char stereo;
static unsigned char bit16;
static unsigned char writeerr;

static void devpDiskConsume(int flush)
{
	int pos1, length1, pos2, length2;

	ringbuffer_get_tail_samples (devpDiskRingBuffer, &pos1, &length1, &pos2, &length2);

#define BUFFER_TO_KEEP 2048  // for visuals

	if (!flush)
	{
		if ((length1 + length2) <= BUFFER_TO_KEEP)
		{
			return;
		}
		if (length2)
		{
			if (length2 < BUFFER_TO_KEEP)
			{
				length1 = (length2 + length1) - BUFFER_TO_KEEP;
				length2 = 0;
			} else {
				length2 = length2 - BUFFER_TO_KEEP;
			}
		} else {
			length1 = length1 - BUFFER_TO_KEEP;
		}
	}

	if (devpDiskShadowBuffer)
	{
		plrConvertBufferFromStereo16BitSigned (devpDiskCache + devpDiskCachePos, (int16_t *)devpDiskBuffer + (pos1 << 1), length1, bit16 /* 16bit */, bit16 /* signed follows 16bit */, stereo, 0 /* revstereo */);
		devpDiskCachePos += length1 << ((!!bit16) + (!!stereo));
		if (length2)
		{
			plrConvertBufferFromStereo16BitSigned (devpDiskCache + devpDiskCachePos, (int16_t *)devpDiskBuffer + (pos2 << 1), length2, bit16 /* 16bit */, bit16 /* signed follows 16bit */, stereo, 0 /* revstereo */);
			devpDiskCachePos += length2 << ((!!bit16) + (!!stereo));
		}
	} else {
		memcpy(devpDiskCache + devpDiskCachePos, (uint8_t *)devpDiskBuffer + (pos1 << 2), length1 << 2);
		devpDiskCachePos += (length1 << 2); /* stereo + bit16 */
		if (length2)
		{
			memcpy(devpDiskCache + devpDiskCachePos, (uint8_t *)devpDiskBuffer + (pos2 << 2), length2 << 2);
			devpDiskCachePos += (length2 << 2); /* stereo + bit16 */
		}
	}

	ringbuffer_tail_consume_samples (devpDiskRingBuffer, length1 + length2);

	assert (devpDiskCachePos <= devpDiskCachelen);
}

static unsigned int devpDiskIdle(void)
{
	unsigned int retval;

	busy++;
	if (busy != 1)
	{
		busy--;
		return 0;
	}

	devpDiskConsume (0);

	if (devpDiskCachePos > (devpDiskCachelen/2))
	{
		if (!writeerr)
		{
			if (bit16)
			{
				int i, j = devpDiskCachePos/2;
				uint16_t *d = (uint16_t *)devpDiskCache;
				for (i=0;i<j;i++)
				{
					d[i]=uint16_little(d[i]);
				}
			}
rewrite:
			if ((unsigned)write(devpDiskFileHandle, devpDiskCache, devpDiskCachePos) != devpDiskCachePos)
			{
				if (errno==EAGAIN)
					goto rewrite;
				if (errno==EINTR)
					goto rewrite;
				writeerr=1;
			}
		}
		devpDiskCachePos = 0;
	}

	retval = ringbuffer_get_tail_available_samples (devpDiskRingBuffer);

	busy--;

	return retval;
}

static void devpDiskCommitBuffer (unsigned int samples)
{
	busy++;

	if (!samples)
	{
		return;
	}

	ringbuffer_head_add_samples (devpDiskRingBuffer, samples);

	busy--;
}

static void devpDiskGetBuffer (void **buf, unsigned int *samples)
{
	int pos1, length1;
	assert (devpDiskRingBuffer);

	ringbuffer_get_head_samples (devpDiskRingBuffer, &pos1, &length1, 0, 0);

	*samples = length1;
	*buf = (uint8_t *)devpDiskBuffer + (pos1<<2); /* stereo + bit16 */
}

static uint32_t devpDiskGetRate (void)
{
	return devpDiskRate;
}

static void devpDiskOnBufferCallback (int samplesuntil, void (*callback)(void *arg, int samples_ago), void *arg)
{
	assert (devpDiskRingBuffer);
	ringbuffer_add_tail_callback_samples (devpDiskRingBuffer, samplesuntil, callback, arg);
}

static int devpDiskPlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int plrbufsize; /* given in ms */
	int buflength;

	stereo = !cfGetProfileBool("commandline_s", "m", !cfGetProfileBool("devpDisk", "stereo", 1, 1), 1);
	bit16 =  !cfGetProfileBool("commandline_s", "8", !cfGetProfileBool("devpDisk", "16bit", 1, 1), 1);

	if (*rate == 0)
	{
		*rate = 44100;
	}
	if (*rate<5000)
	{
		*rate=5000;
	}
	if (*rate>96000)
	{
		*rate=96000;
	}
	devpDiskRate = *rate;
	*format=PLR_STEREO_16BIT_SIGNED; // Fixed

	plrbufsize = cfGetProfileInt2(cfSoundSec, "sound", "plrbufsize", 1000, 10);
	/* clamp the plrbufsize to be atleast 1000ms and below 2000 ms */
	if (plrbufsize < 1000)
	{
		plrbufsize = 1000;
	}
	if (plrbufsize > 2000)
	{
		plrbufsize = 2000;
	}
	buflength = devpDiskRate * plrbufsize / 1000;

	devpDiskBuffer=calloc(buflength, 4);
	if (!devpDiskBuffer)
	{
		fprintf (stderr, "[devpDisk]: malloc() failed #1\n");
		goto error_out;
	}
	devpDiskRingBuffer = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, buflength);
	if (!devpDiskRingBuffer)
	{
		fprintf (stderr, "[devpDisk]: ringbuffer_new_samples() failed\n");
		goto error_out;
	}
	if ((!bit16) || (!stereo))
	{ /* we need to convert the signal format... */
		devpDiskShadowBuffer = malloc ( buflength << ((bit16) + (stereo)));
		if (!devpDiskShadowBuffer)
		{
			fprintf (stderr, "[devpDisk]: malloc() failed #2\n");
			goto error_out;
		}
	}

	writeerr=0;

	devpDiskCachelen = 12*devpDiskRate; /* 3 seconds */
	devpDiskCachePos=0;
	devpDiskCache=calloc(devpDiskCachelen, 1);
	if (!devpDiskCache)
	{
		fprintf (stderr, "[devpDisk]: malloc() failed #3\n");
		goto error_out;
	}

	{
		const char *orig;
		char *fn;
		int i;
		if (source_file)
		{
			dirdbGetName_internalstr (source_file->dirdb_ref, &orig);
			i = 0;
		} else {
			orig = "CPOUT";
			i = 1;
		}
		fn = malloc (strlen (orig) + 10);
		for (; i<1000; i++)
		{
			if (i)
			{
				sprintf (fn, "%s-%03d.wav", orig, i);
			} else {
				sprintf (fn, "%s.wav", orig);
			}
			if ((devpDiskFileHandle=open(fn, O_WRONLY|O_CREAT|O_EXCL, S_IREAD|S_IWRITE))>=0)
				break;
		}
		free (fn);
	}

	if (devpDiskFileHandle<0)
	{
		fprintf (stderr, "[devpDisk]: Failed to open output file\n");
		goto error_out;
	}

	while (1)
	{
		unsigned char hdr[0x2C];
		memset(&hdr, 0, sizeof(hdr));

		if (write(devpDiskFileHandle, hdr, 0x2C)>=0)
			break;
		if (errno==EAGAIN)
			continue;
		if (errno==EINTR)
			continue;
		break;
	}

	busy=0;

	cpifaceSession->GetMasterSample = plrGetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrGetRealMasterVolume;

	return 1;

error_out:
	free (devpDiskBuffer);       devpDiskBuffer = 0;
	free (devpDiskShadowBuffer); devpDiskShadowBuffer = 0;
	free (devpDiskCache);        devpDiskCache = 0;
	if (devpDiskRingBuffer)
	{
		ringbuffer_free (devpDiskRingBuffer);
		devpDiskRingBuffer = 0;
	}

	return 0;
}

static void devpDiskStop(void)
{
	uint32_t wavlen;
	struct __attribute__((packed))
	{
		char riff[4];
		uint32_t len24;
		char wave[4];
		char fmt_[4];
		uint32_t chlen;
		uint16_t form;
		uint16_t chan;
		uint32_t rate;
		uint32_t datarate;
		uint16_t bpsmp;
		uint16_t bits;
		char data[4];
		uint32_t wavlen;
	} wavhdr;

	if (devpDiskFileHandle < 0)
	{
		return;
	}

	devpDiskConsume (1);

	if (!writeerr)
	{
		if (bit16)
		{
			int i, j = devpDiskCachePos/2;
			uint16_t *d = (uint16_t *)devpDiskCache;
			for (i=0;i<j;i++)
			{
				d[i]=uint16_little(d[i]);
			}
		}
rewrite:
		if (write (devpDiskFileHandle, devpDiskCache, devpDiskCachePos)<0)
		{
			if (errno==EINTR)
				goto rewrite;
			if (errno==EAGAIN)
				goto rewrite;
		}
	}

	wavlen = lseek (devpDiskFileHandle, 0, SEEK_CUR)-0x2C;

	lseek (devpDiskFileHandle, 0, SEEK_SET);

	memcpy(wavhdr.riff, "RIFF", 4);
	memcpy(wavhdr.wave, "WAVE", 4);
	memcpy(wavhdr.fmt_, "fmt ", 4);
	memcpy(wavhdr.data, "data", 4);
	wavhdr.chlen=uint32_little(0x10);
	wavhdr.form=uint16_little(1);
	wavhdr.chan=uint16_little(1<<stereo);
	wavhdr.rate=uint32_little(devpDiskRate);
	wavhdr.bits=uint16_little(8<<bit16);
	wavhdr.bpsmp=uint16_little((1<<stereo)*(8<<bit16)/8);
	wavhdr.datarate=uint32_little(((1<<stereo)*(8<<bit16)/8)*devpDiskRate);
	wavhdr.wavlen=uint32_little(wavlen);
	wavhdr.len24=uint32_little(wavlen+0x24);
rewrite2:
	if (write (devpDiskFileHandle, &wavhdr, 0x2C)<0)
	{
		if (errno==EINTR)
			goto rewrite2;
		if (errno==EAGAIN)
			goto rewrite2;
	}

	lseek (devpDiskFileHandle, 0, SEEK_END);
reclose:
	if (close (devpDiskFileHandle) < 0)
	{
		if (errno==EINTR)
			goto reclose;
	}
	free(devpDiskBuffer);
	free(devpDiskShadowBuffer);
	free(devpDiskCache);

	if (devpDiskRingBuffer)
	{
		ringbuffer_reset (devpDiskRingBuffer);
		ringbuffer_free (devpDiskRingBuffer);
		devpDiskRingBuffer = 0;
	}

	devpDiskBuffer = 0;
	devpDiskShadowBuffer = 0;
	devpDiskCache = 0;
	devpDiskFileHandle = -1;
}

static void devpDiskPeekBuffer (void **buf1, unsigned int *buf1length, void **buf2, unsigned int *buf2length)
{
	int pos1, length1, pos2, length2;

	ringbuffer_get_tail_samples (devpDiskRingBuffer, &pos1, &length1, &pos2, &length2);

	if (length1)
	{
		*buf1 = (uint8_t *)devpDiskBuffer + (pos1 << 2); /* stereo + 16bit */
		*buf1length = length1;
		if (length2)
		{
			*buf2 = (uint8_t *)devpDiskBuffer + (pos2 << 2); /* stereo + 16bit */
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
static void devpDiskPause (int pause)
{ /* a no-op on devpDisk */
}

static const struct plrAPI_t devpDisk = {
	devpDiskIdle,
	devpDiskPeekBuffer,
	devpDiskPlay,
	devpDiskGetBuffer,
	devpDiskGetRate,
	devpDiskOnBufferCallback,
	devpDiskCommitBuffer,
	devpDiskPause,
	devpDiskStop,
	0
};

static int dwInit(const struct deviceinfo *d)
{
	plrAPI=&devpDisk;
	return 1;
}

static void dwClose(void)
{
	if (plrAPI  == &devpDisk)
	{
		plrAPI = 0;
	}
}

static int dwDetect(struct deviceinfo *card)
{
	card->devtype=&plrDiskWriter;
	card->port=-1;
	card->port2=-1;
	card->subtype=-1;
	card->mem=0;
	card->chan=2;

	return 1;
}

struct sounddevice plrDiskWriter={SS_PLAYER, 0, "Disk Writer", dwDetect, dwInit, dwClose, 0};

const char *dllinfo = "driver plrDiskWriter";
const struct linkinfostruct dllextinfo = {.name = "devpdisk", .desc = "OpenCP Player Device: Disk Writer (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION};
