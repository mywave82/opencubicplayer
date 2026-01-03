/* OpenCP Module Player
 * copyright (c) 2022-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Font debugger
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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "boot/console.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "filesel/pfilesel.h"
#include "stuff/cp437.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/poutput-fontengine.h"
#include "stuff/poutput-swtext.h"

static uint32_t unicode = 0xfffffffe;

static void up_check(void)
{
	if ((unicode >= 0x3134F) && (unicode < 0xe0000)) unicode = 0xe0000;
	if ((unicode >= 0xE01EF) && (unicode < 0x0F0000)) unicode = 0xF0000;
	if (unicode >= 0xFFFFD) unicode = 0xFFF00;
}

static void down_check(void)
{
	if ((unicode >= 0x3134F) && (unicode < 0xe0000)) unicode = 0x31300;
	if ((unicode >= 0xE01EF) && (unicode < 0x0F0000)) unicode = 0xE0100;
}

static int fontdebugAProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(KEY_UP, "Jump unicode far up");
			cpiKeyHelp(KEY_DOWN, "Jump unicode far down");
			cpiKeyHelp(KEY_RIGHT, "Jump unicode up");
			cpiKeyHelp(KEY_LEFT, "Jump unicode down");
			return 0;
		case KEY_UP:
			if (unicode == 0xfffffffe) { unicode = 0xffffffff; break; }
			if (unicode == 0xffffffff) { unicode = 0x00000000; break; }
			unicode += 0x1000;
			up_check();
			break;
		case KEY_DOWN:
			if (unicode == 0xfffffffe) {                       break; }
			if (unicode == 0xffffffff) { unicode = 0xfffffffe; break; }
			if (unicode == 0x00000000) { unicode = 0xffffffff; break; }
			if (unicode < 0x1000)
			{
				unicode = 0x0;
			} else {
				unicode -= 0x1000;
			}
			down_check();
			break;
		case KEY_RIGHT:
			if (unicode == 0xfffffffe) { unicode = 0xffffffff; break; }
			if (unicode == 0xffffffff) { unicode = 0x00000000; break; }
			unicode += 0x100;
			up_check();
			break;
		case KEY_LEFT:
			if (unicode == 0xfffffffe) {                       break; }
			if (unicode == 0xffffffff) { unicode = 0xfffffffe; break; }
			if (unicode == 0x00000000) { unicode = 0xffffffff; break; }
			if (unicode < 0x100)
			{
				unicode = 0x0;
			} else {
				unicode -= 0x100;
			}
			down_check();
			break;
		default:
			return 0;
	}
	return 1;
}

static int fontdebugIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case 'R':
			cpiSetMode("FontDebug");
			break;
		default:
		return 0;
	}
	return 1;
}

static int fontdebugSetMode(struct cpifaceSessionAPI_t *cpifaceSession)
{
	plSetTextMode(fsScrType);
	fsScrType=plScrType;
	return 0;
}

static void fontdebugDisplayText_16x32(int x, int y, uint32_t *text)
{
	if (plScrLines < ((y + 1) * 32)) return;

	for (;text[0]; text++)
	{
		int width;
		uint8_t data[128];

		if ((unicode == 0xffffffff) || (unicode == 0xfffffffe))
		{
			uint8_t *b = fontengine_16x32 (text[0], &width);
			memcpy (data, b, width * 4);
		} else {
			fontengine_16x32_forceunifont (text[0], &width, data);
		}

		if (width == 32)
		{
			if (plScrLineBytes < ((x + 2) * 16)) return;
			swtext_displaycharattr_double16x32 (y, x, data, 0x0f);
			x+=2;
			text++;
		} else {
			if (plScrLineBytes < ((x + 1) * 16)) return;
			swtext_displaycharattr_single16x32 (y, x, data, 0x0f);
			x++;
		}
	}
}

static void fontdebugDisplayText_8x16(int x, int y, uint32_t *text)
{
	if (plScrLines < ((y + 1) * 16)) return;

	for (;text[0]; text++)
	{
		int width;
		uint8_t data[32];

		if ((unicode == 0xffffffff) || (unicode == 0xfffffffe))
		{
			uint8_t *b = fontengine_8x16 (text[0], &width);
			memcpy (data, b, width * 2);
		} else {
			fontengine_8x16_forceunifont (text[0], &width, data);
		}

		if (width == 16)
		{
			if (plScrLineBytes < ((x + 2) * 8)) return;
			swtext_displaycharattr_double8x16 (y, x, data, 0x0f);
			x+=2;
			text++;
		} else {
			if (plScrLineBytes < ((x + 1) * 8)) return;
			swtext_displaycharattr_single8x16 (y, x, data, 0x0f);
			x++;
		}
	}
}

static void fontdebugDisplayText_8x8(int x, int y, uint32_t *text)
{
	if (plScrLines < ((y + 1) * 8)) return;

	for (;text[0]; text++)
	{
		int width;
		uint8_t data[16];

		if ((unicode == 0xffffffff) || (unicode == 0xfffffffe))
		{
			uint8_t *b = fontengine_8x8 (text[0], &width);
			memcpy (data, b, width);
		} else {
			fontengine_8x8_forceunifont (text[0], &width, data);
		}

		if (width == 16)
		{
			if (plScrLineBytes < ((x + 2) * 8)) return;
			swtext_displaycharattr_double8x8 (y, x, data, 0x0f);
			x+=2;
			text++;
		} else {
			if (plScrLineBytes < ((x + 1) * 8)) return;
			swtext_displaycharattr_single8x8 (y, x, data, 0x0f);
			x++;
		}

	}
}

static void fontdebugDraw (struct cpifaceSessionAPI_t *cpifaceSession)
{
#define POSX_32   0
#define POSX_16 100
#define POSX_8  150

	int i, j;
	uint8_t header8[128];
	uint32_t header32[128];

	cpiDrawGStrings (cpifaceSession);
	if (unicode == 0xffffffff)
	{
		snprintf ((char *)header8, sizeof (header8), "Latin1 OCP font");
	} else if (unicode == 0xfffffffe)
	{
		snprintf ((char *)header8, sizeof (header8), "cp437 OCP font");
	} else {
		snprintf ((char *)header8, sizeof (header8), "U+%06x - U+%06x", unicode, unicode+0xff);
	}
	for (i=0; header8[i]; i++)
	{
		header32[i] = header8[i];
	}
	header32[i] = header8[i];
	fontdebugDisplayText_8x16(0, 5, header32);

	header32[0] = 0x250c;
	for (i=1; i < 16*3; i++)
	{
		header32[i] = ( i % 3 ) == 0 ? 0x252c : 0x2500;
	}
	header32[i] = 0x2510;
	header32[i+1] = 0;

	fontdebugDisplayText_8x8 (POSX_8 , 12, header32); /* top */
	fontdebugDisplayText_8x16(POSX_16,  6, header32);
	fontdebugDisplayText_16x32(POSX_32,  3, header32);

	header32[0] = 0x2514;
	for (i=1; i < 16*3; i++)
	{
		header32[i] = ( i % 3 ) == 0 ? 0x2534 : 0x2500;
	}
	header32[i] = 0x2518;
	fontdebugDisplayText_8x8 (POSX_8 , 12+16*2, header32); /* bottom */
	fontdebugDisplayText_8x16(POSX_16,  6+16*2, header32);
	fontdebugDisplayText_16x32(POSX_32,  3+16*2, header32);

	for (j=0; j < 15; j++)
	{
		header32[0] = 0x251c;
		for (i=1; i < 16*3; i++)
		{
			header32[i] = ( i % 3 ) == 0 ? 0x253c : 0x2500;
		}
		header32[i] = 0x2524;
		fontdebugDisplayText_8x8 (POSX_8 , 12+2+j*2, header32); /* intra */
		fontdebugDisplayText_8x16(POSX_16,  6+2+j*2, header32);
		fontdebugDisplayText_16x32(POSX_32,  3+2+j*2, header32);
	}

	for (j=0; j < 16; j++)
	{
		header32[0] = 0x2502;
		for (i=0; i < 16; i++)
		{
			if (unicode == 0xffffffff)
			{
				header32[i*3+1] = j*16 + i;
			} else if (unicode == 0xfffffffe)
			{
				header32[i*3+1] = ocp_cp437_to_unicode[j*16 + i];
			} else {
				header32[i*3+1] = unicode + j*16 + i;
			}
			if (!header32[i*3+1]) header32[i*3+1]=32;
			header32[i*3+2] = ' ';
			header32[i*3+3] = 0x2502;
		}
		fontdebugDisplayText_8x8 (POSX_8 , 12+1+j*2, header32); /* data */
		fontdebugDisplayText_8x16(POSX_16,  6+1+j*2, header32);
		fontdebugDisplayText_16x32(POSX_32,  3+1+j*2, header32);
	}
}

static int fontdebugEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievOpen:
			return (Console.Driver->DisplayStr_utf8 == swtext_displaystr_utf8);
		default:
		case cpievClose:
		case cpievInit:
		case cpievDone:
		case cpievInitAll:
		case cpievDoneAll:
			return 1;
	}
}

struct cpimoderegstruct cpiModeFontDebug = {"FontDebug", fontdebugSetMode, fontdebugDraw, fontdebugIProcessKey, fontdebugAProcessKey, fontdebugEvent CPIMODEREGSTRUCT_TAIL};

static int FontDebugInit(struct PluginInitAPI_t *API)
{
	cpiRegisterDefMode(&cpiModeFontDebug);
	return errOk;
}

static void FontDebugClose(struct PluginCloseAPI_t *API)
{
	cpiUnregisterDefMode(&cpiModeFontDebug);
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "cpifontdebug", .desc = "OpenCP Font Debugger (c) 2022-'26 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 40, .LateInit = FontDebugInit, .PreClose = FontDebugClose};
/* OpenCP Module Player */
