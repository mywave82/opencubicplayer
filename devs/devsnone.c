/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * NoSound sampler device (samples perfect noise-free 16bit silence)
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
 */

#include "config.h"
#include <stdlib.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dev/imsdev.h"
#include "dev/sampler.h"
#include "stuff/imsrtns.h"

extern struct sounddevice smpNone;
unsigned char *sampbuf;

static void ndSetOptions(int rate, int opt)
{
	if (rate<5000)
		rate=5000;

	if (rate>48000)
		rate=48000;

	smpRate=rate;
	smpOpt=opt;
}

static void ndSetSource(int src)
{
}


static int getbufpos(void)
{
	return 0;
}


static int ndStart(unsigned char **buf, int  *len)
{
	uint32_t initval;
	uint32_t dmalen;

	initval=(smpOpt&SMP_SIGNEDOUT)?0:0x80808080;
	if (smpOpt&SMP_16BIT)
		initval&=0xFF00FF00;

	dmalen=(*len)>>((!!(smpOpt&SMP_STEREO))+(!!(smpOpt&SMP_16BIT)));
	memsetd(*buf, initval, dmalen);

	smpGetBufPos=getbufpos;
	sampbuf=*buf=calloc(*len, 1);
	return 1;
}

static void ndStop(void)
{
	free(sampbuf);
}




static int ndInit(const struct deviceinfo *c)
{
	smpSetOptions=ndSetOptions;
	smpSample=ndStart;
	smpStop=ndStop;
	smpSetSource=ndSetSource;

	smpSetOptions(65535, SMP_STEREO|SMP_16BIT);
	smpSetSource(SMP_LINEIN);

	return 1;
}

static void ndClose(void)
{
  smpSample=0;
}

static int ndDetect(struct deviceinfo *c)
{
	c->devtype=&smpNone;
	c->port=-1;
	c->port2=-1;
/*
	c->irq=-1;
	c->irq2=-1;
	c->dma=-1;
	c->dma2=-1;
*/
	c->subtype=-1;
	c->mem=0;
	c->chan=2;
	return 1;
}


struct sounddevice smpNone={SS_SAMPLER, 0, "Eternal Silence Recorder", ndDetect, ndInit, ndClose, 0};
char *dllinfo="driver smpNone";
struct linkinfostruct dllextinfo = {.name = "devsnone", .desc = "OpenCP Sampler Device: None (c) 1994-09 Niklas Beisert", .ver = DLLVERSION, .size = 0};
