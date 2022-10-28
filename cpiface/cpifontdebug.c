#include "config.h"
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "boot/console.h"
#include "boot/plinkman.h"
#include "cpiface/cpiface.h"
#include "filesel/pfilesel.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/poutput-fontengine.h"
#include "stuff/poutput-swtext.h"

static uint32_t unicode;

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
			unicode += 0x1000;
			up_check();
			break;
		case KEY_DOWN:
			if (unicode < 0x1000)
			{
				unicode = 0x0;
			} else {
				unicode -= 0x1000;
			}
			down_check();
			break;
		case KEY_RIGHT:
			unicode += 0x100;
			up_check();
			break;
		case KEY_LEFT:
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

static void fontdebugSetMode(void)
{
	plSetTextMode(fsScrType);
	fsScrType=plScrType;
}

static void fontdebugDisplayText_8x16(int x, int y, uint32_t *text)
{
	if (plScrLines < ((y + 1) * 16)) return;

	for (;text[0]; text++)
	{
		int width;
		uint8_t data[32];

		fontengine_8x16_forceunifont (text[0], &width, data);

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

		fontengine_8x8_forceunifont (text[0], &width, data);
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
#define POSX_8  50
#define POSX_16 0

	int i, j;
	uint8_t header8[128];
	uint32_t header32[128];

	cpiDrawGStrings (cpifaceSession);
	snprintf ((char *)header8, sizeof (header8), "U+%06x - U+%06x", unicode, unicode+0xff);
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

	header32[0] = 0x2514;
	for (i=1; i < 16*3; i++)
	{
		header32[i] = ( i % 3 ) == 0 ? 0x2534 : 0x2500;
	}
	header32[i] = 0x2518;
	fontdebugDisplayText_8x8 (POSX_8 , 12+16*2, header32); /* bottom */
	fontdebugDisplayText_8x16(POSX_16,  6+16*2, header32);

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
	}

	for (j=0; j < 16; j++)
	{
		header32[0] = 0x2502;
		for (i=0; i < 16; i++)
		{
			header32[i*3+1] = unicode + j*16 + i;
			if (!header32[i*3+1]) header32[i*3+1]=32;
			header32[i*3+2] = ' ';
			header32[i*3+3] = 0x2502;
		}
		fontdebugDisplayText_8x8 (POSX_8 , 12+1+j*2, header32); /* data */
		fontdebugDisplayText_8x16(POSX_16,  6+1+j*2, header32);
	}
}

static int fontdebugEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievOpen:
			return (conDriver->DisplayStr_utf8 == swtext_displaystr_utf8);
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

static int FontDebugInit(void)
{
	cpiRegisterDefMode(&cpiModeFontDebug);
	return errOk;
}

static void FontDebugClose(void)
{
	cpiUnregisterDefMode(&cpiModeFontDebug);
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "cpifontdebug", .desc = "OpenCP Font Debugger (c) 2022 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 40, .LateInit = FontDebugInit, .PreClose = FontDebugClose};
/* OpenCP Module Player */
