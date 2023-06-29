/* OpenCP Module Player
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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

#ifndef H_ADPLUG_RETROWAVE_H
#define H_ADPLUG_RETROWAVE_H

#include "adplug-git/src/opl.h"

struct cpifaceSessionAPI_t;

class oplRetroWave: public Copl
{
public:
	oplRetroWave ( /* cpiDebug is splitted out, since we need to create this from oplconfig.c for testing */
		void(*cpiDebug)(struct cpifaceSessionAPI_t *cpifaceSession, const char *fmt, ...),
		struct cpifaceSessionAPI_t *cpifaceSession,
		const char *device,
		int rate);
	virtual ~oplRetroWave();
	int FailedToOpen;

	uint32_t ratescale = 0x00010000;
	void update(short *buf, int samples);
	void write(int reg, int val);
	void init();

private:
	int rate;
	unsigned int samples_rounded = 0; // update() brings the rounding error into the next call
};

#endif
