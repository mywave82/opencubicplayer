/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
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

#ifndef H_ADPLUG_OPLNUKEDH
#define H_ADPLUG_OPLNUKEDH

#include "adplug-git/src/opl.h"

typedef struct _opl3_chip opl3_chip;

class oplNuked: public Copl
{
public:
	oplNuked(int rate);
	virtual ~oplNuked();

	void update(short *buf, int samples);
	void write(int reg, int val);
	void init();

private:
	opl3_chip *opl;
	int        samplerate;
};

#endif
