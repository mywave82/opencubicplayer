/* OpenCP Module Player
 * copyright (c) '08-'20 Stian Skjelstad <stian@nixia.no>
 *
 * Keyboard shortcut help-browser.
 *'
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
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "stuff/poutput.h"
#include "stuff/framelock.h"
#include "cpiface.h"

struct cpiKeyHelpKeyName
{
	uint16_t key;
	const char *name;
};

static struct cpiKeyHelpKeyName KeyNames[] =
{
	{' ', "spacebar"},
	{KEY_ESC, "esc"},
	{KEY_NPAGE, "page down"},
	{KEY_PPAGE, "page up"},
	{KEY_HOME, "home"},
	{KEY_END, "end"},
	{KEY_TAB, "tab"},
	{KEY_SHIFT_TAB, "shift+tab"},
	{_KEY_ENTER, "enter"},
	{KEY_CTRL_H, "ctrl+h"},
	{KEY_CTRL_P, "ctrl+p"},
	{KEY_CTRL_D, "ctrl+d"},
	{KEY_CTRL_J, "ctrl+j"},
	{KEY_CTRL_K, "ctrl+k"},
	{KEY_CTRL_L, "ctrl+l"},
	{KEY_CTRL_Q, "ctrl+q"},
	{KEY_CTRL_S, "ctrl+s"},
	{KEY_CTRL_Z, "ctrl+z"},
	{KEY_CTRL_BS, "ctrl+backspace"},
	{KEY_CTRL_HOME, "ctrl+home"},
	{KEY_DOWN, "down"},
	{KEY_UP, "up"},
	{KEY_LEFT, "left"},
	{KEY_RIGHT, "right"},
	{KEY_ALT_A, "alt+a"},
	{KEY_ALT_B, "alt+b"},
	{KEY_ALT_C, "alt+c"},
	{KEY_ALT_E, "alt+e"},
	{KEY_ALT_G, "alt+g"},
	{KEY_ALT_I, "alt+i"},
	{KEY_ALT_K, "alt+k"},
	{KEY_ALT_L, "alt+l"},
	{KEY_ALT_M, "alt+m"},
	{KEY_ALT_O, "alt+o"},
	{KEY_ALT_P, "alt+p"},
	{KEY_ALT_R, "alt+r"},
	{KEY_ALT_S, "alt+s"},
	{KEY_ALT_X, "alt+x"},
	{KEY_ALT_Z, "alt+z"},
	{KEY_BACKSPACE, "backspace"},
	{KEY_F(1), "f1"},
	{KEY_F(2), "f2"},
	{KEY_F(3), "f3"},
	{KEY_F(4), "f4"},
	{KEY_F(5), "f5"},
	{KEY_F(6), "f6"},
	{KEY_F(7), "f7"},
	{KEY_F(8), "f8"},
	{KEY_F(9), "f9"},
	{KEY_F(10), "f10"},
	{KEY_F(11), "f11"},
	{KEY_F(12), "f12"},
	{KEY_CTRL_UP, "ctrl+up"},
	{KEY_CTRL_DOWN, "ctrl+down"},
	{KEY_CTRL_LEFT, "ctrl+left"},
	{KEY_CTRL_RIGHT, "ctrl+right"},
	{KEY_CTRL_PGUP, "ctrl+page up"},
	{KEY_CTRL_PGDN, "ctrl+page down"},
	{KEY_CTRL_END, "ctrl+end"},
	{KEY_CTRL_DELETE, "ctrl+delete"},
	{KEY_CTRL_INSERT, "ctrl+insert"},
	{KEY_DELETE, "delete"},
	{KEY_INSERT, "insert"},
	{KEY_ALT_ENTER, "alt+enter"},
	{KEY_CTRL_ENTER, "ctrl+enter"},
	{'a', "a"},
	{'b', "b"},
	{'c', "c"},
	{'d', "d"},
	{'e', "e"},
	{'f', "f"},
	{'g', "g"},
	{'h', "h"},
	{'i', "i"},
	{'j', "j"},
	{'k', "k"},
	{'l', "l"},
	{'m', "m"},
	{'n', "n"},
	{'o', "o"},
	{'p', "p"},
	{'q', "q"},
	{'r', "r"},
	{'s', "s"},
	{'t', "t"},
	{'u', "u"},
	{'v', "v"},
	{'w', "w"},
	{'x', "x"},
	{'y', "y"},
	{'z', "z"},
	{'A', "A"},
	{'B', "B"},
	{'C', "C"},
	{'D', "D"},
	{'E', "E"},
	{'F', "F"},
	{'G', "G"},
	{'H', "H"},
	{'I', "I"},
	{'J', "J"},
	{'K', "K"},
	{'L', "L"},
	{'M', "M"},
	{'N', "N"},
	{'O', "O"},
	{'P', "P"},
	{'Q', "Q"},
	{'R', "R"},
	{'S', "S"},
	{'T', "T"},
	{'U', "U"},
	{'V', "V"},
	{'W', "W"},
	{'X', "X"},
	{'Y', "Y"},
	{'Z', "Z"},
	{'1', "1"},
	{'2', "2"},
	{'3', "3"},
	{'4', "4"},
	{'5', "5"},
	{'6', "6"},
	{'7', "7"},
	{'8', "8"},
	{'9', "9"},
	{'0', "0"},
	{'/', "/"},
	{'*', "*"},
	{'-', "-"},
	{'+', "+"},
	{'\\', "\\"},
	{'\'', "\'"},
	{',', ","},
	{'.', "."},
	{'?', "?"},
	{'!', "!"},
	{'>', ">"},
	{'<', "<"},
	{'|', "|"},
};

static unsigned int keymapping_n = 0;
static struct cpiKeyHelpKeyName keymapping[sizeof(KeyNames)/sizeof(struct cpiKeyHelpKeyName)];


void cpiKeyHelp(uint16_t key, const char *shorthelp)
{
	unsigned int i;
	if ((keymapping_n+1)>=(sizeof(KeyNames)/sizeof(struct cpiKeyHelpKeyName)))
	{
		fprintf(stderr, __FILE__ ": Too many keys\n");
		return;
	}
	for (i=0;i<keymapping_n;i++)
		if (keymapping[i].key==key)
			return;
	keymapping[keymapping_n].key=key;
	keymapping[keymapping_n].name = shorthelp;
	keymapping_n++;
}

static unsigned int top;
static unsigned int height;
static unsigned int left;
static unsigned int width;
static unsigned int offset;
static unsigned int insecret;
static unsigned int vpos;

static void DrawBoxCommon(void)
{
	int i;
	int widest = 0;
	for (i=0; i < keymapping_n; i++)
	{
		int len = strlen (keymapping[i].name);
		if (len > widest)
		{
			widest = len;
		}
	}

	height = plScrHeight-4;
	width = plScrWidth-4;

	if (height > keymapping_n)
	{
		height = keymapping_n;
	}

	if (width > (widest + 15))
	{
		width = widest + 15;
	}

	top = (plScrHeight - height) / 2;
	left = (plScrWidth - width) / 2;

	if (height < keymapping_n)
	{
		vpos = offset * (height - 1) / (keymapping_n - height);
	}
}

static void gDrawBox(void)
{
	unsigned int i;

	DrawBoxCommon ();

	gdrawstr(top-1, left-1, "\xda", 1, 0x04, 0x00);
	for (i=left;i<(left+width+1);i++)
		gdrawstr(top-1, i, "\xc4", 1, 0x04, 0x00);
	gdrawstr(top-1, left + width/2 - 10, " Keyboard short-cuts ", 21, 0x04, 0x00);
	gdrawstr(top-1, left+width+1, "\xbf", 1, 0x04, 0x00);
	for (i=0; i<height; i++)
	{
		gdrawstr(i + top, left         - 1,           "\xb3",        1, 0x04, 0x00);
		gdrawstr(i + top, left + width + 1, (i!=vpos)?"\xb3":"\xdd", 1, 0x04, 0x00);
	}
	gdrawstr(top+height, left-1, "\xc0", 1, 0x04, 0x00);
	for (i=left;i<(left+width+1);i++)
		gdrawstr(top+height, i, "\xc4", 1, 0x04, 0x00);
	gdrawstr(top+height, left+width+1, "\xd9", 1, 0x04, 0x00);
}
static void DrawBox(void)
{
	unsigned int i;

	DrawBoxCommon ();

	displaystr(top-1, left-1, 0x04, "\xda", 1);
	for (i=left;i<(left+width+1);i++)
		displaystr(top-1, i, 0x04, "\xc4", 1);
	displaystr(top-1, left + width/2 - 10, 0x04, " Keyboard short-cuts ", 21);
	displaystr(top-1, left + width + 1, 0x04, "\xbf", 1);
	for (i=0; i<height; i++)
	{
		displaystr (i + top, left         - 1, 0x04,           "\xb3",        1);
		displaystr (i + top, left + width + 1, 0x04, (i!=vpos)?"\xb3":"\xdd", 1);
	}
	displaystr(top+height, left-1, 0x04, "\xc0", 1);
	for (i=left;i<(left+width+1);i++)
		displaystr(top+height, i, 0x04, "\xc4", 1);
	displaystr(top+height, left+width+1, 0x04, "\xd9", 1);
}

static int SecretFontDisplay(void)
{
	static int fontDisplay = 0;

	char buffer[80];
	int i;
	memset (buffer, 32, sizeof (buffer));
	switch (fontDisplay)
	{
		default:
		case 0:
			fontDisplay=0;
			displaystr (2, 1, 0x04, " =-=-=-= CP437+OCP =-=-=-=" , 78);
			displaystr (3, 1, 0x04, "   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F", 78);
			for (i=0; i < 16; i++)
			{
				buffer[1] = "0123456789ABCDEF"[i];
				buffer[3] = i ? i * 16  :32;
				buffer[6] = i * 16 + 1;
				buffer[9] = i * 16 + 2;
				buffer[12] = i * 16 + 3;
				buffer[15] = i * 16 + 4;
				buffer[18] = i * 16 + 5;
				buffer[21] = i * 16 + 6;
				buffer[24] = i * 16 + 7;
				buffer[27] = i * 16 + 8;
				buffer[30] = i * 16 + 9;
				buffer[33] = i * 16 + 10;
				buffer[36] = i * 16 + 11;
				buffer[39] = i * 16 + 12;
				buffer[42] = i * 16 + 13;
				buffer[45] = i * 16 + 14;
				buffer[48] = i * 16 + 15;
				displaystr (4 + i, 1, 0x04, buffer, 78);
			}
			break;
		case 1:
			displaystr_iso8859latin1 (2, 1, 0x04, " =-=-=-= ISO8859-LATIN1 =-=-=-=" , 78);
			displaystr_iso8859latin1 (3, 1, 0x04, "   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F", 78);
			for (i=0; i < 16; i++)
			{
				buffer[1] = "0123456789ABCDEF"[i];
				buffer[3] = i ? i * 16  :32;
				buffer[6] = i * 16 + 1;
				buffer[9] = i * 16 + 2;
				buffer[12] = i * 16 + 3;
				buffer[15] = i * 16 + 4;
				buffer[18] = i * 16 + 5;
				buffer[21] = i * 16 + 6;
				buffer[24] = i * 16 + 7;
				buffer[27] = i * 16 + 8;
				buffer[30] = i * 16 + 9;
				buffer[33] = i * 16 + 10;
				buffer[36] = i * 16 + 11;
				buffer[39] = i * 16 + 12;
				buffer[42] = i * 16 + 13;
				buffer[45] = i * 16 + 14;
				buffer[48] = i * 16 + 15;
				displaystr_iso8859latin1 (4 + i, 1, 0x04, buffer, 78);
			}
			break;
		case 2:
			displaystr_utf8 (2, 1, 0x04, " =-=-=-= UTF-8 =-=-=-=" , 78);
			displaystr_utf8 (3, 1, 0x04, " Norway: U+00E6=>\xc3\xa6 U+00F8=>\xc3\xb8 U+00E5=>\xc3\xa5 U+00C6=>\xc3\x86 U+00D8=>\xc3\x98 U+00C5=>\xc3\x85" , 78);
			displaystr_utf8 (4, 1, 0x04, " Cyrillic U+0411=>\xd0\x91 U+0414=>\xd0\x94 U+0418=>\xd0\x98", 78);
			displaystr_utf8 (5, 1, 0x04, " Linear B Syllabary: U+10002=>\xf0\x90\x80\x82 U+1002A=>\xf0\x90\x80\xaa U+1000F=>\xf0\x90\x80\x8f", 78);
			displaystr_utf8 (6, 1, 0x04, " Linear B Ideograms: U+100b1=>\xf0\x90\x82\xb1 U+100AF=>\xf0\x90\x82\xaf U+10083=>\xf0\x90\x82\x83", 78);
			break;

	}
	while (ekbhit())
	{
		switch (egetch())
		{
			case 'S':
			case ' ':
				fontDisplay++;
				break;
			case KEY_ESC:
				return 0;
		}
	}
	return 1;
}

void cpiKeyHelpClear(void)
{
	insecret=0;
	offset = 0;
	keymapping_n=0;
}

int cpiKeyHelpDisplay(void)
{
	unsigned int i, j;
	unsigned char cola, colb;

	if (insecret)
	{
		insecret = SecretFontDisplay();
		return 1;
	}

	if (!keymapping_n)
		return 0;

	if (keymapping_n <= height)
	{
		offset = 0;
	} else if ((keymapping_n-offset)<height)
	{
		offset = keymapping_n - height;
	}

	if ((plScrMode==100)||(plScrMode==101)||(plScrMode==13))
	{
		gDrawBox();
	} else {
		DrawBox();
	}

	for (j=0;j<(keymapping_n-offset);j++)
	{
		const char *s="unknown key";
		if (j>=height)
			break;
		for (i=0;i<(sizeof(KeyNames)/sizeof(struct cpiKeyHelpKeyName));i++)
		{
			if (KeyNames[i].key==keymapping[j+offset].key)
			{
				s=KeyNames[i].name;
				break;
			}
		}
		if (validkey(KeyNames[i].key))
		{
			cola=0x0f;
			colb=0x0a;
		} else {
			cola=0x01;
			colb=0x01;
		}
		if ((plScrMode==100)||(plScrMode==101)||(plScrMode==13))
		{
			gdrawstr(j+top, left, s, 16, colb, 0x00);
			gdrawstr(j+top, left+16, keymapping[j+offset].name, width-15, cola, 0x00);
		} else {
			displaystr(j+top, left, colb, s, 16);
			displaystr(j+top, left+16, cola, keymapping[j+offset].name, width-15);
		}
	}
	if ((plScrMode==100)||(plScrMode==101)||(plScrMode==13))
		for (;j<height;j++)
			gdrawstr(j+top, left, "", width, 0x00, 0x00);

	while (ekbhit())
	{
		uint16_t c = egetch();

		if (c == 'S')
		{
			insecret = 1;
			return 1;
		}

		if ( ((c >= 'a') && (c <= 'z')) ||
		     ((c >= 'A') && (c <= 'Z')) ||
		     ((c >= '0') && (c <= '9')) )
		{
			return 0;
		}
		switch (c)
		{
			case KEY_PPAGE:
			case KEY_UP:
				if (offset)
					offset--;
				break;
			case ' ':
			case KEY_NPAGE:
			case KEY_DOWN:
				if ((keymapping_n-offset)>height)
					offset++;
				break;
			case _KEY_ENTER:
			case KEY_ESC:
			case KEY_ALT_K:
				return 0;
		}
	}
	return 1;
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo="";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "keyhelp", .desc = "OpenCP Key Helper (c) 2007-09 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
