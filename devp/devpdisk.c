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
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "stuff/imsrtns.h"

extern struct sounddevice plrDiskWriter;

static uint32_t filepos;
static unsigned long buflen;
static char *playbuf;
static unsigned long bufpos;
static unsigned long bufrate;
static int file;
static unsigned char *diskcache;
static unsigned long cachelen;
static unsigned long cachepos;
static volatile char busy;
static unsigned short playrate;
static unsigned char stereo;
static unsigned char bit16;
static unsigned char writeerr;

static void Flush(void)
{
	busy=1;
	if (cachepos>(cachelen/2))
	{
		if (!writeerr)
		{
rewrite:
			if (bit16)
			{
				int i, j = cachepos/2;
				uint16_t *d = (uint16_t *)diskcache;
				for (i=0;i<j;i++)
					d[i]=uint16_little(d[i]);
			}
			if ((unsigned)write(file, diskcache, cachepos)!=cachepos)
			{
				if (errno==EAGAIN)
					goto rewrite;
				if (errno==EINTR)
					goto rewrite;
				writeerr=1;
			}
		}
		filepos+=cachepos;
		cachepos=0;
	}
	busy=0;
}

static int getbufpos(void)
{
	return (buflen + bufpos - (1<<(stereo+bit16))) % buflen;
}

static int getplaypos(void)
{
	return bufpos;
}

static void advance(unsigned int pos)
{
	busy=1;

	if (pos<bufpos)
	{
		memcpy(diskcache+cachepos, playbuf+bufpos, buflen-bufpos);
		memcpy(diskcache+cachepos+buflen-bufpos, playbuf, pos);
		cachepos+=(buflen-bufpos)+pos;
	} else {
		if (pos == bufpos)
		{
			memcpy(diskcache+cachepos, playbuf+bufpos, buflen-bufpos);
			memcpy(diskcache+cachepos+buflen-bufpos, playbuf, pos);
			cachepos += buflen;
		} else {
			memcpy(diskcache+cachepos, playbuf+bufpos, pos-bufpos);
			cachepos+=pos-bufpos;
		}
	}

	if (cachepos>cachelen)
	{
		fprintf(stderr, "devpdisk: cachepos>cachelen\n");
		exit(0);
	}

	bufpos=pos;
	busy=0;
}

static uint32_t gettimer(void)
{
	return imuldiv((filepos+cachepos), (65536>>(stereo+bit16)), playrate);
}

static void dwSetOptions(uint32_t rate, int opt)
{
	stereo=!!(opt&PLR_STEREO);
	bit16=!!(opt&PLR_16BIT);

	if (bit16)
		opt|=PLR_SIGNEDOUT;
	else
		opt&=~PLR_SIGNEDOUT;

	if (rate<5000)
		rate=5000;

	if (rate>64000)
		rate=64000;

	playrate=rate;
	plrRate=rate;
	plrOpt=opt;
}

static int dwPlay(void **buf, unsigned int *len, struct ocpfilehandle_t *source_file)
{
	unsigned char hdr[0x2C];

	memset(&hdr, 0, sizeof(hdr));

	if (*len>32704)
		*len=32704;
	*buf=playbuf=malloc(*len);
	if (!*buf)
		return 0;
	memsetd(*buf, (plrOpt&PLR_SIGNEDOUT)?0:(plrOpt&PLR_16BIT)?0x80008000:0x80808080, (*len)>>2);

	writeerr=0;

	cachelen=(1024*16*playrate/44100)<<(2+stereo+bit16);
	if (cachelen<(*len+1024))
		cachelen=*len+1024;
	cachepos=0;
	diskcache=malloc(cachelen);
	if (!diskcache)
		return 0;

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
			if ((file=open(fn, O_WRONLY|O_CREAT|O_EXCL, S_IREAD|S_IWRITE))>=0)
				break;
		}
		free (fn);
	}

	if (file<0)
		return 0;

	while (1)
	{
		if (write(file, hdr, 0x2C)>=0)
			break;
		if (errno==EAGAIN)
			continue;
		if (errno==EINTR)
			continue;
		break;
	}

	buflen=*len;
	bufpos=0;
	busy=0;
	bufrate=buflen/2;
	if (bufrate>65520)
		bufrate=65520;
	filepos=0;

	plrGetBufPos=getbufpos;
	plrGetPlayPos=getplaypos;
	plrAdvanceTo=advance;
	plrIdle=Flush;
	plrGetTimer=gettimer;

	return 1;
}

static void dwStop(void)
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

	plrIdle=0;

	if (!writeerr)
	{
rewrite:
		if (write(file, diskcache, cachepos)<0)
		{
			if (errno==EINTR)
				goto rewrite;
			if (errno==EAGAIN)
				goto rewrite;
		}
	}

	wavlen=lseek(file, 0, SEEK_CUR)-0x2C;

	lseek(file, 0, SEEK_SET);

	memcpy(wavhdr.riff, "RIFF", 4);
	memcpy(wavhdr.wave, "WAVE", 4);
	memcpy(wavhdr.fmt_, "fmt ", 4);
	memcpy(wavhdr.data, "data", 4);
	wavhdr.chlen=uint32_little(0x10);
	wavhdr.form=uint16_little(1);
	wavhdr.chan=uint16_little(1<<stereo);
	wavhdr.rate=uint32_little(playrate);
	wavhdr.bits=uint16_little(8<<bit16);
	wavhdr.bpsmp=uint16_little((1<<stereo)*(8<<bit16)/8);
	wavhdr.datarate=uint32_little(((1<<stereo)*(8<<bit16)/8)*playrate);
	wavhdr.wavlen=uint32_little(wavlen);
	wavhdr.len24=uint32_little(wavlen+0x24);
rewrite2:
	if (write(file, &wavhdr, 0x2C)<0)
	{
		if (errno==EINTR)
			goto rewrite2;
		if (errno==EAGAIN)
			goto rewrite2;
	}

	lseek(file, 0, SEEK_END);
reclose:
	if (close(file)<0)
	{
		if (errno==EINTR)
			goto reclose;
	}
	free(playbuf);
	free(diskcache);
}

static int dwInit(const struct deviceinfo *d)
{
	plrSetOptions=dwSetOptions;
	plrPlay=dwPlay;
	plrStop=dwStop;
	return 1;
}

static void dwClose(void)
{
	plrPlay=0;
}

static int dwDetect(struct deviceinfo *card)
{
	card->devtype=&plrDiskWriter;
	card->port=-1;
	card->port2=-1;
/*
	card->irq=-1;
	card->irq2=-1;
	card->dma=-1;
	card->dma2=-1;
*/
	card->subtype=-1;
	card->mem=0;
	card->chan=2;

	return 1;
}

#include "dev/devigen.h"
#include "boot/psetting.h"

struct sounddevice plrDiskWriter={SS_PLAYER, 0, "Disk Writer", dwDetect, dwInit, dwClose, 0};

char *dllinfo = "driver plrDiskWriter;";
struct linkinfostruct dllextinfo = {.name = "devpdisk", .desc = "OpenCP Player Device: Disk Writer (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0};
