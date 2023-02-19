/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Variables for wavetable system
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
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "dev/mcp.h"
#include "stuff/freq.h"

unsigned int mcpMixMaxRate;
unsigned int mcpMixProcRate;

static struct mcpAPI_t _mcpAPI =
{
	mcpGetFreq6848,
	mcpGetFreq8363,
	mcpGetNote6848,
	mcpGetNote8363,
	mcpReduceSamples,
};

const struct mcpAPI_t *mcpAPI = &_mcpAPI;
