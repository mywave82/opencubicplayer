/* OpenCP Module Player
 * copyright (c) 2011-'25 François Revol <revol@free.fr>
 *
 * SDL Player device
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
 *  -fr110319   François Revol <revol@free.fr>
 *    -copied from devpcoreaudio
 */

#include "config.h"
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_audio.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#ifdef SDL_DEBUG
 #define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
 #define PRINT(...) do {} while(0)
#endif

#include "devpsdl-common.c"

static const struct plrDevAPI_t *sdlInit (const struct plrDriver_t *driver, const struct plrDriverAPI_t *DriverAPI)
{
	char drivername[FILENAME_MAX];

	plrDriverAPI = DriverAPI;

	PRINT("%s()\n", __FUNCTION__);
	if (SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		fprintf(stderr, "[SDL] SDL_InitSubSystem (SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
		return 0;
	}

	fprintf(stderr, "[SDL] Using driver %s\n", SDL_AudioDriverName(drivername, sizeof(drivername)));
	return &devpSDL;
}

static const struct plrDriver_t plrSDL =
{
	"devpSDL",
	"SDL 1.x Player",
	sdlDetect,
	sdlInit,
	sdlClose
};

DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "devpsdl", .desc = "OpenCP Player Device: SDL (c) 2011-'25 François Revol & Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99, .PluginInit = sdlPluginInit, .PluginClose = sdlPluginClose};
