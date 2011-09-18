/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "cpiface/vol.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "dev/devigen.h"
#ifdef PLR_DEBUG
#include "stuff/poutput.h"
#endif
#include "stuff/imsrtns.h"

/* TODO, AFMT_S16_NE, 32bit AFMT_S32_BE would be porn !! */

#define REVSTEREO 1

extern struct sounddevice plrOSS;

static struct deviceinfo currentcard;

static int fd_dsp=-1;
static int fd_mixer=-1;
static char *playbuf;
static int buflen;

static volatile int kernpos, cachepos, bufpos;
static volatile int cachelen, kernlen; /* to make life easier */

/*  playbuf     kernpos  cachepos   bufpos      buflen
 *    |           | kernlen | cachelen |          |
 *
 *  on flush, we update all variables> *  on getbufpos we return kernpos-(1 sample) as safe point to buffer up to
 *  on getplaypos we use last known kernpos if locked, else update kernpos
 */

static volatile uint32_t playpos; /* how many samples have we done totally */
static int stereo;
static int bit16;

static volatile int busy=0;

static int mixer_devmask=0;
static struct ocpvolstruct mixer_entries[SOUND_MIXER_NRDEVICES];

static void flush(void)
{
	int result, n, odelay;

	struct audio_buf_info info;

	if (busy++)
	{
#ifdef OSS_LOCK_DEBUG
		write(2, "devposs: flush(), BUSY set\n", 28);
#endif
		busy--;
		return;
	}

#ifdef SNDCTL_DSP_GETODELAY
	/* update kernlen stuff before we check if cachelen is set furter down */
	if (ioctl(fd_dsp, SNDCTL_DSP_GETODELAY, &odelay))
	{
#ifdef OSS_DEBUG
		perror("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETODELAY, &count)");
#endif
		busy--;
		return;
	}
#ifdef OSS_DEBUG
	{
		if (odelay<0)
		{
			write(2, "devposs: buggy nvidia driver?\n", 30);
		}
	}
#endif
#else
	{
		struct count_info i;
		if (ioctl(fd_dsp, SNDCTL_DSP_GETOPTR, &i))
		{
#ifdef OSS_DEBUG
			perror("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETOPTR, &i)");
#endif
			busy--;
			return;
		}
		odelay=i.bytes;
	}
#endif
	odelay=abs(odelay); /* This is because of nvidia sound-drivers */
	if (odelay>kernlen)
	{
#ifdef OSS_DEBUG
		write(2, "devposs: flush() kernelbug found!\n", 34);
#endif
		odelay=kernlen;
	} else if ((odelay<kernlen))
	{
		kernlen=odelay;
		kernpos=(cachepos-kernlen+buflen)%buflen;
	}

	if (!cachelen)
	{
		busy--;
		return;
	}

	if (ioctl(fd_dsp, SNDCTL_DSP_GETOSPACE, &info))
	{
#ifdef OSS_DEBUG
		perror("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETOSPACE, &info)");
#endif
		busy--;
		return;
	}
	if (!info.bytes)
	{
		busy--;
		return;
	}

	if (bufpos<=cachepos)
		n=buflen-cachepos;
	else
		n=bufpos-cachepos;

	/* first.. don't overrun kernel buffer, since that can block */
	if (n>info.bytes)
		n=info.bytes;

	if (n%(1<<(bit16+stereo)))
	{
#ifdef OSS_DEBUG
		write(2, "devposs: flush() alignment failed\n", 35);
#endif
		n>>=bit16+stereo;
		n<<=bit16+stereo;
	}

	if (n<=0)
	{
		busy--;
		return;
	}
	result=write(fd_dsp, playbuf+cachepos, n);
	if (result<0)
	{
#ifdef OSS_DEBUG
		perror("devposs: flush() write");
#endif
		busy--;
		return;
	}
	cachepos=(cachepos+result+buflen)%buflen;
	playpos+=result;
	cachelen-=result;
	kernlen+=result;

	busy--;
}

static int getplaypos(void)
{
	int retval;

	if (busy++)
	{
#ifdef OSS_LOCK_DEBUG
		write(2, "devposs: getplaypos() BUSY set\n", 31);
#endif
	} else {
		int tmp;
#ifdef SNDCTL_DSP_GETODELAY
		if (ioctl(fd_dsp, SNDCTL_DSP_GETODELAY, &tmp))
		{
#ifdef OSS_DEBUG
			perror("devposs: ioctl(OSS, SNDCTL_DSP_GETODELAY, &count)");
#endif
			busy--;
			return kernpos;
		}
#ifdef OSS_DEBUG
		{
			if (tmp<0)
			{
				write(2, "devposs: buggy nvidia driver?\n", 30);
			}
		}
#else
		{
			struct count_info i;
			if (ioctl(fd_dsp, SNDCTL_DSP_GETOPTR, &i))
			{
#ifdef OSS_DEBUG
				perror("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETOPTR, &i)");
#endif
				busy--;
				return kernpos;
			}
			tmp=i.bytes;
		}
#endif

#endif
		tmp=abs(tmp); /* we love nvidia, don't we */
		if (tmp>kernlen)
		{
#ifdef OSS_DEBUG
			write(2, "devposs: getplaypos() kernelbug found\n", 38);
#endif
		} else {
			kernlen=tmp;
		}
		kernpos=(cachepos-kernlen+buflen)%buflen;
	}
	retval=kernpos;
	if (retval<0)
	{
#ifdef OSS_DEBUG
		write(2, "devposs: assert #1 failed\n", 26);
		retval=0;
#endif
	}
	busy--;
	return retval;
}

static int getbufpos(void)
{
	int retval;

	if (busy++)
	{
#ifdef OSS_LOCK_DEBUG
		write(2, "devposs: getbufpos() BUSY set\n", 30);
#endif
	}

	if (kernpos==bufpos)
	{
		if (cachelen|kernlen)
		{
#ifdef OSS_DEBUG
			write(2, "devposs: getbufpos() unreachable code\n", 38);
#endif
			retval=kernpos;
			busy--;
			return retval;
		}
	}
/*
	if ((!cachelen)&&(!kernlen))
		retval=(kernpos+buflen-(0<<(bit16+stereo)))%buflen;
	else*/
		retval=(kernpos+buflen-(1<<(bit16+stereo)))%buflen;
	busy--;
	return retval;
}

static void advance(unsigned int pos)
{
	if (busy++)
	{
#ifdef OSS_LOCK_DEBUG
		write(2, "devposs: advance() BUSY set\n", 28);
#endif
	}
/*
	if (bit16)
	{
		int i;
		i=bufpos;
		while (i!=pos)
		{
			((uint16_t *)playbuf)[i>>1]=uint16_little(((uint16_t *)playbuf)[i>>1]);
			i+=2;
			if (i>=buflen)
				i=0;
		}
	}
*/
	cachelen+=(pos-bufpos+buflen)%buflen;
	bufpos=pos;

#ifdef OSS_DEBUG
	if ((cachelen+kernlen)>buflen)
		write(2, "devposs: assert #2 failed\n", 26);
#endif

	busy--;
}

static uint32_t gettimer(void)
{
	long tmp=playpos;
	int odelay;

	if (busy++)
	{
#ifdef OSS_LOCK_DEBUG
		write(2, "devposs: gettimer(): BUSY set\n", 30);
#endif
		odelay=kernlen;
	} else {
#ifdef SNDCTL_DSP_GETODELAY
		if (ioctl(fd_dsp, SNDCTL_DSP_GETODELAY, &odelay))
		{
#ifdef OSS_DEBUG
			perror("ioctl(OSS, SNDCTL_DSP_GETODELAY, &count)");
#endif
			odelay=kernlen;
		}
#ifdef OSS_DEBUG
		{
			if (odelay<0)
			{
				write(2, "devposs: buggy nvidia driver?\n", 30);
			}
		}
#endif
#else
		{
			struct count_info i;
			if (ioctl(fd_dsp, SNDCTL_DSP_GETOPTR, &i))
		{
#ifdef OSS_DEBUG
				perror("devposs: flush() ioctl(OSS, SNDCTL_DSP_GETOPTR, &i)");
#endif
				odelay=kernlen;
			} else
				odelay=i.bytes;
		}
#endif

		odelay=abs(odelay);
		if (odelay>kernlen)
		{
#ifdef OSS_DEBUG
			write(2, "devposs: gettimer() kernelbug found\n", 36);
#endif
			odelay=kernlen;
		} else {
			kernlen=odelay;
			kernpos=(cachepos-kernlen+buflen)%buflen;
		}
	}

	tmp-=odelay;
	busy--;
	return imuldiv(tmp, 65536>>(stereo+bit16), plrRate);
}

static void ossStop(void)
{
	if (fd_dsp<0)
		return;
	free(playbuf);
#ifdef PLR_DEBUG
	plrDebug=0;
#endif
	plrIdle=0;
	close(fd_dsp);
	fd_dsp=-1;
}

#ifdef PLR_DEBUG
static char *ossDebug(void)
{
	static char buffer[100];
	strcpy(buffer, "devposs: ");
	convnum(cachelen, buffer+9, 10, 6, 1);
	strcat(buffer, "/");
	convnum(kernlen, buffer+16, 10, 6, 1);
	strcat(buffer, "/");
	convnum(buflen, buffer+23, 10, 6, 1);
	return buffer;
}
#endif

static int ossPlay(void **buf, unsigned int *len)
{
#ifdef OSS_DEBUG
	int tmp;
#endif
	if ((*len)<(plrRate&~3))
		*len=plrRate&~3;
	if ((*len)>(plrRate*4))
		*len=plrRate*4;
	playbuf=*buf=malloc(*len);

	memsetd(*buf, (plrOpt&PLR_SIGNEDOUT)?0:(plrOpt&PLR_16BIT)?0x80008000:0x80808080, (*len)>>2);

	buflen=*len;
	bufpos=0;
	cachepos=0;
	cachelen=0;
	playpos=0;
	kernpos=0;
	kernlen=0;

	plrGetBufPos=getbufpos;
	plrGetPlayPos=getplaypos;
	plrIdle=flush;
	plrAdvanceTo=advance;
	plrGetTimer=gettimer;
#ifdef PLR_DEBUG
	plrDebug=ossDebug;
#endif

	if ((fd_dsp=open(currentcard.path, O_WRONLY|O_NONBLOCK))<0)
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devposs: open(%s, O_WRONLY|O_NONBLOCK): %s\n", currentcard.path, strerror(errno));
		sleep(3);
#endif
		return 0;
	}
	if (fcntl(fd_dsp, F_SETFD, FD_CLOEXEC)<0)
		perror("devposs: fcntl(fd_dsp, F_SETFD, FD_CLOEXEC)");
#ifdef OSS_DEBUG
 #if defined(OSS_GETVERSION)
	if (ioctl(fd_dsp, OSS_GETVERSION, &tmp)<0)
		tmp=0;
	if (tmp<361)
		tmp= ((tmp&0xf00)<<8) | ((tmp&0xf0)<<4) | (tmp&0xf);

	fprintf(stderr, "devposs: compiled agains OSS version %d.%d.%d, version %d.%d.%d detected\n", (SOUND_VERSION&0xff0000)>>16, (SOUND_VERSION&0xff00)>>8, SOUND_VERSION&0xff, (tmp&0xff0000)>>16, (tmp&0xff00)>>8, tmp&0xff);
 #elif defined(SOUND_VERSION)
	fprintf(stderr, "devposs: compiled agains OSS version %d.%d.%d\n", (SOUND_VERSION&0xff0000)>>16, (SOUND_VERSION&0xff00)>>8, SOUND_VERSION&0xff);
 #endif
#endif
	plrSetOptions(plrRate, plrOpt);

	return 1;
}

static void SetOptions(uint32_t rate, int opt)
{
	int tmp;

	int newopt;

	int fd;

	if (fd_dsp<0)
	{
		if ((fd=open(currentcard.path, O_WRONLY|O_NONBLOCK))<0) /* no need to FD_SET this, since it will be closed very soon */
		{
#ifdef OSS_DEBUG
			fprintf(stderr, "devposs: open(%s, O_WRONLY|O_NONBLOCK): %s\n", currentcard.path, strerror(errno));
			sleep(3);
#endif
			plrRate=rate;
			plrOpt=opt; /* have to do... but it stinks */
			return;
		}
	} else
		fd=fd_dsp;

	if (opt&PLR_16BIT)
		tmp=16;
	else
		tmp=8;
	if (ioctl(fd, SOUND_PCM_WRITE_BITS, &tmp)<0)
	{
#ifdef OSS_DEBUG
		perror("devposs: ioctl(fd_dsp, SOUND_PCM_WRITE_BITS, &tmp)");
#endif
	}

	if ((bit16=(tmp==16)))
		newopt = PLR_16BIT | PLR_SIGNEDOUT;
	else
		newopt = 0;

	if (opt&PLR_STEREO)
		tmp=2;
	else
		tmp=1;

	if (ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &tmp)<0)
	{
#ifdef OSS_DEBUG
		perror("devposs: ioctl(fd_dsp, SOUND_PCM_WRITE_CHANNELS, tmp)");
#endif
	}

	if ((stereo=(tmp==2)))
		newopt |= PLR_STEREO;

	if (ioctl(fd, SOUND_PCM_WRITE_RATE, &rate)<0)
	{
#ifdef OSS_DEBUG
		perror("devposs: ioctl(fd_dsp, SOUND_PCM_WRITE_RATE, rate)");
#endif
	}
	if (currentcard.opt&REVSTEREO)
		newopt|=PLR_REVERSESTEREO;

	plrRate=rate;
	plrOpt=newopt;
	if (fd_dsp<0) /* ugly hack */
		close(fd);
}

static int ossDetect(struct deviceinfo *card)
{
#ifdef HAVE_SYS_SOUNDCARD_H
	struct stat st;
#endif
	int tmp;
	char *temp;

	card->devtype=&plrOSS;
	card->port=-1;
	card->port2=-1;
	card->subtype=-1;
	card->mem=0;
	if ((card->chan<=0)||(card->chan>2))
		card->chan=2;
	if ((temp=getenv("DSP")))
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devposs: $DSP found\n");
#endif
		strncpy(card->path, temp, DEVICE_NAME_MAX);
		card->path[DEVICE_NAME_MAX-1]=0;
	} else if (!card->path[0])
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devposs: path not set, using /dev/dsp\n");
#endif
		strcpy(card->path, "/dev/dsp");
	}
	if ((temp=getenv("MIXER")))
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devposs: $MIXER found\n");
#endif
		strncpy(card->mixer, temp, DEVICE_NAME_MAX);
		card->mixer[DEVICE_NAME_MAX-1]=0;
	}
#ifdef HAVE_SYS_SOUNDCARD_H
/* this test will fail with liboss */
	if (stat(card->path, &st))
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devposs: stat(%s, &st): %s\n", card->path, strerror(errno));
#endif
		return 0;
	}
#endif

	/* test if basic OSS functions exists */
	if ((fd_dsp=open(card->path, O_WRONLY|O_NONBLOCK))<0) /* no need to FD_SETFD, since it will be closed soon */
	{
		if ((errno==EAGAIN)||(errno==EINTR))
		{
#ifdef OSS_DEBUG
			fprintf(stderr, "devposs: OSS detected (EAGAIN/EINTR)\n");
#endif
			return 1;
		}
#ifdef OSS_DEBUG
		fprintf(stderr, "devposs: open(%s, O_WRONLY|O_NONBLOCK): %s\n", card->path, strerror(errno));
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

static int ossInit(const struct deviceinfo *card)
{
	memcpy(&currentcard, card, sizeof(struct deviceinfo));
	plrSetOptions=SetOptions;
	plrPlay=ossPlay;
	plrStop=ossStop;

	if (!card->mixer[0])
		fd_mixer=-1;
	else if ((fd_mixer=open(card->mixer, O_RDWR|O_NONBLOCK))<0)
	{
#ifdef OSS_DEBUG
		fprintf(stderr, "devposs: open(%s, O_RDWR|O_NONBLOCK): %s\n", card->mixer, strerror(errno));
		sleep(3);
#endif
	}
	if (fd_mixer>=0)
	{
		int i;
		const char *labels[]=SOUND_DEVICE_LABELS; /* const data should be statically allocated, while the refs we don't care about */
		if (fcntl(fd_mixer, F_SETFD, FD_CLOEXEC)<0)
			perror("devposs: fcntl(fd_mixer, F_SETFD, FD_CLOEXEC)");
		if (ioctl(fd_mixer, SOUND_MIXER_READ_DEVMASK, &mixer_devmask))
		{
#ifdef OSS_DEBUG
			perror("devposs: ioctl(fd_mixer, SOUND_MIXER_READ_DEVMASK)");
#endif
			goto no_mixer;
		}
		for (i=0;i<SOUND_MIXER_NRDEVICES;i++)
		{
			if (mixer_devmask&(1<<i))
			{
				if (ioctl(fd_mixer, MIXER_READ(i), &mixer_entries[i].val))
				{
#ifdef OSS_DEBUG
					perror("devposs: ioctl(fd_mixer, MIXER_READ(i), &val)");
#endif
					mixer_entries[i].val=0;
				} else {
					mixer_entries[i].val=((mixer_entries[i].val&255)+(mixer_entries[i].val>>8))>>1; /* we don't vare about balance in current version */
				}
			} else
				mixer_entries[i].val=0;
			mixer_entries[i].min=0;
			mixer_entries[i].max=100;
			mixer_entries[i].step=1;
			mixer_entries[i].log=0;
			mixer_entries[i].name=labels[i];
		}
	} else {
		mixer_devmask=0;
	}

	goto clean_exit;

no_mixer:
	close(fd_mixer);
	fd_mixer=-1;
	mixer_devmask=0;

clean_exit:
	SetOptions(44100, PLR_16BIT|PLR_STEREO);
	return 1;
}

static void ossClose(void)
{
	plrPlay=0;
	plrStop=0;
	plrSetOptions=0;
	if (fd_dsp>=0)
		close(fd_dsp);
	fd_dsp=-1;
	if (fd_mixer>=0)
		close(fd_mixer);
	fd_mixer=-1;
}

static uint32_t ossGetOpt(const char *sec)
{
	uint32_t opt=0;

	if (cfGetProfileBool(sec, "revstereo", 0, 0))
		opt|=REVSTEREO;
	return opt;
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
#ifdef OSS_DEBUG
			perror("devposs: ioctl(fd_mixer, MIXER_WRITE(n), &val)");
#endif
		}
		return 1;
	}
	return 0;
}

static struct devaddstruct plrOSSAdd = {ossGetOpt, 0, 0, 0};
struct sounddevice plrOSS={SS_PLAYER, 0, "OSS player", ossDetect,  ossInit,  ossClose, &plrOSSAdd};
struct ocpvolregstruct voloss={volossGetNumVolume, volossGetVolume, volossSetVolume};

char *dllinfo = "driver plrOSS; volregs voloss";
struct linkinfostruct dllextinfo = {"devposs", "OpenCP Player Device: OSS (c) 2004-09 Stian Skjelstad", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
