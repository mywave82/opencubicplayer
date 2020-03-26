/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '11-'20 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "poutput-fb.h"
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

static void reset_api(void);
static void (*console_clean)(void)=NULL;

static void vgaMakePal(void)
{
	int pal[16];
	char palstr[1024];
	int bg,fg;
	char scol[4];
	char const *ps2=NULL;

	strcpy(palstr,cfGetProfileString2(cfScreenSec, "screen", "palette", "0 1 2 3 4 5 6 7 8 9 A B C D E F"));

	for (bg=0; bg<16; bg++)
		pal[bg]=bg;

	bg=0;
	ps2=palstr;
	while (cfGetSpaceListEntry(scol, &ps2, 2) && bg<16)
		pal[bg++]=strtol(scol,0,16)&0x0f;

	for (bg=0; bg<16; bg++)
		for (fg=0; fg<16; fg++)
			plpalette[16*bg+fg]=16*pal[bg]+pal[fg];
}
static int console_init(void)
{
	struct stat st;
#ifdef __linux
	char _stdout[128];
	char _stdin[128];
# ifdef HAVE_FRAMEBUFFER
	int test_fb=0;
	int test_vcsa=0;
# endif
#endif

	vgaMakePal();

	reset_api();

	fprintf(stderr, "Initing console... \n");
	fflush(stderr);
	{
		const char *driver = cfGetProfileString("CommandLine", "d", NULL);
		if (driver)
		{
			if (!strcmp(driver, "curses"))
			{
				if (!curses_init())
				{
					console_clean=curses_done;
					return 0;
				}
				fprintf(stderr, "curses init failed\n");
				return -1;
			} else if (!strcmp(driver, "x11"))
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
				memset(_stdin, 0, sizeof(_stdin));
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
					fprintf(stderr, "stdin is not a tty\n");
					return -1;
				}
				if (!vcsa_init(st.st_rdev&0xff))
				{
					console_clean=vcsa_done;
					fb_init(st.st_rdev&0xff);
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
	memset(_stdin, 0, sizeof(_stdin));
	memset(_stdout, 0, sizeof(_stdout));

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
			test_fb=1;
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
			if (test_fb)
				fb_init(st.st_rdev&0xff);
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

	if (!curses_init())
	{
		console_clean=curses_done;
		return 0;
	}

	return -1;
}

static void console_done(void)
{
	if (console_clean)
	{
		console_clean();
		console_clean=NULL;
	}
	reset_api();
}

static void __plSetTextMode(unsigned char x)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "plSetTextMode not implemented in this console driver\n");
#endif
}
static void __plSetBarFont(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "plSetBarFont not implemented in this console driver\n");
#endif
}
static void __displaystr(unsigned short y, unsigned short x, unsigned char attr, const char *str, unsigned short len)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "displaystr not implemented in this console driver\n");
#endif
}
static void __displaystrattr(unsigned short y, unsigned short x, const unsigned short *buf, unsigned short len)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "displaystrattr not implemented in this console driver\n");
#endif
}
static void __displayvoid(unsigned short y, unsigned short x, unsigned short len)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "displayvoid not implemented in this console driver\n");
#endif
}
static void __displaystr_iso8859latin1(unsigned short y, unsigned short x, unsigned char attr, const char *str, unsigned short len)
{ /* fallback */
	while (len)
	{
		char temp = latin1_table[*(unsigned char *)str];
		_displaystr(y, x, attr, &temp, 1);
		len--;
		if (*str)
		{
			str++;
		}
		x++;
	}
}
static void __displaystrattr_iso8859latin1(unsigned short y, unsigned short x, const unsigned short *buf, unsigned short len)
{ /* fallback */
	while (len)
	{
		unsigned short temp = latin1_table[(*buf) & 0xff] | (*buf & 0xff00);
		_displaystrattr(y, x, &temp, 1);
		len--;
		if (*buf)
		{
			buf++;
		}
		x++;
	}

}
static void __displaystr_utf8(unsigned short y, unsigned short x, unsigned char attr, const char *str, unsigned short len)
{ /* fallback */
	while (len)
	{
		int codepoint;
		int inc = 0;
		uint8_t temp;
		codepoint = utf8_decode (str, strlen (str), &inc);
		str += inc;
		if (codepoint > 255)
		{
			temp = '?';
		} else {
			temp = codepoint;
		}
		_displaystr_iso8859latin1(y, x, attr, (char *)&temp, 1);
		len--;
		x++;
	}
}

static int __plSetGraphMode(int size)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "plSetGraphMode not implemented in this console driver\n");
#endif
	return -1;
}
static void __gdrawchar(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gdrawchar not implemented in this console driver\n");
#endif
}
static void __gdrawchart(unsigned short x, unsigned short y, unsigned char c, unsigned char f)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gdrawchart not implemented in this console driver\n");
#endif
}
static void __gdrawcharp(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gdrawcharp not implemented in this console driver\n");
#endif
}
static void __gdrawchar8(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gdrawchar8 not implemented in this console driver\n");
#endif
}
static void __gdrawchar8t(unsigned short x, unsigned short y, unsigned char c, unsigned char f)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gdrawchar8t not implemented in this console driver\n");
#endif
}
static void __gdrawchar8p(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gdrawchar8p not implemented in this console driver\n");
#endif
}
static void __gdrawstr(unsigned short y, unsigned short x, const char *s, unsigned short len, unsigned char f, unsigned char b)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gdrawstr not implemented in this console driver\n");
#endif
}
static void __gupdatestr(unsigned short y, unsigned short x, const uint16_t *str, unsigned short len, uint16_t *old)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gupdatestr not implemented in this console driver\n");
#endif
}
static void __gupdatepal(unsigned char color, unsigned char red, unsigned char green, unsigned char blue)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gupdatepal not implemented in this console driver\n");
#endif
}
static void __gflushpal(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "gflushpal not implemented in this console driver\n");
#endif
}

static int __ekbhit(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "ekbhit not implemented in this console driver\n");
#endif
	return 0;
}
static int __egetch(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "egetch not implemented in this console driver\n");
#endif
	return 0;
}
static int __validkey(uint16_t key)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "validkey not implemented in this console driver\n");
#endif
	return 0;
}
static void __drawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "drawbar not implemented in this console driver\n");
#endif
}

static void __idrawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "idrawbar not implemented in this console driver\n");
#endif
}

static void __Screenshot(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "Screenshot not implemented in this console driver\n");
#endif
}
static void __TextScreenshot(int scrType)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "TextScreenshot not implemented in this console driver\n");
#endif
}
static void __setcur(uint16_t y, uint16_t x)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "setcur not implemented in this console driver\n");
#endif
}
static void __setcurshape(unsigned short shape)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "setcurshape not implemented in this console driver\n");
#endif
}
static int __conRestore(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "conRestore not implemented in this console driver\n");
#endif
	return 1;
}
static void __conSave(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "conSave not implemented in this console driver\n");
#endif
}
static void __plDosShell(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "plDosShell not implemented in this console driver\n");
#endif
}
static void __plDisplaySetupTextMode(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "plDisplaySetupTextMode not implemented in this console driver\n");
#endif
}
static const char *__plGetDisplayTextModeName(void)
{
#ifdef CONSOLE_DEBUG
	fprintf(stderr, "plGetDisplayTextModeName not implemented in this console driver\n");
#endif
	return "unknown";
}


static void reset_api(void)
{
	_plSetTextMode=__plSetTextMode;
	_plSetBarFont=__plSetBarFont;
	_displaystr=__displaystr;
	_displaystrattr=__displaystrattr;
	_displayvoid=__displayvoid;

	_displaystr_iso8859latin1=__displaystr_iso8859latin1;
	_displaystrattr_iso8859latin1=__displaystrattr_iso8859latin1;
	_displaystr_utf8=__displaystr_utf8;

	_plDisplaySetupTextMode=__plDisplaySetupTextMode;
	_plGetDisplayTextModeName=__plGetDisplayTextModeName;

	_plSetGraphMode=__plSetGraphMode;
	_gdrawchar=__gdrawchar;
	_gdrawchart=__gdrawchart;
	_gdrawcharp=__gdrawcharp;
	_gdrawchar8=__gdrawchar8;
	_gdrawchar8t=__gdrawchar8t;
	_gdrawchar8p=__gdrawchar8p;
	_gdrawstr=__gdrawstr;
	_gupdatestr=__gupdatestr;
	_gupdatepal=__gupdatepal;
	_gflushpal=__gflushpal;

	_ekbhit=__ekbhit;
	_egetch=__egetch;
	_validkey=__validkey;

	_drawbar=__drawbar;
	_idrawbar=__idrawbar;

	_Screenshot=__Screenshot;
	_TextScreenshot=__TextScreenshot;
	_setcur=__setcur;
	_setcurshape=__setcurshape;

	_conRestore=__conRestore;
	_conSave=__conSave;

	_plDosShell=__plDosShell;
	_vga13=NULL;
}
#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "poutput", .desc = "OpenCP Output Routines (c) 1994-10 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0, .Init = console_init, .Close = console_done};
