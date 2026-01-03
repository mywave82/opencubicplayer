/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2011-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Basic glue for the different console implementations for unix
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
 *  -ss040614   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#define _CONSOLE_DRIVER
#include "config.h"

#ifdef __linux
#include <linux/major.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "boot/console.h"
#include "poutput.h"
#include "poutput-curses.h"
#ifdef HAVE_X11
#include "poutput-x11.h"
#endif
#ifdef HAVE_SDL
#include "poutput-sdl.h"
#endif
#ifdef HAVE_SDL2
#include "poutput-sdl2.h"
#endif
#ifdef HAVE_FRAMEBUFFER
#include "poutput-vcsa.h"
#endif
#include "latin1.h"
#include "utf-8.h"

static void (*console_clean)(void)=NULL;

static void vgaMakePal (const struct configAPI_t *configAPI)
{
	int pal[16];
	char palstr[1024];
	int bg,fg;
	char scol[4];
	char const *ps2=NULL;

	strcpy (palstr, configAPI->GetProfileString2 (configAPI->ScreenSec, "screen", "palette", "0 1 2 3 4 5 6 7 8 9 A B C D E F"));

	for (bg=0; bg<16; bg++)
		pal[bg]=bg;

	bg=0;
	ps2=palstr;
	while (configAPI->GetSpaceListEntry (scol, &ps2, 2) && bg<16)
		pal[bg++]=strtol(scol,0,16)&0x0f;

	for (bg=0; bg<16; bg++)
		for (fg=0; fg<16; fg++)
			plpalette[16*bg+fg]=16*pal[bg]+pal[fg];
}

static int console_init(const struct configAPI_t *configAPI)
{
#ifdef __linux
	struct stat st;
	char _stdout[128];
	char _stdin[128];
# ifdef HAVE_FRAMEBUFFER
	int test_vcsa=0;
# endif
#endif

	vgaMakePal (configAPI);

	fprintf(stderr, "Initing console... \n");
	fflush(stderr);
	{
		const char *driver = configAPI->GetProfileString ("CommandLine", "d", NULL);
		if (driver)
		{
#ifndef _WIN32
			if (!strcmp(driver, "curses"))
			{
				if (!curses_init())
				{
					console_clean=curses_done;
					return 0;
				}
				fprintf(stderr, "curses init failed\n");
				return -1;
			} else
#endif
			if (!strcmp(driver, "x11"))
			{
#ifdef HAVE_X11
				if (!x11_init(1))
				{
					console_clean=x11_done;
					return 0;
				}
				fprintf(stderr, "X11 init failed\n");
#else
				fprintf(stderr, "X11 support not compiled in\n");
#endif
				return -1;
			} else if (!strcmp(driver, "vcsa"))
			{
#ifdef HAVE_FRAMEBUFFER
				memset (_stdin, 0, sizeof(_stdin));
				if (readlink("/proc/self/fd/0", _stdin, sizeof(_stdin)-1)<0)
				if (readlink("/dev/fd/0", _stdin, sizeof(_stdin)-1)<0)
				{
					fprintf(stderr, "Failed to read link /proc/self/fd/0\n");
					return -1;
				}
				if (stat(_stdin, &st))
				{
					fprintf(stderr, "stat failed on %s\n", _stdin);
					return -1;
				}
				if (((st.st_rdev&0xff00)>>8)!=TTY_MAJOR)
				{
					fprintf(stderr, "stdin is not a tty (%s)\n", _stdin);
					return -1;
				}
				if (!vcsa_init(st.st_rdev&0xff))
				{
					console_clean=vcsa_done;
					return 0;
				}
				fprintf(stderr, "vcsa init failed\n");
#else
				fprintf(stderr, "VCSA (and FB) driver not compiled in\n");
#endif
				return -1;
			} else if (!strcmp(driver, "sdl"))
			{
#ifdef HAVE_SDL
				if (!sdl_init())
				{
					console_clean=sdl_done;
					return 0;
				}
				fprintf(stderr, "SDL init failed\n");
#else
				fprintf(stderr, "SDL driver not compiled in\n");
#endif
				return -1;
			} else if (!strcmp(driver, "sdl2"))
			{
#ifdef HAVE_SDL2
				if (!sdl2_init())
				{
					console_clean=sdl2_done;
					return 0;
				}
				fprintf(stderr, "SDL2 init failed\n");
#else
				fprintf(stderr, "SDL2 driver not compiled in\n");
#endif
			}
		}
	}
#ifdef __linux
	memset (_stdin, 0, sizeof(_stdin));
	memset (_stdout, 0, sizeof(_stdout));

	if (readlink("/proc/self/fd/0", _stdin, sizeof(_stdin)-1)<0)
	if (readlink("/dev/fd/0", _stdin, sizeof(_stdin)-1)<0)
	{
		fprintf(stderr, "Failed to read link /proc/self/fd/0\n");
		return -1;
	}
	if (readlink("/proc/self/fd/1", _stdout, sizeof(_stdout)-1)<0)
	if (readlink("/dev/fd/1", _stdout, sizeof(_stdout)-1)<0)
	{
		fprintf(stderr, "Failed to read link /proc/self/fd/1\n");
		return -1;
	}
	if (strcmp(_stdin, _stdout))
	{
#ifdef HAVE_X11
		fprintf(stderr, "stdout and stdin does not come from the same device, trying X11\n");
		if (!x11_init(0))
		{
			console_clean=x11_done;
			return 0;
		}
#endif
#ifdef HAVE_SDL2
		fprintf(stderr, "stdout and stdin does not come from the same device, trying SDL2\n");
		if (!sdl2_init())
		{
			console_clean=sdl2_done;
			return 0;
		}
#endif
#ifdef HAVE_SDL
		fprintf(stderr, "stdout and stdin does not come from the same device, trying SDL\n");
		if (!sdl_init())
		{
			console_clean=sdl_done;
			return 0;
		}
#endif
		fprintf(stderr, "Failed to find a non-TTY driver\n");
		return -1;
	}

	if (stat(_stdin, &st))
	{
		fprintf(stderr, "stat failed on %s\n", _stdin);
		return -1;
	}

	if ((st.st_mode&S_IFMT)!=S_IFCHR)
	{
		fprintf(stderr, "stdout/stdin is not a character device\n");
		return -1;
	}

	switch ((st.st_rdev&0xff00)>>8)
	{
		case TTY_MAJOR:
			fprintf(stderr, "We have a tty, testing:\n    Framebuffer (/dev/fb)\n    VCSA (/dev/vcsa)\n    Curses\n");
#ifdef HAVE_FRAMEBUFFER
			test_vcsa=1;
#endif
			break;
		case UNIX98_PTY_SLAVE_MAJOR:
#if (UNIX98_PTY_MAJOR_COUNT>=2)
		case UNIX98_PTY_SLAVE_MAJOR+1:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=3)
		case UNIX98_PTY_SLAVE_MAJOR+2:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=4)
		case UNIX98_PTY_SLAVE_MAJOR+3:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=5)
		case UNIX98_PTY_SLAVE_MAJOR+4:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=6)
		case UNIX98_PTY_SLAVE_MAJOR+5:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=7)
		case UNIX98_PTY_SLAVE_MAJOR+6:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=8)
		case UNIX98_PTY_SLAVE_MAJOR+7:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=9)
		case UNIX98_PTY_SLAVE_MAJOR+8:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=10)
		case UNIX98_PTY_SLAVE_MAJOR+9:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=11)
		case UNIX98_PTY_SLAVE_MAJOR+10:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=12)
		case UNIX98_PTY_SLAVE_MAJOR+11:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=13)
		case UNIX98_PTY_SLAVE_MAJOR+12:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=14)
		case UNIX98_PTY_SLAVE_MAJOR+13:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=15)
		case UNIX98_PTY_SLAVE_MAJOR+14:
#endif
#if (UNIX98_PTY_MAJOR_COUNT>=16)
		case UNIX98_PTY_SLAVE_MAJOR+15:
#endif
			fprintf(stderr, "We have a PTY (so no need to test for framebuffer and/or vcsa)\n");
			break;
		default:
			fprintf(stderr, "We have an unknown console type (so no need to test for framebuffer and/or vcsa)\n");
			break;
	}

#ifdef HAVE_FRAMEBUFFER
	if (test_vcsa)
	{
		if (!vcsa_init(st.st_rdev&0xff))
		{
			console_clean=vcsa_done;
			return 0;
		}
	}
#endif

#endif

#ifdef HAVE_X11
	if (!x11_init(0))
	{
		console_clean=x11_done;
		return 0;
	}
#endif

#ifdef HAVE_SDL2
	if (!sdl2_init())
	{
		console_clean=sdl2_done;
		return 0;
	}
#endif

#ifdef HAVE_SDL
	if (!sdl_init())
	{
		console_clean=sdl_done;
		return 0;
	}
#endif

#ifndef _WIN32
	if (!curses_init())
	{
		console_clean=curses_done;
		return 0;
	}
#endif

	return -1;
}

static void console_done(void)
{
	if (console_clean)
	{
		console_clean();
		console_clean=NULL;
	}
	Console.Driver = &dummyConsoleDriver;
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "poutput", .desc = "OpenCP Output Routines (c) 1994-'26 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 1, .Init = console_init, .Close = console_done};
