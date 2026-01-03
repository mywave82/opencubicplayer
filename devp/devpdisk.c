/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/file.h"
#include "stuff/imsrtns.h"

static const struct plrDriverAPI_t *plrDriverAPI;

static void *devpDiskBuffer;
static struct ringbuffer_t *devpDiskRingBuffer;
static char *devpDiskShadowBuffer;
static osfile *devpDiskFileHandle;
static unsigned char *devpDiskCache;
static unsigned long devpDiskCachelen;
static unsigned long devpDiskCachePos;
static volatile char busy;
static uint32_t devpDiskRate;
static unsigned char stereo;
static unsigned char bit16;
static unsigned char writeerr;

static const struct plrDriver_t plrDiskWriter;

static void devpDiskConsume(int flush)
{
	int pos1, length1, pos2, length2;

	plrDriverAPI->ringbufferAPI->get_tail_samples (devpDiskRingBuffer, &pos1, &length1, &pos2, &length2);

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
		plrDriverAPI->ConvertBufferFromStereo16BitSigned (devpDiskCache + devpDiskCachePos, (int16_t *)devpDiskBuffer + (pos1 << 1), length1, bit16 /* 16bit */, bit16 /* signed follows 16bit */, stereo, 0 /* revstereo */);
		devpDiskCachePos += length1 << ((!!bit16) + (!!stereo));
		if (length2)
		{
			plrDriverAPI->ConvertBufferFromStereo16BitSigned (devpDiskCache + devpDiskCachePos, (int16_t *)devpDiskBuffer + (pos2 << 1), length2, bit16 /* 16bit */, bit16 /* signed follows 16bit */, stereo, 0 /* revstereo */);
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

	plrDriverAPI->ringbufferAPI->tail_consume_samples (devpDiskRingBuffer, length1 + length2);

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
			if ((unsigned)osfile_write(devpDiskFileHandle, devpDiskCache, devpDiskCachePos) != devpDiskCachePos)
			{
				writeerr=1;
			}
		}
		devpDiskCachePos = 0;
	}

	retval = plrDriverAPI->ringbufferAPI->get_tail_available_samples (devpDiskRingBuffer);

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

	plrDriverAPI->ringbufferAPI->head_add_samples (devpDiskRingBuffer, samples);

	busy--;
}

static void devpDiskGetBuffer (void **buf, unsigned int *samples)
{
	int pos1, length1;
	assert (devpDiskRingBuffer);

	plrDriverAPI->ringbufferAPI->get_head_samples (devpDiskRingBuffer, &pos1, &length1, 0, 0);

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
	plrDriverAPI->ringbufferAPI->add_tail_callback_samples (devpDiskRingBuffer, samplesuntil, callback, arg);
}

static int devpDiskPlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int plrbufsize; /* given in ms */
	int buflength;

	stereo = !cpifaceSession->configAPI->GetProfileBool("commandline_s", "m", !cpifaceSession->configAPI->GetProfileBool("devpDisk", "stereo", 1, 1), 1);
	bit16 =  !cpifaceSession->configAPI->GetProfileBool("commandline_s", "8", !cpifaceSession->configAPI->GetProfileBool("devpDisk", "16bit", 1, 1), 1);

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

	plrbufsize = cpifaceSession->configAPI->GetProfileInt2(cpifaceSession->configAPI->SoundSec, "sound", "plrbufsize", 1000, 10);
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
	devpDiskRingBuffer = plrDriverAPI->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, buflength);
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
			cpifaceSession->dirdb->GetName_internalstr (source_file->dirdb_ref, &orig);
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
			if ((devpDiskFileHandle=osfile_open_readwrite(fn, 0, 1)))
				break;
		}
		free (fn);
	}

	if (!devpDiskFileHandle)
	{
		fprintf (stderr, "[devpDisk]: Failed to open output file\n");
		goto error_out;
	}

	{
		unsigned char hdr[0x2C];
		memset(&hdr, 0, sizeof(hdr));

		osfile_write(devpDiskFileHandle, hdr, 0x2C);
	}

	busy=0;

	cpifaceSession->GetMasterSample = plrDriverAPI->GetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrDriverAPI->GetRealMasterVolume;
	cpifaceSession->plrActive = 1;

	return 1;

error_out:
	free (devpDiskBuffer);       devpDiskBuffer = 0;
	free (devpDiskShadowBuffer); devpDiskShadowBuffer = 0;
	free (devpDiskCache);        devpDiskCache = 0;
	if (devpDiskRingBuffer)
	{
		plrDriverAPI->ringbufferAPI->free (devpDiskRingBuffer);
		devpDiskRingBuffer = 0;
	}

	return 0;
}

static void devpDiskStop (struct cpifaceSessionAPI_t *cpifaceSession)
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

	if (!devpDiskFileHandle)
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
		osfile_write (devpDiskFileHandle, devpDiskCache, devpDiskCachePos);
	}

	wavlen = osfile_getpos (devpDiskFileHandle) - 0x2C;
	osfile_setpos (devpDiskFileHandle, 0);

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
	osfile_write (devpDiskFileHandle, &wavhdr, 0x2C);

	osfile_close (devpDiskFileHandle);
	devpDiskFileHandle = 0;
	free(devpDiskBuffer);
	free(devpDiskShadowBuffer);
	free(devpDiskCache);

	if (devpDiskRingBuffer)
	{
		plrDriverAPI->ringbufferAPI->reset (devpDiskRingBuffer);
		plrDriverAPI->ringbufferAPI->free (devpDiskRingBuffer);
		devpDiskRingBuffer = 0;
	}

	devpDiskBuffer = 0;
	devpDiskShadowBuffer = 0;
	devpDiskCache = 0;
	cpifaceSession->plrActive = 0;
}

static void devpDiskPeekBuffer (void **buf1, unsigned int *buf1length, void **buf2, unsigned int *buf2length)
{
	int pos1, length1, pos2, length2;

	plrDriverAPI->ringbufferAPI->get_tail_samples (devpDiskRingBuffer, &pos1, &length1, &pos2, &length2);

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

static void devpDiskGetStats (uint64_t *committed, uint64_t *processed)
{
	plrDriverAPI->ringbufferAPI->get_stats (devpDiskRingBuffer, committed, processed);
}

static const struct plrDevAPI_t devpDisk = {
	devpDiskIdle,
	devpDiskPeekBuffer,
	devpDiskPlay,
	devpDiskGetBuffer,
	devpDiskGetRate,
	devpDiskOnBufferCallback,
	devpDiskCommitBuffer,
	devpDiskPause,
	devpDiskStop,
	0, /* VolRegs */
	0, /* ProcessKey */
	devpDiskGetStats
};

static const struct plrDevAPI_t *dwInit (const struct plrDriver_t *driver, const struct plrDriverAPI_t *DriverAPI)
{
	plrDriverAPI = DriverAPI;

	return &devpDisk;
}

static void dwClose (const struct plrDriver_t *driver)
{
}

static int dwDetect (const struct plrDriver_t *driver)
{
	return 1;
}

static int diskWriterPluginInit (struct PluginInitAPI_t *API)
{
	API->plrRegisterDriver (&plrDiskWriter);

	return errOk;
}

static void diskWriterPluginClose (struct PluginCloseAPI_t *API)
{
	API->plrUnregisterDriver (&plrDiskWriter);
}

static const struct plrDriver_t plrDiskWriter =
{
	"devpDisk",
	"Disk Writer",
	dwDetect,
	dwInit,
	dwClose
};

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "devpdisk", .desc = "OpenCP Player Device: Disk Writer (c) 1994-'26 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99, .PluginInit = diskWriterPluginInit, .PluginClose = diskWriterPluginClose};
