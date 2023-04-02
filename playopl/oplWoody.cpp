/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2006 Simon Peter, <dn.tlp@gmx.net>, et al.
 *
 * This file is based on Adplug Project:
 * wemuopl.h - Emulated OPL using the DOSBox OPL3 emulator
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
#include "oplWoody.h"

extern "C" {
#include "adplug-git/src/woodyopl.h"
}

oplWoody::oplWoody(int rate) : samplerate(rate)
{
	opl.adlib_init (rate, /* stereo */ 2, /* 16bit */ 2);
	currType = TYPE_OPL3;
}

oplWoody::~oplWoody()
{
}

void oplWoody::update(short *buf, int samples)
{
	opl.adlib_getsample (buf, samples);
}

void oplWoody::write(int reg, int val)
{
	opl.adlib_write ((currChip << 8) | reg, val);
}

void oplWoody::init()
{
	opl.adlib_init (samplerate, /* stereo */ 2, /* 16bit */ 2);
	currChip = 0;
}
