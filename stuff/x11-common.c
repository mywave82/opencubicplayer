/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * X11 common stuff
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
 *  -ss050429   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#define _CONSOLE_DRIVER
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include "types.h"
#include "poutput.h"
#include "x11-common.h"

static uint16_t red[256]=  {0x0000, 0x0000, 0x0000, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0x5555, 0x5555, 0x5555, 0x5555, 0xffff, 0xffff, 0xffff, 0xffff};
static uint16_t green[256]={0x0000, 0x0000, 0xaaaa, 0xaaaa, 0x0000, 0x0000, 0x5555, 0xaaaa, 0x5555, 0x5555, 0xffff, 0xffff, 0x5555, 0x5555, 0xffff, 0xffff};
static uint16_t blue[256]= {0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff};

uint32_t x11_palette32[256];
uint16_t x11_palette16[256];
uint16_t x11_palette15[256];

Display *mDisplay = NULL;
int mLocalDisplay=0;
int mScreen;
int x11_depth=8;

void x11_gUpdatePal (uint8_t color, uint8_t _red, uint8_t _green, uint8_t _blue)
{
	red[color]=_red<<10;
	green[color]=_green<<10;
	blue[color]=_blue<<10;
}

void x11_gFlushPal (void)
{
	int i, r, g, b;
	if (x11_depth==8)
	{
		Colormap cmap=0;

		cmap=XCreateColormap(mDisplay, mScreen, XDefaultVisual(mDisplay, mScreen), AllocAll);
		for (i = 0; i < 256; i++)
		{
			XColor xcol;

			xcol.pixel = i;
			xcol.red = red[i];
			xcol.green = green[i];
			xcol.blue = blue[i];
			xcol.flags = DoBlue | DoGreen | DoRed;
			XStoreColor(mDisplay, cmap, &xcol);
		}
		XInstallColormap(mDisplay, cmap);
		XFreeColormap(mDisplay, cmap);
		return;
	}
	for (i=0;i<256;i++)
	{
		r=red[i]>>8;
		g=green[i]>>8;
		b=blue[i]>>8;
		x11_palette32[i]=(r<<16)+(g<<8)+b;
		r>>=3;
		g>>=2;
		b>>=3;
		x11_palette16[i]=(r<<11)+(g<<5)+b;
		g>>=1;
		x11_palette15[i]=(r<<10)+(g<<5)+b;
	}
}

static int inited=0;

int x11_connect (void)
{
	char *dispName;

	if (inited++)
		return !mDisplay;

	dispName = XDisplayName(NULL);

	if ( (mDisplay = XOpenDisplay(dispName)) == NULL )
	{
		fprintf(stderr, "[x11] can't connect to X server %s\n", XDisplayName(NULL));
		return -1;
	}
	fprintf(stderr, "[x11] X is online\n");

	if (strncmp(dispName, "unix:", 5) == 0)
		dispName += 4;
	else if (strncmp(dispName, "localhost:", 10) == 0)
		dispName += 9;
	if (*dispName == ':' && atoi(dispName + 1) < 10)
		mLocalDisplay = 1;
	else
		mLocalDisplay = 0;

	mScreen=DefaultScreen(mDisplay);

	return 0;
}

void x11_disconnect (void)
{
	if (!inited)
		return;
	if (!(--inited))
	{
		XCloseDisplay (mDisplay);
		mDisplay = NULL;
	}
}
