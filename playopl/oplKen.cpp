/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2006 Simon Peter, <dn.tlp@gmx.net>, et al.
 *
 * This file is based on Adplug Project:
 * kemuopl.cpp - Emulated OPL using Ken Silverman's emulator
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "ocpemu.h"
#include "oplKen.h"

#include "config.h"
#include <string.h>
#include "types.h"

extern "C" {
#include "adplug-git/src/adlibemu.h"
}

oplKen::oplKen(int rate) : samplerate(rate), mixbufSamples(0)
{
	memset (ctx, 0, sizeof (ctx));
	currType = TYPE_DUAL_OPL2;
	init();
}

oplKen::~oplKen()
{
	if(mixbufSamples)
	{
		delete [] mixbuf0;
		delete [] mixbuf1;
	}
}

void oplKen::update(short *buf, int samples)
{
	int i;
	//ensure that our mix buffers are adequately sized
	if (mixbufSamples < samples)
	{
		if(mixbufSamples)
		{
			delete[] mixbuf0;
			delete[] mixbuf1;
		}
		mixbufSamples = samples;
		//*2 = make room for stereo, if we need it
		mixbuf0 = new short[samples*2];
		mixbuf1 = new short[samples*2];
	}

	//render each chip to a different tempbuffer
	adlibgetsample(&ctx[0], (unsigned char *)mixbuf0, samples * 2 /* 16-bit */);
	adlibgetsample(&ctx[1], (unsigned char *)mixbuf1, samples * 2 /* 16-bit */);

	for(i=0;i<samples;i++)
	{
		buf[i*2]   = mixbuf0[i];
		buf[i*2+1] = mixbuf1[i];
	}
}

void oplKen::write(int reg, int val)
{
	adlib0(&ctx[currChip], reg, val);
}

void oplKen::init()
{
	adlibinit(&ctx[0], samplerate, /* mono */ 1, /* 16-bit */ 2);
	adlibinit(&ctx[1], samplerate, /* mono */ 1, /* 16-bit */ 2);
	currChip = 0;
}
