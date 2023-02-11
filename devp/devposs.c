/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Player device for OSS output
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
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040826   Stian Skjelstad <stian@nixia.no>
 *    -added /dev/mixer support, fixed at 101 steps
 *  -ss040906   Stian Skjelstad <stian@nixia.no>
 *    -release /dev/dsp whenever it is not used.. needed for devsoss
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "cpiface/vol.h"
#include "dev/deviplay.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#ifdef OSS_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format, args...) ((void)0)
#endif

static void *devpOSSBuffer;
static char *devpOSSShadowBuffer;
static struct ringbuffer_t *devpOSSRingBuffer;
static int devpOSSPauseSamples;
static int devpOSSInPause;
static uint32_t devpOSSRate;
static const struct plrDevAPI_t devpOSS;

static const struct plrDriver_t plrOSS;
#define OSS_DEVICE_NAME_MAX 63
static char ossCardName[OSS_DEVICE_NAME_MAX+1];
static char ossMixerName[OSS_DEVICE_NAME_MAX+1];

static int fd_dsp=-1;
static int fd_mixer=-1;
static int stereo;
static int bit16;
static int revstereo;

static volatile int busy=0;

static int mixer_devmask=0;
static struct ocpvolstruct mixer_entries[SOUND_MIXER_NRDEVICES];

static unsigned int devpOSSIdle(void)
{
	int pos1, length1, pos2, length2;
	int result, odelay, tmp;
	int kernlen;
	unsigned int RetVal;

	struct audio_buf_info info;

	if (busy++)
	{
#ifdef OSS_LOCK_DEBUG
		write(2, "devposs: flush(), BUSY set\n", 28);
#endif
		busy--;
		return 0;
	}

	debug_printf ("devpOSSIdle PRE: tail:%d processing:%d head:%d\n", ringbuffer_get_tail_available_samples (devpOSSRingBuffer), ringbuffer_get_processing_available_samples(devpOSSRingBuffer), ringbuffer_get_head_available_samples(devpOSSRingBuffer));

/* Update the ringbuffer-tail START */
#ifdef SNDCTL_DSP_GETODELAY
	/* update kernlen stuff before we check if cachelen is set furter down */
	if (ioctl(fd_dsp, SNDCTL_DSP_GETODELAY, &odelay))
	{
		debug_printf ("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETODELAY, &count): %s\n", strerror (errno));
		busy--;
		return 0;
	}
	if (odelay<0)
	{
		debug_printf ("devposs: buggy nvidia driver?\n");
	} else {
		debug_printf ("devposs: ODELAY=%d\n", odelay);
	}
#else
	{
		struct count_info i;
		if (ioctl(fd_dsp, SNDCTL_DSP_GETOPTR, &i))
		{
			debug_printf ("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETOPTR, &i): %s\n", strerror (errno));
			busy--;
			return 0;
		}
		odelay=i.bytes;
		debug_printf ("devposs: SNDCTL_DSP_GETOPTR=%d\n", i.bytes);
	}
#endif
	odelay=abs(odelay); /* This is because of old nvidia sound-drivers and probably others too */
	odelay >>= (!!bit16) + (!!stereo);

	kernlen = ringbuffer_get_tail_available_samples (devpOSSRingBuffer);
	if (odelay>kernlen)
	{
		debug_printf ("devposs: flush() kernelbug found!\n");
		odelay=kernlen;
	} else if ((odelay<kernlen))
	{
		ringbuffer_tail_consume_samples (devpOSSRingBuffer, kernlen - odelay);
		if (devpOSSPauseSamples)
		{
			if ((kernlen - odelay) > devpOSSPauseSamples)
			{
				devpOSSPauseSamples = 0;
			} else {
				devpOSSPauseSamples -= (kernlen - odelay);
			}
		}
	}
/* Update the ringbuffer-tail DONE */

/* do we need to insert pause-samples? START */
	if (devpOSSInPause)
	{
		ringbuffer_get_head_bytes (devpOSSRingBuffer, &pos1, &length1, &pos2, &length2);
		bzero ((char *)devpOSSBuffer+pos1, length1);
		if (length2)
		{
			bzero ((char *)devpOSSBuffer+pos2, length2);
		}
		ringbuffer_head_add_bytes (devpOSSRingBuffer, length1 + length2);
		devpOSSPauseSamples += (length1 + length2) >> 2; /* stereo + 16bit */
	}
/* do we need to insert pause-samples? DONE */

/* Move data from ringbuffer-head into processing/kernel START */
	if (ioctl(fd_dsp, SNDCTL_DSP_GETOSPACE, &info))
	{
		debug_printf ("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETOSPACE, &info): %s\n", strerror (errno));
		busy--;
		return 0;
	}
	debug_printf("devposs: GETOSPACE.bytes=%d\n", info.bytes);
	if (!info.bytes)
	{
		busy--;
		return 0;
	}
	tmp = info.bytes >> ((!bit16) + (!stereo));

	ringbuffer_get_processing_samples (devpOSSRingBuffer, &pos1, &length1, &pos2, &length2);
	if (tmp < length1)
	{
		length1 = tmp;
		length2 = 0;
	} else if (tmp < (length1 + length2))
	{
		length2 = tmp - length1;
	}

	while (length1)
	{
		if (devpOSSShadowBuffer)
		{
			plrConvertBufferFromStereo16BitSigned (devpOSSShadowBuffer, (int16_t *)devpOSSBuffer + (pos1<<1), length1, bit16 /* 16bit */, bit16 /* signed follows 16bit */, stereo, 0 /* revstereo */);
			result=write(fd_dsp, devpOSSShadowBuffer, length1<<((!!stereo)+(!!bit16)));
			if (result > 0)
			{
				result >>= (!!stereo) + (!!bit16);
			}
		} else {
			result=write(fd_dsp, (uint8_t *)devpOSSBuffer + (pos1<<2), length1<<2 /* stereo + bit16 */);
			if (result > 0)
			{
				result >>= 2; /* stereo + bit16 */
			}
		}
		if (result > 0)
		{
			ringbuffer_processing_consume_samples (devpOSSRingBuffer, result);
		}
		if (result < 0)
		{
			debug_printf ("devposs: flush() write: %s\n", strerror (errno));
			break;
		}
		if (result != length1)
		{
			break;
		}
		length1 = length2;
		length2 = 0;
		pos1 = pos2;
		pos2 = 0;
	}
/* Move data from ringbuffer-head into processing/kernel STOP */

	debug_printf ("devpOSSIdle POST: tail:%d processing:%d head:%d\n", ringbuffer_get_tail_available_samples (devpOSSRingBuffer), ringbuffer_get_processing_available_samples(devpOSSRingBuffer), ringbuffer_get_head_available_samples(devpOSSRingBuffer));

	ringbuffer_get_tailandprocessing_samples (devpOSSRingBuffer, &pos1, &length1, &pos2, &length2);

	busy--;

	RetVal = length1 + length2;
	if (devpOSSPauseSamples >= RetVal)
	{
		return 0;
	}
	return RetVal - devpOSSPauseSamples;

}

static void devpOSSPeekBuffer (void **buf1, unsigned int *buf1length, void **buf2, unsigned int *buf2length)
{
	int pos1, length1, pos2, length2;

	ringbuffer_get_tailandprocessing_samples (devpOSSRingBuffer, &pos1, &length1, &pos2, &length2);

	if (length1)
	{
		*buf1 = (uint8_t *)devpOSSBuffer + (pos1 << 2); /* stereo + 16bit */
		*buf1length = length1;
		if (length2)
		{
			*buf2 = (uint8_t *)devpOSSBuffer + (pos2 << 2); /* stereo + 16bit */
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

static void devpOSSGetBuffer (void **buf, unsigned int *samples)
{
	int pos1, length1;
	assert (devpOSSRingBuffer);

	debug_printf("%s()\n", __FUNCTION__);

	ringbuffer_get_head_samples (devpOSSRingBuffer, &pos1, &length1, 0, 0);

	*samples = length1;
	*buf = (uint8_t *)devpOSSBuffer + (pos1<<2); /* stereo + bit16 */
}

static uint32_t devpOSSGetRate (void)
{
	return devpOSSRate;
}

static void devpOSSOnBufferCallback (int samplesuntil, void (*callback)(void *arg, int samples_ago), void *arg)
{
	assert (devpOSSRingBuffer);
	ringbuffer_add_tail_callback_samples (devpOSSRingBuffer, samplesuntil, callback, arg);
}

static void devpOSSCommitBuffer (unsigned int samples)
{
	debug_printf ("%s(%u)\n", __FUNCTION__, samples);

	debug_printf ("devpOSSCommitBuffer PRE: tail:%d processing:%d head:%d\n", ringbuffer_get_tail_available_samples (devpOSSRingBuffer), ringbuffer_get_processing_available_samples(devpOSSRingBuffer), ringbuffer_get_head_available_samples(devpOSSRingBuffer));

	ringbuffer_head_add_samples (devpOSSRingBuffer, samples);

	debug_printf ("devpOSSCommitBuffer POST: tail:%d processing:%d head:%d\n", ringbuffer_get_tail_available_samples (devpOSSRingBuffer), ringbuffer_get_processing_available_samples(devpOSSRingBuffer), ringbuffer_get_head_available_samples(devpOSSRingBuffer));
}

static void devpOSSPause (int pause)
{
	assert (devpOSSBuffer);
	devpOSSInPause = pause;
}

static int devpOSSPlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int plrbufsize, buflength;
	struct audio_buf_info info;
	int tmp;

	devpOSSInPause = 0;
	devpOSSPauseSamples = 0;

	*format = PLR_STEREO_16BIT_SIGNED;

	if ((fd_dsp=open(ossCardName, O_WRONLY|O_NONBLOCK))<0)
	{
		fprintf(stderr, "devposs: open(%s, O_WRONLY|O_NONBLOCK): %s\n", ossCardName, strerror(errno));
#ifdef OSS_DEBUG
		sleep(3);
#endif
		return 0;
	}
	if (fcntl(fd_dsp, F_SETFD, FD_CLOEXEC)<0)
	{
		perror("devposs: fcntl(fd_dsp, F_SETFD, FD_CLOEXEC)");
	}
#ifdef OSS_DEBUG
 #if defined(OSS_GETVERSION)
	if (ioctl(fd_dsp, OSS_GETVERSION, &tmp)<0)
	{
		tmp=0;
	}
	if (tmp<361)
	{
		tmp= ((tmp&0xf00)<<8) | ((tmp&0xf0)<<4) | (tmp&0xf);
	}

	fprintf(stderr, "devposs: compiled agains OSS version %d.%d.%d, version %d.%d.%d detected\n", (SOUND_VERSION&0xff0000)>>16, (SOUND_VERSION&0xff00)>>8, SOUND_VERSION&0xff, (tmp&0xff0000)>>16, (tmp&0xff00)>>8, tmp&0xff);
 #elif defined(SOUND_VERSION)
	fprintf(stderr, "devposs: compiled agains OSS version %d.%d.%d\n", (SOUND_VERSION&0xff0000)>>16, (SOUND_VERSION&0xff00)>>8, SOUND_VERSION&0xff);
 #endif
#endif

	if (ioctl(fd_dsp, SNDCTL_DSP_GETFMTS, &tmp)<0)
	{
		perror("devposs: ioctl(fd_dsp, SNDCTL_DSP_GETFMTS, &tmp)");
		close (fd_dsp);
		fd_dsp=-1;
		return 0;
	}
	if (tmp & AFMT_S16_NE)
	{
		tmp = AFMT_S16_NE;
		if (ioctl(fd_dsp, SNDCTL_DSP_SETFMT, &tmp)>=0)
		{
			bit16 = 1;
		} else {
			tmp = AFMT_U8;
			if (ioctl(fd_dsp, SNDCTL_DSP_SETFMT, &tmp)>=0)
			{
				bit16 = 0;
			} else {
				perror("devposs: ioctl(fd_dsp, SNDCTL_DSP_SETFMT, {AFMT_S16_NE, AFMT_U8})");
				close (fd_dsp);
				fd_dsp=-1;
				return 0;
			}
		}
	}

	tmp=2;
	if (ioctl(fd_dsp, SNDCTL_DSP_CHANNELS, &tmp)<0)
	{
		perror("devposs: ioctl(fd_dsp, SNDCTL_DSP_CHANNELS, tmp)");
	}
	stereo=(tmp==2);

	tmp = *rate;
	if (tmp <= 0)
	{
		tmp = 44100;
	}
	if (ioctl(fd_dsp, SNDCTL_DSP_SPEED, &tmp)<0)
	{
		perror("devposs: ioctl(fd_dsp, SNDCTL_DSP_SPEED, rate)");
	}
	*rate = tmp;
	devpOSSRate = *rate;

	plrbufsize = cpifaceSession->configAPI->GetProfileInt2 (cpifaceSession->configAPI->SoundSec, "sound", "plrbufsize", 200, 10);
	/* clamp the plrbufsize to be atleast 150ms and below 1000 ms */
	if (plrbufsize < 150)
	{
		plrbufsize = 150;
	}
	if (plrbufsize > 1000)
	{
		plrbufsize = 1000;
	}
	buflength = *rate * plrbufsize / 1000;

	/* our buffer should be bigger than the kernel-size one ( + 50ms) */
	if (!ioctl(fd_dsp, SNDCTL_DSP_GETOSPACE, &info))
	{
		if (buflength < ((info.bytes >> ((!!bit16) + (!!stereo))) + (*rate) * 50 / 1000))
		{
			buflength = ((info.bytes >> ((!!bit16) + (!!stereo))) + (*rate) * 50 / 1000);
		}
	}

	if (!(devpOSSBuffer=calloc (buflength, 4)))
	{
		fprintf (stderr, "alsaPlay(): calloc() failed\n");
		close (fd_dsp);
		fd_dsp=-1;
		return 0;
	}

	if ((!bit16) || (!stereo) || (revstereo))
	{ /* we need to convert the signal format... */
		devpOSSShadowBuffer = malloc ( buflength << ((!!bit16) + (!!stereo)));
		if (!devpOSSShadowBuffer)
		{
			fprintf (stderr, "ossPlay(): malloc() failed #2\n");
			free (devpOSSBuffer);
			devpOSSBuffer = 0;
			close (fd_dsp);
			fd_dsp=-1;
			return 0;
		}
	}

	if (!(devpOSSRingBuffer = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, buflength)))
	{
		free (devpOSSBuffer);
		devpOSSBuffer = 0;
		free (devpOSSShadowBuffer);
		devpOSSShadowBuffer=0;
		close (fd_dsp);
		fd_dsp=-1;
		return 0;
	}

	cpifaceSession->GetMasterSample = plrGetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrGetRealMasterVolume;
	cpifaceSession->plrActive = 1;

	return 1;
}

static void devpOSSStop (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (fd_dsp<0)
	{
		return;
	}

	free (devpOSSBuffer);
	devpOSSBuffer = 0;
	free (devpOSSShadowBuffer);
	devpOSSShadowBuffer=0;

	if (devpOSSRingBuffer)
	{
		ringbuffer_reset (devpOSSRingBuffer);
		ringbuffer_free (devpOSSRingBuffer);
		devpOSSRingBuffer = 0;
	}

	close (fd_dsp);
	fd_dsp=-1;

	cpifaceSession->plrActive = 0;
}

static int ossDetect (const struct plrDriver_t *driver)
{
#ifdef HAVE_SYS_SOUNDCARD_H
	struct stat st;
#endif
	int tmp;
	char *temp;

	snprintf (ossCardName, sizeof(ossCardName), "%s", cfGetProfileString("devpOSS", "path", "/dev/dsp"));
	snprintf (ossMixerName, sizeof(ossMixerName), "%s", cfGetProfileString("devpOSS", "mixer", "/dev/mixer"));

	if ((temp=getenv("DSP")))
	{
		debug_printf("devposs: $DSP found\n");
		snprintf (ossCardName, sizeof (ossCardName), "%s", temp);
	}
	if ((temp=getenv("MIXER")))
	{
		debug_printf("devposs: $MIXER found\n");
		snprintf (ossMixerName, sizeof (ossMixerName), "%s", temp);
	}
#ifdef HAVE_SYS_SOUNDCARD_H
/* this test will fail with liboss */
	if (stat(ossCardName, &st))
	{
		debug_printf ("devposs: stat(%s, &st): %s\n", ossCardName, strerror(errno));
		return 0;
	}
#endif

	/* test if basic OSS functions exists */
	if ((fd_dsp=open(ossCardName, O_WRONLY|O_NONBLOCK))<0) /* no need to FD_SETFD, since it will be closed soon */
	{
		if ((errno==EAGAIN)||(errno==EINTR))
		{
			debug_printf ("devposs: OSS detected (EAGAIN/EINTR)\n");
			return 1;
		}
		debug_printf ("devposs: open(%s, O_WRONLY|O_NONBLOCK): %s\n", ossCardName, strerror(errno));
		return 0;
	}

#if defined(OSS_GETVERSION)
	if (ioctl(fd_dsp, OSS_GETVERSION, &tmp)<0)
	{
		debug_printf ("devposs: (warning) ioctl(fd_dsp, OSS_GETVERSION, &tmp): %s\n", strerror (errno));
	}
#endif

	debug_printf ("devposs: OSS detected\n");
	close(fd_dsp);
	fd_dsp=-1;

	return 1; /* device exists, so we are happy.. can do better tests later */
}

static const struct plrDevAPI_t *ossInit (const struct plrDriver_t *driver)
{
	mixer_devmask=0;

	if (!ossMixerName[0])
	{
		fd_mixer=-1;
	} else if ((fd_mixer=open(ossMixerName, O_RDWR|O_NONBLOCK))<0)
	{
		debug_printf ("devposs: open(%s, O_RDWR|O_NONBLOCK): %s\n", ossMixerName, strerror(errno));
#ifdef OSS_DEBUG
		sleep(3);
#endif
	} else { // if (fd_mixer>=0)
		int i;
		const char *labels[]=SOUND_DEVICE_LABELS; /* const data should be statically allocated, while the refs we don't care about */
		if (fcntl(fd_mixer, F_SETFD, FD_CLOEXEC)<0)
			perror("devposs: fcntl(fd_mixer, F_SETFD, FD_CLOEXEC)");
		if (ioctl(fd_mixer, SOUND_MIXER_READ_DEVMASK, &mixer_devmask))
		{
			debug_printf ("devposs: ioctl(fd_mixer, SOUND_MIXER_READ_DEVMASK): %s\n", strerror (errno));
//no_mixer:
			close(fd_mixer);
			fd_mixer=-1;
			mixer_devmask=0;

		}
		for (i=0;i<SOUND_MIXER_NRDEVICES;i++)
		{
			if (mixer_devmask&(1<<i))
			{
				if (ioctl(fd_mixer, MIXER_READ(i), &mixer_entries[i].val))
				{
					debug_printf ("devposs: ioctl(fd_mixer, MIXER_READ(i), &val): %s\n", strerror (errno));
					mixer_entries[i].val=0;
				} else {
					mixer_entries[i].val=((mixer_entries[i].val&255)+(mixer_entries[i].val>>8))>>1; /* we don't vare about balance in current version */
				}
			} else {
				mixer_entries[i].val=0;
			}
			mixer_entries[i].min=0;
			mixer_entries[i].max=100;
			mixer_entries[i].step=1;
			mixer_entries[i].log=0;
			mixer_entries[i].name=labels[i];
		}
	}

	revstereo = cfGetProfileBool("devpOSS", "revstereo", 0, 0);

	return &devpOSS;
}

static void ossClose (const struct plrDriver_t *driver)
{
	if (fd_dsp>=0)
		close(fd_dsp);
	fd_dsp=-1;
	if (fd_mixer>=0)
		close(fd_mixer);
	fd_mixer=-1;
}

static int volossGetNumVolume(void)
{
	if (fd_mixer<0)
		return 0;
	return SOUND_MIXER_NRDEVICES;
}

static int volossGetVolume(struct ocpvolstruct *v, int n)
{
	if ((fd_mixer>=0)&&(n<SOUND_MIXER_NRDEVICES)&&(n>=0))
	if (mixer_devmask&(1<<n))
	{
		memcpy(v, &mixer_entries[n], sizeof(mixer_entries[n]));
		return 1;
	}
	return 0;
}

static int volossSetVolume(struct ocpvolstruct *v, int n)
{
	if ((fd_mixer>=0)&&(n<SOUND_MIXER_NRDEVICES)&&(n>=0))
	if (mixer_devmask&(1<<n))
	{
		int i=mixer_entries[n].val=v->val;
		i+=i<<8;
		if (ioctl(fd_mixer, MIXER_WRITE(n), &i))
		{
			debug_printf ("devposs: ioctl(fd_mixer, MIXER_WRITE(n), &val): %s\n", strerror (errno));
		}
		return 1;
	}
	return 0;
}

static int ossPluginInit (struct PluginInitAPI_t *API)
{
	API->plrRegisterDriver (&plrOSS);

	return errOk;
}

static void ossPluginClose (struct PluginCloseAPI_t *API)
{
	API->plrUnregisterDriver (&plrOSS);
}

static struct ocpvolregstruct voloss={volossGetNumVolume, volossGetVolume, volossSetVolume};

static const struct plrDevAPI_t devpOSS = {
	devpOSSIdle,
	devpOSSPeekBuffer,
	devpOSSPlay,
	devpOSSGetBuffer,
	devpOSSGetRate,
	devpOSSOnBufferCallback,
	devpOSSCommitBuffer,
	devpOSSPause,
	devpOSSStop,
	&voloss,
	0 /* ProcessKey */
};

static const struct plrDriver_t plrOSS =
{
	"devpOSS",
	"OSS player",
	ossDetect,
	ossInit,
	ossClose
};

DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "devposs", .desc = "OpenCP Player Device: OSS (c) 2004-'23 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99, .PluginInit = ossPluginInit, .PluginClose = ossPluginClose};
