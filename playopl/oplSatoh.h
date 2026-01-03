/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * This file is based on AdPlug project, released under GPLv2
 * with permission from Simon Peter.
 *
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2004 Simon Peter, <dn.tlp@gmx.net>, et al.
 * emuopl.h - Emulated OPL, by Simon Peter <dn.tlp@gmx.net>
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

#ifndef H_ADPLUG_OPLSATOH
#define H_ADPLUG_OPLSATOH

#include "adplug-git/src/opl.h"
extern "C" {
#include "adplug-git/src/fmopl.h"
}

#include "ocpemu.h"

class oplSatoh: public Copl
{
public:
	oplSatoh(int rate);                     // rate = sample rate
	virtual ~oplSatoh();

	void update(short *buf, int samples);   // fill buffer
	void write(int reg, int val);
	void init();

private:
	FM_OPL *opl[2];                         // holds emulator data
	short  *mixbuf0,
	       *mixbuf1;
	int     mixbufSamples;
};

#endif
