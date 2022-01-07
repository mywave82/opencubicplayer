/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Samples device for OSS input
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
 *  -ss040906   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "dev/devigen.h"
#include "dev/imsdev.h"
#include "dev/sampler.h"
#include "stuff/imsrtns.h"
#ifdef SMP_DEBUG
#include "stuff/poutput.h"
#endif

#define REVSTEREO 1

extern struct sounddevice smpOSS;
static struct deviceinfo currentcard;

static int fd_dsp=-1, fd_mixer=-1;
static unsigned char *sampbuf;
static int buflen, bufpos;

static int stereo;
static int bit16;

static int igain;

static int getbufpos(void)
{
	while (1)
	{
		int target, result;
		if (bufpos==buflen)
			bufpos=0;
		target=buflen-bufpos;
		result=read(fd_dsp, sampbuf+bufpos, target);
		if (result>0)
			bufpos+=result;
		if (result!=target) /* We don't care if we overrun buffer */
			break;
	}
	return bufpos;
}

static void SetOptions(int rate, int opt)
{
	int tmp;

	int newopt;

	int fd;

	if (fd_dsp<0)
	{
		if ((fd=open(currentcard.path, O_RDONLY|O_NONBLOCK))<0)
		{
#ifdef OSS_DEBUG
			fprintf(stderr, "devsoss: open(%s, O_RDONLY|O_NONBLOCK): %s\n", currentcard.path, strerror(errno));
#endif
			smpRate=rate; /* damn it... device currently busy */
			smpOpt=opt;
			return;
		}
	} else
		fd=fd_dsp;

	if (opt&SMP_16BIT)
		tmp=16;
	else
		tmp=8;
	if (ioctl(fd, SOUND_PCM_WRITE_BITS, &tmp)<0)
	{
#ifdef OSS_DEBUG
		perror("devsoss: ioctl(fd_dsp, SOUND_PCM_WRITE_BITS, &tmp)");
#endif
	}
	if ((bit16=(tmp==16)))
		newopt = SMP_16BIT | SMP_SIGNEDOUT;
	else
		newopt = 0;

	if (opt&SMP_STEREO)
		tmp=2;
	else
		tmp=1;
	if (ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &tmp)<0)
	{
#ifdef OSS_DEBUG
		perror("devsoss: ioctl(fd_dsp, SOUND_PCM_WRITE_CHANNELS, tmp)");
#endif
	}
	if ((stereo=(tmp==2)))
		newopt |= SMP_STEREO;

	if (ioctl(fd, SOUND_PCM_WRITE_RATE, &rate)<0)
	{
#ifdef OSS_DEBUG
		perror("devposs: ioctl(fd_dsp, SOUND_PCM_WRITE_RATE, rate)");
#endif
	}

	if (currentcard.opt&REVSTEREO)
		newopt|=SMP_REVERSESTEREO;

	smpRate=rate;
	smpOpt=newopt;

	if (fd_dsp<0) /* nasty hack */
		close(fd);
}

static void ossSetSource(int src)
{
	int oss_src;
	if (fd_mixer<0)
		return;
	switch (src)
	{
		case SMP_MIC:
			oss_src=1<<SOUND_MIXER_MIC;
			break;
		case SMP_LINEIN:
			oss_src=1<<SOUND_MIXER_LINE;
			break;
		default: /* SMP_CD: */
			oss_src=1<<SOUND_MIXER_CD;
	}
	if ((ioctl(fd_mixer, SOUND_MIXER_WRITE_RECSRC, &oss_src)))
	{
#ifdef OSS_DEBUG
		perror("devsoss: ioctl(fd_mixer, SOUND_MIXER_WRITE_RECSRC, &oss_src)");
#endif
	}
}

static int ossSample(unsigned char **buf, int *len)
{
#ifdef OSS_DEBUG
	int tmp;
#endif

	if (*len>65536)
		*len=65536;
	if (*len<4096)
		*len=4096;

	*buf=sampbuf=malloc(*len);

	memsetd(*buf, (smpOpt&SMP_SIGNEDOUT)?0:(smpOpt&SMP_16BIT)?0x80008000:0x80808080, (*len)>>2);

	buflen=*len;
	bufpos=0;
	smpGetBufPos=getbufpos;

	if ((fd_dsp=open(currentcard.path, O_RDONLY|O_NONBLOCK))<0)
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devsoss: open(%s, O_RDONLY|O_NONBLOCK): %s\n", currentcard.path, strerror(errno));
		sleep(3);
#endif
		return 0;
	}
	if (fcntl(fd_dsp, F_SETFD, FD_CLOEXEC)<0)
		perror("devsoss: fcntl(fd_dsp, F_SETFD, FD_CLOEXEC)");
#ifdef OSS_DEBUG
 #if defined(OSS_GETVERSION)
	if (ioctl(fd_dsp, OSS_GETVERSION, &tmp)<0)
		tmp=0;
	if (tmp<361)
		tmp= ((tmp&0xf00)<<8) | ((tmp&0xf0)<<4) | (tmp&0xf);

	fprintf(stderr, "devsoss: compiled agains OSS version %d.%d.%d, version %d.%d.%d detected\n", (SOUND_VERSION&0xff0000)>>16, (SOUND_VERSION&0xff00)>>8, SOUND_VERSION&0xff, (tmp&0xff0000)>>16, (tmp&0xff00)>>8, tmp&0xff);
 #elif defined(SOUND_VERSION)
	fprintf(stderr, "devsoss: compiled agains OSS version %d.%d.%d\n", (SOUND_VERSION&0xff0000)>>16, (SOUND_VERSION&0xff00)>>8, SOUND_VERSION&0xff);
 #endif
#endif

#ifdef SMP_DEBUG
	smpDebug=ossDebug;
#endif

	smpSetOptions(smpRate, smpOpt); /* nasty hack */

	if ((fd_mixer>=0)&&igain>=0)
	{
		int tmp;
		if (igain>100)
			igain=100;
		tmp=((uint8_t)igain)|(((uint8_t)igain)<<8);
		if (ioctl(fd_mixer, SOUND_MIXER_WRITE_IGAIN, &tmp))
		{
#ifdef OSS_DEBUG
			perror("devsoss: ioctl(fd_mixer, SOUND_MIXER_WRITE_IGAIN, &tmp)");
#endif
		}
	}

	return 1;
}

static void ossStop(void)
{
	free(sampbuf);
#ifdef SMP_DEBUG
	smpDebug=0;
#endif
	if (fd_dsp)
	{
		close(fd_dsp);
		fd_dsp=-1;
	}
}

static int ossInit(const struct deviceinfo *card)
{
	memcpy(&currentcard, card, sizeof(struct deviceinfo));

	igain=(int8_t)(card->opt>>8);

	smpSetOptions=SetOptions;
	smpSample=ossSample;
	smpStop=ossStop;
	smpSetSource=ossSetSource;

	if (!card->mixer[0])
		fd_mixer=-1;
	else if ((fd_mixer=open(card->mixer, O_RDWR|O_NONBLOCK))<0)
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devsoss: open(%s, O_RDWR|O_NONBLOCK): %s\n", card->mixer, strerror(errno));
		sleep(3);
#endif
	} else {
		if (fcntl(fd_mixer, F_SETFD, FD_CLOEXEC))
			perror("fcntl(fd_mixer, F_SETFD, FD_CLOEXEC)");
	}

	smpSetOptions(44100, SMP_STEREO|SMP_16BIT);
	smpSetSource(SMP_LINEIN);

	return 1;
}

static void ossClose(void)
{
	smpSample=0;
	smpStop=0;
	smpSetOptions=0;
	smpSetSource=0;
	if (fd_dsp>=0)
		close(fd_dsp);
	fd_dsp=-1;
	if (fd_mixer>=0)
		close(fd_mixer);
	fd_mixer=-1;
}

static int ossDetect(struct deviceinfo *card)
{
	struct stat st;
#if defined(OSS_GETVERSION)
	int tmp;
#endif
	char *temp;

	card->devtype=&smpOSS;
	card->port=-1;
	card->port2=-1;
	card->subtype=-1;
	card->mem=0;
	if ((card->chan<=0)||(card->chan>2))
		card->chan=2;
	if ((temp=getenv("DSP")))
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devsoss: $DSP found\n");
#endif
		strncpy(card->path, temp, DEVICE_NAME_MAX);
		card->path[DEVICE_NAME_MAX-1]=0;
	} else if (!card->path[0])
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devsoss: path not set, using /dev/dsp\n");
#endif
		strcpy(card->path, "/dev/dsp");
	}
	if ((temp=getenv("MIXER")))
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devsoss: $MIXER found\n");
#endif
		strncpy(card->mixer, temp, DEVICE_NAME_MAX);
		card->mixer[DEVICE_NAME_MAX-1]=0;
	}
	if (stat(card->path, &st))
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devsoss: stat(%s, &st): %s\n", card->path, strerror(errno));
#endif
		return 0;
	}

	/* test if basic OSS functions exists */
	if ((fd_dsp=open(card->path, O_RDONLY|O_NONBLOCK))<0)
	{
		if (errno==EAGAIN)
		{
#ifdef OSS_DEBUG
			fprintf(stderr, "devsoss: OSS detected (EGAIN)\n");
#endif
			return 1;
		}
#ifdef OSS_DEBUG
		fprintf(stderr, "devsoss: open(%s, O_RDONLY|O_NONBLOCK): %s\n", card->path, strerror(errno));
#endif
		return 0;
	}

#if defined(OSS_GETVERSION)
	if (ioctl(fd_dsp, OSS_GETVERSION, &tmp)<0)
	{
#ifdef OSS_DEBUG
		perror("devposs: (warning) ioctl(fd_dsp, OSS_GETVERSION, &tmp)");
#endif
	}
#endif

#ifdef OSS_DEBUG
	fprintf(stderr, "devposs: OSS detected\n");
#endif
	close(fd_dsp);
	fd_dsp=-1;

	return 1; /* device exists, so we are happy.. can do better tests later */
}

static uint32_t ossGetOpt(const char *sec)
{
	uint32_t opt=0;
	int8_t igain;

	if (cfGetProfileBool(sec, "revstereo", 0, 0))
		opt|=REVSTEREO;
	igain=cfGetProfileInt(sec, "igain", -1, 10);
	opt|=(((uint16_t)igain)<<8);
	return opt;
}

struct devaddstruct smpOSSAdd = {ossGetOpt, 0, 0, 0};
struct sounddevice smpOSS={SS_SAMPLER, 0, "OSS Recorder", ossDetect, ossInit, ossClose, &smpOSSAdd};

char *dllinfo="driver smpOSS";
struct linkinfostruct dllextinfo = {.name = "devsoss", .desc = "OpenCP Sampler Device: OSS (c) 2004-'22 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
