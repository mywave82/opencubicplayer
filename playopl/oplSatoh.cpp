/* OpenCP Module Player
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * This file is based on AdPlug project, released under GPLv2
 * with permission from Simon Peter.
 *
 * AdPlug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2002 Simon Peter <dn.tlp@gmx.net>, et al.
 * emuopl.cpp - Emulated OPL, by Simon Peter <dn.tlp@gmx.net>
 *
 * File is changed in letting FM_OPL be public
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

#include "ocpemu.h"
#include "oplSatoh.h"
#include <string.h>
#include <math.h>

oplSatoh::oplSatoh(int rate) : mixbufSamples(0)
{
	opl[0] = OPLCreate(OPL_TYPE_YM3812, 3579545, rate);
	opl[1] = OPLCreate(OPL_TYPE_YM3812, 3579545, rate);

	currType = TYPE_DUAL_OPL2;

	init();
}

oplSatoh::~oplSatoh()
{
	OPLDestroy(opl[0]);
	OPLDestroy(opl[1]);
	if(mixbufSamples)
	{
		delete[] mixbuf0;
		delete[] mixbuf1;
	}
}

void oplSatoh::update(short *buf, int samples)
{
	int i;

	if (!samples)
	{
		return;
	}

	if(mixbufSamples < samples)
	{
		if(mixbufSamples)
		{
			delete[] mixbuf0;
			delete[] mixbuf1;
		}
		mixbufSamples = samples;
		mixbuf0 = new short[samples];
		mixbuf1 = new short[samples];
	}

	YM3812UpdateOne(opl[0], mixbuf0, samples);
	YM3812UpdateOne(opl[1], mixbuf1, samples);

	for(i=0;i<samples;i++)
	{
		buf[i*2]   = mixbuf0[i];
		buf[i*2+1] = mixbuf1[i];
	}
}

void oplSatoh::write(int reg, int val)
{
	OPLWrite(opl[currChip],0,reg);
	OPLWrite(opl[currChip],1,val);
}

void oplSatoh::init()
{
	OPLResetChip(opl[0]);
	OPLResetChip(opl[1]);
	currChip = 0;
}
