/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelestad@gmail.com>
 *
 * Curses console driver
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
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040902   Stian Skjelstad <stian@nixia.no>
 *    -added setcur, setcurshape and plDosShell
 *    -made displaystrattr not use bugfix for first blank cell (to avoid black
 *     cursor)
 *  -doj040907  Dirk Jagdmann  <doj@cubic.org>
 *    -enchanced some of the entries in the chr_table
 *  -ss040909   Stian Skjelstad <stian@nixia.no>
 *    -Made NCURSES_BUGFIX1 a config option instead: [curses] fixbadgraphic
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -Tweaked conRestore, conSave and curses_init, so we get the bahavior we
 *     expect
 *  -ss040919   Stian Skjelstad <stian@nixia.no>
 *    -New logic for setcurshape
 *  -ss150104   Stian Skjelstad <stian.skjelstad@gmail.com>
 *    -Work around for broken write() support inside ncurses library when application receives SIGALRM
 */
#define _CONSOLE_DRIVER

#include "config.h"
/* we assume that ncursesw includes wchar.h if needed */
#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include "types.h"

#include "poutput-curses.h"
#include "poutput-keyboard.h"
#include "boot/console.h"
#include "poutput.h"
#include "poll.h"
#include "boot/psetting.h"
#include "cp437.h"
#include "utf-8.h"

#if defined(SIGWINCH) && defined(TIOCGWINSZ) && HAVE_RESIZE_TERM
#define CAN_RESIZE 1
#else
#undef CAN_RESIZE
#endif

#if CAN_RESIZE
static void adjust (int sig);
#endif

static NCURSES_ATTR_T attr_table[256];
static chtype chr_table[256];
static chtype chr_table_iso8859latin1[256];

static int Width, Height;

static int fixbadgraphic;

#ifdef HAVE_NCURSESW
static int useunicode = 0;
#endif

static iconv_t utf8_to_native = (iconv_t)-1;

static struct consoleDriver_t ncursesConsoleDriver;

static void ncurses_DisplayChr (uint16_t y, uint16_t x, uint8_t attr, char chr, uint16_t len)
{
	if (!len)
	{
		return;
	}
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		wchar_t buffer[CONSOLE_MAX_X+1];
		unsigned int i;

		for (i=0; i < len; i++)
		{
			buffer[i] = chr_table[(uint8_t)chr];
		}
		buffer[i] = 0;

		attrset (attr_table[plpalette[attr]]);
		mvaddwstr(y, x, buffer);
	} else
#endif
	{
		chtype output;

		if (((!chr)||(chr==' '))&&(!(attr&0x80))&&fixbadgraphic)
		{
			output=chr_table['X']|attr_table[plpalette[(attr&0xf0)+((attr>>4)&0xf)]];
		} else {
			output=chr_table[(uint8_t)chr]|attr_table[plpalette[attr]];
		}

		move(y, x);
		while(len)
		{
			addch(output);
			len--;
		}
	}
}

static void ncurses_DisplayStr (uint16_t y, uint16_t x, uint8_t attr, const char *buf, uint16_t len)
{
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		wchar_t buffer[CONSOLE_MAX_X+1];
		wchar_t *ptr = buffer;

		while(len)
		{
			unsigned char ch=(*buf)&0xff;
			*(ptr++) = chr_table[ch];

			if (*buf)
				buf++;
			len--;
		}
		attrset (attr_table[plpalette[attr]]);
		*ptr = 0;
		mvaddwstr(y, x, buffer);
	} else
#endif
	{
		move(y, x);
		while(len)
		{
			unsigned char ch=(*buf)&0xff;
			chtype output;
			if (((!ch)||(ch==' '))&&(!(attr&0x80))&&fixbadgraphic)
			{
				output=chr_table['X']|attr_table[plpalette[(attr&0xf0)+((attr>>4)&0xf)]];
				addch(output);
			} else {
				output=chr_table[ch]|attr_table[plpalette[attr]];
				addch(output);
			}

			if (*buf)
				buf++;
			len--;
		}
	}
}

static void ncurses_DisplayStr_utf8 (uint16_t y, uint16_t x, uint8_t attr, const char *buf, uint16_t len)
{
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		int srclen = strlen (buf);
		wchar_t buffer[CONSOLE_MAX_X+1];
		wchar_t *ptr = buffer;

		while(len)
		{
			int inc = 0;
			wchar_t t;
			int width;

			if (buf[0])
			{
				t = utf8_decode (buf, srclen, &inc);
				width = wcwidth (t);
			} else {
				t = ' ';
				width = 1;
			}

			if (width > len) /* if we can not fit double char at the end, we remove it */
			{
				t = ' ';
				width = 1;
			}
			if (width > 0)
			{
				*(ptr++) = t;
				len -= width;
			}
			buf += inc;
			srclen -= inc;
		}
		attrset (attr_table[plpalette[attr]]);
		*ptr = 0;
		mvaddwstr(y, x, buffer);
	} else
#endif
	{
		size_t srclen = strlen (buf);

		move(y, x);
		while(len)
		{
			chtype output;

			if (srclen)
			{
				if (utf8_to_native != (iconv_t)(-1))
				{
					char ch;
					char *dst = &ch;
					size_t dstlen=1;
					if (iconv (utf8_to_native, (char **)&buf, &srclen, &dst, &dstlen)<(size_t)-1)
					{
						goto skipone;
					}
					if (dstlen!=0)
					{
						goto skipone;
					}
					output=(uint8_t)ch;
				} else {
					int inc = 0;
					int codepoint;
				skipone:
					codepoint = utf8_decode (buf, srclen, &inc);
					buf+=inc;
					srclen-=inc;
					if (codepoint > 255)
					{
						output='?';
					} else {
						output=chr_table_iso8859latin1[codepoint];
					}
				}
			} else {
				output = ' ';
			}
			output |= attr_table[plpalette[attr]];

			addch(output);

			len--;
		}
	}
}

static int ncurses_MeasureStr_utf8 (const char *buf, int buflen)
{
	int retval = 0;
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		while (buflen > 0)
		{
			int inc = 0;
			wchar_t t;
			int width;

			t = utf8_decode (buf, buflen, &inc);
			width = wcwidth (t);

			if (width > 0)
			{
				retval += width;
			}
			buf += inc;
			buflen -= inc;
		}
	} else
#endif
	{
		int inc = 0;

		utf8_decode (buf, buflen, &inc);
		buf += inc;
		buflen -= inc;
		retval += 1;
	}

	return retval;
}

static void ncurses_DisplayStrAttr (uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		wchar_t buffer[CONSOLE_MAX_X+1];
		wchar_t *ptr = buffer;
		unsigned char lastattr = ((*buf)>>8);
		move (y, x);

		while(len)
		{
			unsigned char ch=(*buf)&0xff;
			unsigned char attr=((*buf)>>8);

			if (lastattr != attr)
			{
				attrset (attr_table[plpalette[lastattr]]);
				*ptr = 0;
				addwstr (buffer);
				ptr = buffer;
				lastattr = attr;
			}

			*(ptr++) = chr_table[ch];

			buf++;
			len--;
		}
		attrset (attr_table[plpalette[lastattr]]);
		*ptr=0;
		addwstr(buffer);
	} else
#endif
	{
		int first=1; /* we need this since we sometimes place the cursor at empty spots... dirty */

		move(y, x);
		while(len)
		{
			unsigned char ch=(*buf)&0xff;
			unsigned char attr=((*buf)>>8);
			chtype output;

			if (((!ch)||(ch==' '))&&(!(attr&0x80))&&fixbadgraphic)
			{
				if (first)
				{
					output=chr_table[ch]|attr_table[plpalette[attr]];
					first=0;
				} else {
					output=chr_table['X']|attr_table[plpalette[(attr&0xf0)+((attr>>4)&0xf)]];
				}
				addch(output);
			} else {
				first=1;
				output=chr_table[ch]|attr_table[plpalette[attr]];
				addch(output);
			}
			buf++;
			len--;
		}
	}
}

static void ncurses_DisplayVoid (uint16_t y, uint16_t x, uint16_t len)
{
	ncurses_DisplayChr (y, x, 0x07, ' ', len);
}

#ifdef HAVE_NCURSESW
static wchar_t bartops_unicode[17] = {
' ',
L'\u2581', L'\u2581',
L'\u2582', L'\u2582',
L'\u2583', L'\u2583',
L'\u2584', L'\u2584',
L'\u2585', L'\u2585',
L'\u2586', L'\u2586',
L'\u2587', L'\u2587',
L'\u2588', L'\u2588'};
#endif

static char bartops[18]="  ___...---===**#";
static char ibartops[18]="  ```^^^~~~===**#";

static void ncurses_iDrawBar (uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
{
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
#if 1
		wchar_t buffer[2] = {0, 0};
		unsigned int i;
		uint16_t yh1=(height+2)/3;
		uint16_t yh2=(height+yh1+1)/2;

		uint8_t _c;

		y-=height-1;

		if (value>((unsigned int)(height*16)-4))
		{
			value=(height*16)-4;
		}

		/* Background can not do bright colors across platforms in a stable way - hence 0x70 */
		_c = ((c << 4) & 0x70) | (((c >> 4) & 0x0f));
		attrset (attr_table[plpalette[_c]]);

		for (i=0; i < yh1; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			buffer[0] = bartops_unicode[16 - v];
			mvaddwstr (y++, x, buffer);
		}

		c>>=8;
		_c = ((c << 4) & 0x70) | (((c >> 4) & 0x0f));
		attrset (attr_table[plpalette[_c]]);

		for (i=yh1; i < yh2; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			buffer[0] = bartops_unicode[16 - v];
			value-=(value>16)?16:value;
			mvaddwstr (y++, x, buffer);
		}

		c>>=8;
		_c = ((c << 4) & 0x70) | (((c >> 4) & 0x0f));
		attrset (attr_table[plpalette[_c]]);

		for (i=yh2; i < height; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			buffer[0] = bartops_unicode[16 - v];
			value-=(value>16)?16:value;
			mvaddwstr (y++, x, buffer);
		}
#endif
	} else
#endif
	{
		unsigned int i;
		uint16_t yh1=(height+2)/3;
		uint16_t yh2=(height+yh1+1)/2;

		y-=height-1;

		if (value>((unsigned int)(height*16)-4))
		{
			value=(height*16)-4;
		}

		for (i=0; i<yh1; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			ncurses_DisplayStr(y, x, c&0xff, ibartops + v, 1);
			y++;
		}
		c>>=8;
		for (i=yh1; i<yh2; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			ncurses_DisplayStr(y, x, c&0xff, ibartops + v, 1);
			y++;
		}
		c>>=8;
		for (i=yh2; i<height; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			ncurses_DisplayStr(y, x, c&0xff, ibartops + v, 1);
			y++;
		}
	}
}

static void ncurses_DrawBar (uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
{
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		wchar_t buffer[2] = {0, 0};
		unsigned int i;
		uint16_t yh1=(height+2)/3;
		uint16_t yh2=(height+yh1+1)/2;

		if (value>((unsigned int)(height*16)-4))
		{
			value=(height*16)-4;
		}

		attrset (attr_table[plpalette[c & 0xff]]);
		c>>=8;

		for (i=0; i < yh1; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			buffer[0] = bartops_unicode[v];
			mvaddwstr (y--, x, buffer);
		}

		attrset (attr_table[plpalette[c & 0xff]]);
		c>>=8;

		for (i=yh1; i < yh2; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			buffer[0] = bartops_unicode[v];
			mvaddwstr (y--, x, buffer);
		}

		attrset (attr_table[plpalette[c & 0xff]]);
		c>>=8;

		for (i=yh2; i < height; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			buffer[0] = bartops_unicode[v];
			mvaddwstr (y--, x, buffer);
		}
	} else
#endif
	{
		unsigned int i;
		uint16_t yh1=(height+2)/3;
		uint16_t yh2=(height+yh1+1)/2;

		if (value>((unsigned int)(height*16)-4))
		{
			value=(height*16)-4;
		}

		for (i=0; i<yh1; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			ncurses_DisplayStr(y, x, c&0xff, bartops + v, 1);
			y--;
		}
		c>>=8;
		for (i=yh1; i<yh2; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			ncurses_DisplayStr(y, x, c&0xff, bartops + v, 1);
			y--;
		}
		c>>=8;
		for (i=yh2; i<height; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			ncurses_DisplayStr(y, x, c&0xff, bartops + v, 1);
			y--;
		}
	}
}

static int ncurses_ekbhit (void);
static int ncurses_egetch (void);
#ifdef NCURSES_VERSION
static int ncurses_HasKey (uint16_t key)
{
	switch (key)
	{
		case ' ': return 1;
		case 'a': return 1;
		case 'b': return 1;
		case 'c': return 1;
		case 'd': return 1;
		case 'e': return 1;
		case 'f': return 1;
		case 'g': return 1;
		case 'h': return 1;
		case 'i': return 1;
		case 'j': return 1;
		case 'k': return 1;
		case 'l': return 1;
		case 'm': return 1;
		case 'n': return 1;
		case 'o': return 1;
		case 'p': return 1;
		case 'q': return 1;
		case 'r': return 1;
		case 's': return 1;
		case 't': return 1;
		case 'u': return 1;
		case 'v': return 1;
		case 'w': return 1;
		case 'x': return 1;
		case 'y': return 1;
		case 'z': return 1;
		case 'A': return 1;
		case 'B': return 1;
		case 'C': return 1;
		case 'D': return 1;
		case 'E': return 1;
		case 'F': return 1;
		case 'G': return 1;
		case 'H': return 1;
		case 'I': return 1;
		case 'J': return 1;
		case 'K': return 1;
		case 'L': return 1;
		case 'M': return 1;
		case 'N': return 1;
		case 'O': return 1;
		case 'P': return 1;
		case 'Q': return 1;
		case 'R': return 1;
		case 'S': return 1;
		case 'T': return 1;
		case 'U': return 1;
		case 'V': return 1;
		case 'W': return 1;
		case 'X': return 1;
		case 'Y': return 1;
		case 'Z': return 1;
		case '1': return 1;
		case '2': return 1;
		case '3': return 1;
		case '4': return 1;
		case '5': return 1;
		case '6': return 1;
		case '7': return 1;
		case '8': return 1;
		case '9': return 1;
		case '0': return 1;
		case '/': return 1;
		case '*': return 1;
		case '-': return 1;
		case '+': return 1;
		case '\\': return 1;
		case '\'': return 1;
		case ',': return 1;
		case '.': return 1;
		case '?': return 1;
		case '!': return 1;
		case '>': return 1;
		case '<': return 1;
		case '|': return 1;
		case KEY_TAB: return 1;
		case KEY_ESC: return 1;
		case KEY_ALT_A: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_B: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_C: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_E: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_G: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_I: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_K: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_L: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_M: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_O: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_P: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_R: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_S: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_X: return 1; /* not documented in ncurses.h.... */
		case KEY_ALT_Z: return 1; /* not documented in ncurses.h.... */

		case KEY_CTRL_H: return 1;
		case KEY_CTRL_P: return 1;
		case KEY_CTRL_D: return 1;
		case KEY_CTRL_J: return 1;
		case KEY_CTRL_K: return 1;
		case KEY_CTRL_L: return 1;
		case KEY_CTRL_Q: return 1; /* can be XON */
		case KEY_CTRL_S: return 1; /* can be XOFF */
		case KEY_CTRL_Z: return 1;
		case KEY_BACKSPACE: return 1;
		case _KEY_ENTER: return 1;
		case KEY_ALT_ENTER: return 1; /* curses does not fetch this, but our filter does */
	}

	return !!keybound (key, 0);
}
#endif

static void ncurses_SetTextMode (uint8_t x)
{
	unsigned int i;

	// If we make a stackable curses driver, we will need this
	//Console.Driver->SetGraphMode (-1);

	___setup_key (ncurses_ekbhit, ncurses_egetch);

	Console.TextHeight = Height;
	Console.TextWidth = Width;
	Console.CurrentMode = 0;

	for (i = 0; i < Console.TextHeight; i++)
	{
		ncurses_DisplayVoid (i, 0, Console.TextWidth);
	}
}

#if (defined(NCURSES_VERSION_MAJOR)&&((NCURSES_VERSION_MAJOR<5)||((NCURSES_VERSION_MAJOR==5)&&(NCURSES_VERSION_MINOR<9))||((NCURSES_VERSION_MAJOR==5)&&(NCURSES_VERSION_MINOR==9)&&(NCURSES_VERSION_PATCH<20150103))))

#warning NCURSES_VERSION <= 5.9-20150103 has broken write() when using TIMERS, disabling SIGALRM curing ncurses library calls

static int block_level = 0;
static sigset_t block_mask;

static void ncurses_block_signals (void)
{
	if (!block_level)
	{
		sigset_t copy_mask;
		sigprocmask (SIG_SETMASK, 0, &block_mask);
		copy_mask = block_mask;
		sigaddset (&copy_mask, SIGALRM);
		sigprocmask (SIG_SETMASK, &copy_mask, 0);
	}
	block_level ++;
}

static void ncurses_unblock_signals (void)
{
	block_level --;
	if (!block_level)
	{
		sigprocmask (SIG_SETMASK, &block_mask, 0);
	}
}

#else
static void ncurses_block_signals (void)
{
}

static void ncurses_unblock_signals (void)
{
}

#endif

#ifdef CAN_RESIZE
int resized=0;
static void adjust (int sig)
{
	resized=1;
	signal(SIGWINCH, adjust);    /* some systems need this */
}
static void do_resize (void)
{
	struct winsize size;

	ncurses_block_signals();

	if (ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0)
	{
		resize_term(size.ws_row, size.ws_col);
		wrefresh(curscr);

		Height = Console.TextHeight = size.ws_row;
		if ((Width = Console.TextWidth = size.ws_col) > CONSOLE_MAX_X)
		{
			Width = Console.TextWidth = CONSOLE_MAX_X;
		} else if (Console.TextWidth < 80)
		{
			Width = Console.TextWidth = 80; /* If a console gets smaller than THIS, we are doomed */
		}
		___push_key(VIRT_KEY_RESIZE);
	}
	resized=0;

	ncurses_unblock_signals();
}
#endif /* CAN_RESIZE */

static void ncurses_RefreshScreen (void)
{
	ncurses_block_signals ();

#ifdef CAN_RESIZE
	if (resized)
		do_resize();
#endif
	refresh();

	ncurses_unblock_signals ();
}

static int buffer = ERR;
static int sigintcounter = 0;

static int ncurses_ekbhit (void)
{
	if (sigintcounter)
	{
		return 1;
	}

	if (buffer!=ERR)
	{
		return 1;
	}

	ncurses_block_signals ();

	buffer=getch();
	if (buffer!=ERR)
	{
		ncurses_unblock_signals ();
		return 1;
	}
	ncurses_RefreshScreen();

	ncurses_unblock_signals ();
	return 0;
}

static int ncurses_egetch (void)
{
	int retval;

	if (sigintcounter)
	{
		sigintcounter--;
		return 27;
	}

	ncurses_block_signals ();

	ncurses_RefreshScreen();
	if (buffer!=ERR)
	{
		retval=buffer;
		buffer=ERR;

		ncurses_unblock_signals ();

		return retval;
	}
	retval=getch();

	ncurses_unblock_signals ();

	if (retval==ERR)
		retval=0;

	return retval;
}

static void ncurses_SetCursorPosition (uint16_t y, uint16_t x)
{
	move(y, x);
}

static void ncurses_SetCursorShape (uint16_t shape)
{
	curs_set(shape);
}

static int conactive=0;

static int ncurses_consoleRestore (void)
{
	if (!conactive)
		return 0;
	endwin();
	conactive=0;
	return 0;
}

static void ncurses_consoleSave (void)
{
	if (conactive)
		return;
	fflush(stderr);
	clear();
	refresh();
	cbreak();
	nodelay(stdscr, TRUE);
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	idlok(stdscr, FALSE);
	start_color();
	conactive=1;
}

static void ncurses_plDosShell (void)
{
	pid_t child;

	printf ("Spawning a new shell - Exit shell to return back to Open Cubic Player\n");

	if (!(child=fork()))
	{
		char *shell=getenv("SHELL");
		if (!shell)
			shell="/bin/sh";
		if (!isatty(2))
		{
			close(2);
			if (dup(1)!=2)
				fprintf(stderr, __FILE__ ": dup(1) != 2\n");
		}
		execl(shell, shell, NULL);
		perror("execl()");
		exit(-1);
	} else if (child>0)
	{
		while(1)
		{
			int status, retval;
			if ((retval=waitpid(child, &status, WNOHANG))<=0)
			{
				if (errno==EINTR)
					continue;
				usleep (20000); /* 20ms, 50 FPS */
				tmTimerHandler ();
			} else {
				break;
			}
		}
	}
}

static void ncurses_DisplaySetupTextMode (void)
{
	/* dummy */
}

static const char *ncurses_GetDisplayTextModeName (void)
{
	static char mode[16];
	snprintf(mode, sizeof(mode), "%d x %d", Width, Height);
	return mode;
}

static void ncurses_sigint (int signal)
{
	sigintcounter++;
	if (sigintcounter > 2) /* program is frozen and we already have two presses in the queue */
	{
		kill (getpid(), SIGQUIT);
	}
}

int curses_init (void)
{
	int i;
	char *temp;
	iconv_t cd_cp437 = (iconv_t)-1;
	iconv_t cd_latin1 = (iconv_t)-1;

	fprintf(stderr, "Initing curses... (%s)\n",  curses_version());
	if ((fixbadgraphic=cfGetProfileBool("curses", "fixbadgraphic", 0, 0)))
		fprintf(stderr, "curses: fixbadgraphic is enabled in config\n");

	setlocale(LC_CTYPE, "");

	temp = nl_langinfo (CODESET);
#ifdef HAVE_NCURSESW
	if (temp && strstr (temp, "UTF-8"))
	{
		useunicode = 1;
		fprintf (stderr, "curses: will use UTF-8 characters instead of ASCII + ACS\n");

		for (i=0; i < 256; i++)
		{
			chr_table[i] = cp437_to_unicode[i];
		}

		for (i=0; i < 256; i++)
		{
			if ((i<=32) || ((i>=0x7f) && (i<=0xa0)))
			{
				chr_table_iso8859latin1[i] = ' ';
			} else {
				chr_table_iso8859latin1[i] = i;
			}
		}
	}
	else
#endif
	{
		char temp2[64];

		if (temp && strstr (temp, "UTF-8"))
		{
			fprintf(stderr, "curses: console is UTF-8, but OCP is not compiled with ncursesw - falling back to ASCII\n");
			setlocale(LC_CTYPE, "C.ISO-8859-1");
			temp="ASCII";
		}
		if (!temp)
		{
			temp = "ASCII";
		}
		if (!strstr(temp, "//TRANSLIT"))
		{
			snprintf (temp2, sizeof (temp2), "%s//TRANSLIT", temp);

			cd_cp437 = iconv_open(temp2, OCP_FONT);
			if (cd_cp437 == (iconv_t)(-1))
			{
				fprintf (stderr, "curses: Failed to make iconv matrix for %s->%s, retry with %s\n", OCP_FONT, temp2, temp);
				goto no_translit;
			} else {
				fprintf (stderr, "curses: Converting between %s -> %s\n", OCP_FONT, temp2);
#ifdef HAVE_NCURSESW
				if (!useunicode)
#else
// when not using NCURSESW, we never use unicode
#endif
				{
					utf8_to_native = iconv_open (temp2, "UTF-8");
					fprintf (stderr, "curses: Converting between UTF-8 -> %s\n", temp2);
				}

				cd_latin1 = iconv_open(temp2, "ISO-8859-1");
				if (cd_latin1 == (iconv_t)(-1))
				{
					fprintf (stderr, "curses: Failed to make iconv matrix for ISO-8859-1 %s\n", temp2);
				} else {
					fprintf (stderr, "curses: Converting between ISO-8859-1 -> %s\n", temp2);
				}
			}
		} else {
no_translit:
			cd_cp437 = iconv_open(temp, OCP_FONT);
			if (cd_cp437 == (iconv_t)(-1))
			{
				fprintf (stderr, "curses: Failed to make iconv matrix for %s->%s\n", OCP_FONT, temp);
			} else {
#ifdef HAVE_NCURSESW
				if (!useunicode)
#else
// when not using NCURSESW, we never use unicode
#endif
				{
					utf8_to_native = iconv_open (temp, "UTF-8");
					fprintf (stderr, "curses: Converting between UTF-8 -> %s\n", temp);
				}
				fprintf (stderr, "curses: Converting between %s -> %s\n", OCP_FONT, temp);
			}

			cd_latin1 = iconv_open(temp, "ISO-8859-1");
			if (cd_latin1 == (iconv_t)(-1))
			{
				fprintf (stderr, "curses: Failed to make iconv matrix for ISO-8859-1 %s\n", temp);
			} else {
				fprintf (stderr, "curses: Converting between ISO-8859-1 -> %s\n", temp);
			}
		}

		for (i=0; i < 256; i++)
		{
			if ((i >= 32) && (i < 127))
			{
				chr_table[i]=i;
			} else {
				int t = 0;
				switch (i)
				{
					case 0xff:
					case 0x00: t = ' '; break;
					case 0x04: t = ACS_DIAMOND; break; /* looks good on the header line */
					case 0x1a:
					case 0x10: t = ACS_RARROW; break;
					case 0x1b:
					case 0x11: t = ACS_LARROW; break;
					case 0x12: t = ACS_PLMINUS; break; /* we want an up+down arrow */
					case 0x18: t = ACS_UARROW;  break;
					case 0x19: t = ACS_DARROW;  break;
					case 0x1d: t = ACS_PLUS;    break; /* speed pitch lock */
					case 0xf9: t = ACS_BULLET;  break; /*|A_BOLD*/;
					case 0xfa: t = ACS_BULLET;  break;
				}
				if (t)
				{
					chr_table[i] = t;
				} else {
					switch (i) /* worst case backups */
					{
						case 0x01:
							chr_table[i] = 'S'; break;

						case 0x0d: case 0x0e:
							chr_table[i] = 'n'; break;

						case 0x11: case 0x1b: case 0xae:
							chr_table[i] = '<'; break;

						case 0x10: case 0x1a: case 0xaf:
							chr_table[i] = '>'; break;

						case 0xb3:
						case 0xba:
							chr_table[i] = '|'; break;

						case 0xb4: case 0xb5: case 0xb6: case 0xb7:
						case 0xb8: case 0xb9:            case 0xbb:
						case 0xbc: case 0xbd: case 0xbe: case 0xbf:
						case 0xc0: case 0xc1: case 0xc2: case 0xc3:
						           case 0xc5: case 0xc6: case 0xc7:
						case 0xc8: case 0xc9: case 0xca: case 0xcb:
						case 0xcc:            case 0xce: case 0xcf:
						case 0xd0: case 0xd1: case 0xd2: case 0xd3:
						case 0xd4: case 0xd5: case 0xd6: case 0xd7:
						case 0xd8: case 0xd9: case 0xda:
							chr_table[i] = '+'; break;

						case 0x29: case 0xc4: case 0xcd:
							chr_table[i] = '-'; break;

						case 0xdb:
						case 0xdc: case 0xdd: case 0xde: case 0xdf:
						case 0xfe:
							chr_table[i] = '#'; break;

						case 0xf0:
						case 0xf7:
							chr_table[i] = '='; break;

						case 0x82: case 0x88: case 0x89: case 0x8a:
							chr_table[i] = 'e'; break;

						case 0x83: case 0x84: case 0x85: case 0x86: case 0xa0: case 0xa6: case 0xe0: case 0x91:
							chr_table[i] = 'a'; break;

						case 0x8b: case 0x8c: case 0x8d: case 0xa1:
							chr_table[i] = 'i'; break;

						case 0x8e: case 0x8f: case 0x92:
							chr_table[i] = 'A'; break;

						case 0x93: case 0x94: case 0x95: case 0xa2: case 0xa7:
							chr_table[i] = 'o'; break;

						case 0x96: case 0x97: case 0xa3:
							chr_table[i] = 'u'; break;

						case 0x87: case 0x9b:
							chr_table[i] = 'c'; break;

						case 0x04: chr_table[i] = '*'; break;
						case 0x13: chr_table[i] = '!'; break;
						case 0x18: chr_table[i] = '^'; break;
						case 0x19: chr_table[i] = '\\'; break;
						case 0x80: chr_table[i] = 'C'; break;
						case 0x81: chr_table[i] = 'u'; break;
						case 0x90: chr_table[i] = 'E'; break;
						case 0x98: chr_table[i] = 'y'; break;
						case 0x99: chr_table[i] = 'O'; break;
						case 0x9a: chr_table[i] = 'U'; break;
						case 0x9c: chr_table[i] = 'L'; break;
						case 0x9d: chr_table[i] = 'Y'; break;
						case 0x9f: chr_table[i] = 'f'; break;
						case 0xa4: chr_table[i] = 'n'; break;
						case 0xa5: chr_table[i] = 'N'; break;
						case 0xa8: chr_table[i] = '?'; break;
						case 0xad: chr_table[i] = '!'; break;
						case 0xe1: chr_table[i] = 'b'; break;
						case 0xe4: chr_table[i] = 'M'; break;
						case 0xe5: chr_table[i] = 'o'; break;
						case 0xe6: chr_table[i] = 'u'; break;
						case 0xe7: chr_table[i] = 't'; break;

						default:
							chr_table[i] = '_'; break;
					}

					if (cd_cp437 != (iconv_t)(-1))
					{
						char src[1];
						unsigned char dst[16];
						char *to=(char *)dst, *from=src;
						size_t _to=16, _from=1;
						src[0]=(char)i;
						if (iconv(cd_cp437, &from, &_from, &to, &_to) != (size_t)-1)
						{
							if ((dst[0] < 0x20) && strstr(temp, "8859"))
							{ /* GNU iconv seems to very crude.. ISO-8859-X should not have have control codes */
								continue;
							}
							if ((_to==15)&&(_from==0)&&(dst[0]) &&
							    (dst[0]!=0x04) && /* End Of Medium */
							    (dst[0]!=0x07) && /* Bell */
							    (dst[0]!=0x08) && /* Backspace */
							    (dst[0]!=0x09) && /* Tab */
							    (dst[0]!=0x0a) && /* New Line */
							    (dst[0]!=0x0b) && /* Form Feed / Clear screen */
							    (dst[0]!=0x0c) && /* Vertical Tab */
							    (dst[0]!=0x0d) && /* Line Feed */
							    (dst[0]!=0x10) && /* Data Link Escape */
							    (dst[0]!=0x11) && /* Device Control One */
							    (dst[0]!=0x12) && /* Device Control Two */
							    (dst[0]!=0x13) && /* Device Control Three */
							    (dst[0]!=0x14) && /* Device Control Four */
						            (dst[0]!=0x19) && /* End Of Medium */
							    (dst[0]!=0x1c))   /* Escape - starts escape sequence */
							{
								chr_table[i]=dst[0];
							}
						}
					}
				}
			}
		}

		for (i=0; i < 256; i++)
		{
			if ((i<=32) || ((i>=0x7f) && (i<=0xa0)))
			{
				chr_table_iso8859latin1[i] = ' ';
			} else if (i < 127)
			{
				chr_table_iso8859latin1[i] = i;
			} else {
				switch (i) /* worst case backups */
				{
					case 0xa1: chr_table_iso8859latin1[i] = '!'; break;
					case 0xa2: case 0xe7:
					           chr_table_iso8859latin1[i] = 'c'; break;
					case 0xa3: chr_table_iso8859latin1[i] = 'L'; break;
					case 0xa5: case 0xdd:
					           chr_table_iso8859latin1[i] = 'Y'; break;
					case 0xa6: chr_table_iso8859latin1[i] = '|'; break;
					case 0xa7: chr_table_iso8859latin1[i] = 'S'; break;
					case 0xa9: case 0xc7:
					           chr_table_iso8859latin1[i] = 'C'; break;
					case 0xaa: case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: case 0xe6:
					           chr_table_iso8859latin1[i] = 'a'; break;
					case 0xab: chr_table_iso8859latin1[i] = '<'; break;
					case 0xae: chr_table_iso8859latin1[i] = 'R'; break;
					case 0xb0: chr_table_iso8859latin1[i] = '0'; break;
					case 0xb2: chr_table_iso8859latin1[i] = '2'; break;
					case 0xb3: chr_table_iso8859latin1[i] = '3'; break;
					case 0xb5: case 0xf9: case 0xfa: case 0xfb: case 0xfc:
					           chr_table_iso8859latin1[i] = 'u'; break;
					case 0xb6: chr_table_iso8859latin1[i] = 'P'; break;
					case 0xb7: chr_table_iso8859latin1[i] = '.'; break;
					case 0xb8: chr_table_iso8859latin1[i] = ','; break;
					case 0xb9: chr_table_iso8859latin1[i] = '1'; break;
					case 0xba: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf8:
					           chr_table_iso8859latin1[i] = 'o'; break;
					case 0xbb: chr_table_iso8859latin1[i] = '>'; break;
					case 0xbf: chr_table_iso8859latin1[i] = '?'; break;
					case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: case 0xc6:
					           chr_table_iso8859latin1[i] = 'A'; break;
					case 0xc8: case 0xc9: case 0xca: case 0xcb:
					           chr_table_iso8859latin1[i] = 'E'; break;
					case 0xcc: case 0xcd: case 0xce: case 0xcf:
					           chr_table_iso8859latin1[i] = 'I'; break;
					case 0xd0: chr_table_iso8859latin1[i] = 'D'; break;
					case 0xd1: chr_table_iso8859latin1[i] = 'N'; break;
					case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd8:
					           chr_table_iso8859latin1[i] = 'O'; break;
					case 0xd7: chr_table_iso8859latin1[i] = 'x'; break;
					case 0xd9: case 0xda: case 0xdb: case 0xdc:
					           chr_table_iso8859latin1[i] = 'U'; break;
					case 0xde: chr_table_iso8859latin1[i] = 'p'; break;
					case 0xdf: chr_table_iso8859latin1[i] = 'B'; break;
					           chr_table_iso8859latin1[i] = 'a'; break;
					case 0xe8: case 0xe9: case 0xea: case 0xeb:
					           chr_table_iso8859latin1[i] = 'e'; break;
					case 0xec: case 0xed: case 0xee: case 0xef:
					           chr_table_iso8859latin1[i] = 'i'; break;
					case 0xf0: chr_table_iso8859latin1[i] = 'd'; break;
					case 0xf1: chr_table_iso8859latin1[i] = 'n'; break;
					case 0xfd: case 0xff:
					           chr_table_iso8859latin1[i] = 'y'; break;
					case 0xfe: chr_table_iso8859latin1[i] = 'P'; break;
					default:   chr_table_iso8859latin1[i] = '_'; break;
				}
				if (cd_latin1 != (iconv_t)(-1))
				{
					char src[1];
					char dst[16];
					char *to=dst, *from=src;
					size_t _to=16, _from=1;
					src[0]=(char)i;
					if (iconv(cd_latin1, &from, &_from, &to, &_to) != (size_t)-1)
					{
						if ((_to==15)&&(_from==0)&&(dst[0]) &&
						    (dst[0]!=0x04) && /* End Of Medium */
						    (dst[0]!=0x07) && /* Bell */
						    (dst[0]!=0x08) && /* Backspace */
						    (dst[0]!=0x09) && /* Tab */
						    (dst[0]!=0x0a) && /* New Line */
						    (dst[0]!=0x0b) && /* Form Feed / Clear screen */
						    (dst[0]!=0x0c) && /* Vertical Tab */
						    (dst[0]!=0x0d) && /* Line Feed */
						    (dst[0]!=0x10) && /* Data Link Escape */
						    (dst[0]!=0x11) && /* Device Control One */
						    (dst[0]!=0x12) && /* Device Control Two */
						    (dst[0]!=0x13) && /* Device Control Three */
						    (dst[0]!=0x14) && /* Device Control Four */
					            (dst[0]!=0x19) && /* End Of Medium */
						    (dst[0]!=0x1c))   /* Escape - starts escape sequence */
						{
							chr_table_iso8859latin1[i]=(unsigned char)dst[0];
						}
					}
				}
			}
		}

	}

	if (cd_cp437 != (iconv_t)-1)
	{
		iconv_close(cd_cp437);
	}

	if (cd_latin1 != (iconv_t)-1)
	{
		iconv_close(cd_latin1);
	}


#if 0
	fprintf (stderr, "          0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f\n");
	fprintf (stderr, "     +--------------------------------------------------------------------------------\n");
	for (i=0; i < 256; i+=16)
	{
		fprintf (stderr, "0x%xx | %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x\n", i>>4,
			chr_table[i +  0], chr_table[i +  1], chr_table[i +  2], chr_table[i +  3],
			chr_table[i +  4], chr_table[i +  5], chr_table[i +  6], chr_table[i +  7],
			chr_table[i +  8], chr_table[i +  9], chr_table[i + 10], chr_table[i + 11],
			chr_table[i + 12], chr_table[i + 13], chr_table[i + 14], chr_table[i + 15]);
	}
#endif

	if (!initscr())
	{
		fprintf(stderr, "curses failed to init\n");
		return -1;
	}

	if (!getenv("ESCDELAY"))
	{
		set_escdelay (25);
	}

	ncurses_consoleSave ();
#if CAN_RESIZE
	signal(SIGWINCH, adjust);        /* arrange interrupts to resize */
#endif
	signal(SIGINT, ncurses_sigint); /* emulate ESC key on ctrl-c, but multiple presses + OCP deadlocked will make it quit by force */

	Console.Driver = &ncursesConsoleDriver;
	___setup_key (ncurses_ekbhit, ncurses_egetch); /* filters in more keys */

	start_color();

	attron(A_NORMAL);

	/* the attr_table is insane... on a regular i386 ncurses, you have 64 pairs,
	 * and entry 0 is locked to grey text on black, and colors apear in wrong
	 * order compared to DOS etc, so this is just butt fucking ugly, but it seems
	 * to do the work. The code in displayattr etc looks nice then, and fast
	 * enough
	 */
	for (i=1;i<COLOR_PAIRS;i++)
	{
		unsigned char color_table[8] = {COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE};
		unsigned char ch, fg, bg;

		ch=i^0x07;
		fg=color_table[ch&0x7];
		bg=color_table[(ch&0x38)>>3];
		init_pair(i, fg, bg);
	}

	for (i=0;i<256;i++)
	{
		attr_table[i]=COLOR_PAIR((((i&0x7)^0x07)+((i&0x70)>>1)));

		if (!i)
			attr_table[i] |= A_INVIS;
		if (i&0x08)
			attr_table[i] |= A_BOLD;
		if (i&0x80)
			attr_table[i] |= A_STANDOUT;
	}

	Console.VidType = vidNorm;
	Console.LastTextMode = 0;
	Console.CurrentMode = 0;
	ncurses_RefreshScreen();

	Height = Console.TextHeight = LINES;
	if ((Width = Console.TextWidth = COLS) > CONSOLE_MAX_X)
	{
		Width = Console.TextWidth = CONSOLE_MAX_X;
	} else if (Console.TextWidth < 80)
	{
		Width = Console.TextWidth = 80; /* If a console gets smaller than THIS, the user-experience will be non-normal */
	}

	ncurses_consoleRestore ();

	return 0;
}

void curses_done (void)
{
	if (utf8_to_native != (iconv_t)-1)
	{
		iconv_close (utf8_to_native);
		utf8_to_native = (iconv_t)-1;
	}
	ncurses_consoleRestore ();
}

static struct consoleDriver_t ncursesConsoleDriver =
{
	0,   /* vga13 */
	ncurses_SetTextMode,
	ncurses_DisplaySetupTextMode,
	ncurses_GetDisplayTextModeName,
	ncurses_MeasureStr_utf8,
	ncurses_DisplayStr_utf8,
	ncurses_DisplayChr,
	ncurses_DisplayStr,
	ncurses_DisplayStrAttr,
	ncurses_DisplayVoid,
	ncurses_DrawBar,
	ncurses_iDrawBar,
	0,   /* TextOverlayAddBGRA */
	0,   /* TextOverlayRemove */
	0,   /* SetGraphMode */
	0,   /* gDrawChar16 */
	0,   /* gDrawChar16P */
	0,   /* gDrawChar8 */
	0,   /* gDrawChar8P */
	0,   /* gDrawStr */
	0,   /* gUpdateStr */
	0,   /* gUpdatePal */
	0,   /* gFlushPal */
#ifdef NCURSES_VERSION
	ncurses_HasKey,
#else
	consoleHasKey,
#endif
	ncurses_SetCursorPosition,
	ncurses_SetCursorShape,
	ncurses_consoleRestore,
	ncurses_consoleSave,
	ncurses_plDosShell
};
