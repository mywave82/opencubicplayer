/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '11-'20 Stian Skjelstad <stian.skjelestad@gmail.com>
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
#include "boot/console.h"
#include "stuff/poutput.h"
#include "boot/psetting.h"

#if defined(SIGWINCH) && defined(TIOCGWINSZ) && HAVE_RESIZE_TERM
#define CAN_RESIZE 1
#else
#undef CAN_RESIZE
#endif

#if CAN_RESIZE
static void adjust(int sig);
#endif

static NCURSES_ATTR_T attr_table[256];
static chtype chr_table[256];
static chtype chr_table_iso8859latin1[256];

static int Width, Height;

static int fixbadgraphic;

#ifdef HAVE_NCURSESW
static int useunicode = 0;
#endif

static void displayvoid(unsigned short y, unsigned short x, unsigned short len)
{
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		wchar_t buffer[CONSOLE_MAX_X+1];
		wchar_t *ptr = buffer;

		while(len)
		{
			*(ptr++) = chr_table[' '];
			len--;
		}
		attrset (attr_table[plpalette[0]]);
		*ptr = 0;
		mvaddwstr(y, x, buffer);

	} else
#endif
	{
		move(y, x);
		while(len)
		{
			chtype output;
			output='X'|attr_table[plpalette[0]];
			addch(output);
			len--;
		}
	}

}

static void displaystr(unsigned short y, unsigned short x, unsigned char attr, const char *buf, unsigned short len)
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

static void displaystr_iso8859latin1(unsigned short y, unsigned short x, unsigned char attr, const char *buf, unsigned short len)
{
#ifdef HAVE_NCURSESW
	if (useunicode)
	{
		wchar_t buffer[CONSOLE_MAX_X+1];
		wchar_t *ptr = buffer;

		while(len)
		{
			unsigned char ch=(*buf)&0xff;
			*(ptr++) = chr_table_iso8859latin1[ch];

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
				output=chr_table_iso8859latin1['X']|attr_table[plpalette[(attr&0xf0)+((attr>>4)&0xf)]];
				addch(output);
			} else {
				output=chr_table_iso8859latin1[ch]|attr_table[plpalette[attr]];
				addch(output);
			}

			if (*buf)
				buf++;
			len--;
		}
	}
}

static void displaystrattr(unsigned short y, unsigned short x, const uint16_t *buf, unsigned short len)
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

static void displaystrattr_iso8859latin1(unsigned short y, unsigned short x, const uint16_t *buf, unsigned short len)
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

			*(ptr++) = chr_table_iso8859latin1[ch];

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
					output=chr_table_iso8859latin1[ch]|attr_table[plpalette[attr]];
					first=0;
				} else {
					output=chr_table_iso8859latin1['X']|attr_table[plpalette[(attr&0xf0)+((attr>>4)&0xf)]];
				}
				addch(output);
			} else {
				first=1;
				output=chr_table_iso8859latin1[ch]|attr_table[plpalette[attr]];
				addch(output);
			}
			buf++;
			len--;
		}
	}
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

static void drawbar(uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c);

static void idrawbar(uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
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
			displaystr(y, x, c&0xff, ibartops + v, 1);
			y++;
		}
		c>>=8;
		for (i=yh1; i<yh2; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			displaystr(y, x, c&0xff, ibartops + v, 1);
			y++;
		}
		c>>=8;
		for (i=yh2; i<height; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			displaystr(y, x, c&0xff, ibartops + v, 1);
			y++;
		}
	}
}

static void drawbar(uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
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
			displaystr(y, x, c&0xff, bartops + v, 1);
			y--;
		}
		c>>=8;
		for (i=yh1; i<yh2; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			displaystr(y, x, c&0xff, bartops + v, 1);
			y--;
		}
		c>>=8;
		for (i=yh2; i<height; i++)
		{
			uint32_t v = ( value >= 16 ) ? 16 : value;
			value -= v;
			displaystr(y, x, c&0xff, bartops + v, 1);
			y--;
		}
	}
}

static int ekbhit(void);
static int egetch(void);

static void plSetTextMode(unsigned char x)
{
	unsigned int i;

	_plSetGraphMode(-1);

	___setup_key(ekbhit, egetch);

	plScrHeight=Height;
	plScrWidth=Width;
	plScrMode=0;

	for (i=0;i<plScrHeight;i++)
		displayvoid(i, 0, plScrWidth);
}

#if (defined(NCURSES_VERSION_MAJOR)&&((NCURSES_VERSION_MAJOR<5)||((NCURSES_VERSION_MAJOR==5)&&(NCURSES_VERSION_MINOR<9))||((NCURSES_VERSION_MAJOR==5)&&(NCURSES_VERSION_MINOR==9)&&(NCURSES_VERSION_PATCH<20150103))))

#warning NCURSES_VERSION <= 5.9-20150103 has broken write() when using TIMERS, disabling SIGALRM curing ncurses library calls

static int block_level = 0;
static sigset_t block_mask;

static void curses_block_signals ()
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

static void curses_unblock_signals ()
{
	block_level --;
	if (!block_level)
	{
		sigprocmask (SIG_SETMASK, &block_mask, 0);
	}
}

#else
static void curses_block_signals ()
{
}

static void curses_unblock_signals ()
{
}

#endif

#ifdef CAN_RESIZE
int resized=0;
static void adjust(int sig)
{
	resized=1;
	signal(SIGWINCH, adjust);    /* some systems need this */
}
static void do_resize(void)
{
	struct winsize size;

	curses_block_signals();

	if (ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0)
	{
		resize_term(size.ws_row, size.ws_col);
		wrefresh(curscr);

		Height=plScrHeight=size.ws_row;
		if ((Width=plScrWidth=size.ws_col)>CONSOLE_MAX_X)
			Width=plScrWidth=CONSOLE_MAX_X;
		else if (plScrWidth<80)
			Width=plScrWidth=80; /* If a console gets smaller than THIS, the user deserves to get fucked up his or her ASS */

		___push_key(VIRT_KEY_RESIZE);
	}
	resized=0;

	curses_unblock_signals();
}
#endif /* CAN_RESIZE */

static void RefreshScreen(void)
{
	curses_block_signals ();

#ifdef CAN_RESIZE
	if (resized)
		do_resize();
#endif
	refresh();

	curses_unblock_signals ();
}

static int buffer=ERR;

static int ekbhit(void)
{
	if (buffer!=ERR)
		return 1;

	curses_block_signals ();

	buffer=getch();
	if (buffer!=ERR)
	{
		curses_unblock_signals ();
		return 1;
	}
	RefreshScreen();

	curses_unblock_signals ();
	return 0;
}

static int egetch(void)
{
	int retval;

	curses_block_signals ();

	RefreshScreen();
	if (buffer!=ERR)
	{
		retval=buffer;
		buffer=ERR;

		curses_unblock_signals ();

		return retval;
	}
	retval=getch();

	curses_unblock_signals ();

	if (retval==ERR)
		retval=0;

	return retval;
}

static void setcur(unsigned char y, unsigned char x)
{
	move(y, x);
}

static void setcurshape(unsigned short shape)
{
	curs_set(!!shape);
}

static int conactive=0;

static int conRestore(void)
{
	if (!conactive)
		return 0;
	endwin();
	conactive=0;
	return 0;
}

static void conSave(void)
{
	if (conactive)
		return;
	fflush(stderr);
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

static void plDosShell(void)
{
	pid_t child;
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
			if ((retval=waitpid(child, &status, 0))<0)
			{
				if (errno==EINTR)
					continue;
			}
			break;
		}
	}
}

static const char *plGetDisplayTextModeName(void)
{
	static char mode[16];
	snprintf(mode, sizeof(mode), "%d x %d", Width, Height);
	return mode;
}

int curses_init(void)
{
	int i;
	char *temp;

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
			switch (i)
			{
				default:   chr_table[i]=i;         break;
				case 0xff:
				case 0x00: chr_table[i]=' ';       break;
				case 0x01: chr_table[i]='S';       break; /* Surround */
				case 0x02: chr_table[i]=L'\u263b'; break; /* INVERTED SMILEY */
				case 0x03: chr_table[i]=L'\u2665'; break; /* HEART */
				case 0x04: chr_table[i]=L'\u2666'; break; /* DIAMOND */
				case 0x05: chr_table[i]=L'\u2663'; break; /* CLUBS */
				case 0x06: chr_table[i]=L'\u2660'; break; /* SPADES */
				case 0x07: chr_table[i]=L'\u2022'; break; /* and this?*/
				case 0x08: chr_table[i]=L'\u25d8'; break; /* ??? */
				case 0x09: chr_table[i]=L'\u25cb'; break; /* ??? */
				case 0x0a: chr_table[i]=L'\u25d9'; break; /* when the heck does this occure */
				case 0x0b: chr_table[i]=L'\u2642'; break; /* ??? */
				case 0x0c: chr_table[i]=L'\u2640'; break; /* ??? */
				case 0x0d: chr_table[i]=L'\u266a'; break; /* we want a note */
				case 0x0e: chr_table[i]=L'\u266b'; break;
				case 0x0f: chr_table[i]=L'\u263c'; break;
				case 0x10: chr_table[i]=L'\u25ba'; break; /* right arrow */
				case 0x11: chr_table[i]=L'\u25c4'; break; /* left arrow */
				case 0x12: chr_table[i]=L'\u2195'; break; /* up+down arrow */
				case 0x13: chr_table[i]=L'\u203c'; break; /* DOUBLE EXCLAMATION MARK */
				case 0x14: chr_table[i]=L'\u00b6'; break; /* PILCROW */
				case 0x15: chr_table[i]=L'\u00a7'; break; /* SECTION SIGN */
				case 0x16: chr_table[i]=L'\u25ac'; break; /* ??? */
				case 0x17: chr_table[i]=L'\u21a8'; break; /* ??? */
				case 0x18: chr_table[i]=L'\u2191'; break; /* up arrow */
				case 0x19: chr_table[i]=L'\u2193'; break; /* down arrow */
				case 0x1a: chr_table[i]=L'\u2192'; break; /* pitch? */
				case 0x1b: chr_table[i]=L'\u2190'; break; /* pitch? */
				case 0x1c: chr_table[i]=L'\u221f'; break; /* ???? */
				case 0x1d: chr_table[i]=L'\u2194'; break; /* left+right arrow */
				case 0x1e: chr_table[i]=L'\u25be'; break; /* ??? */
				case 0x1f: chr_table[i]=L'\u25bc'; break; /* ??? */
				case 0x7f: chr_table[i]=L'\u2302'; break; /* HOUSE */
				case 0x80: chr_table[i]=L'\u00c7'; break; /* Capital c-cedilla */
				case 0x81: chr_table[i]=L'\u00fc'; break; /* Small u-umlaut */
				case 0x82: chr_table[i]=L'\u00e9'; break; /* Small e-acute */
				case 0x83: chr_table[i]=L'\u00e2'; break; /* Small a-circumflex */
				case 0x84: chr_table[i]=L'\u00e4'; break; /* Small a-umlaut */
				case 0x85: chr_table[i]=L'\u00e0'; break; /* Small a-grave */
				case 0x86: chr_table[i]=L'\u00e4'; break; /* Small a-ring */
				case 0x87: chr_table[i]=L'\u00e7'; break; /* Small c-cedilla */
				case 0x88: chr_table[i]=L'\u00ea'; break; /* Small e-circumflex */
				case 0x89: chr_table[i]=L'\u00eb'; break; /* Small e-diaeresis */
				case 0x8a: chr_table[i]=L'\u00e8'; break; /* Small e-grave */
				case 0x8b: chr_table[i]=L'\u00ef'; break; /* Small i-unlaut */
				case 0x8c: chr_table[i]=L'\u00ee'; break; /* Small i-circumflex */
				case 0x8d: chr_table[i]=L'\u00ec'; break; /* Small i-grave */
				case 0x8e: chr_table[i]=L'\u00c4'; break; /* Capital a-umlaut */
				case 0x8f: chr_table[i]=L'\u00c5'; break; /* Capital a-ring */
				case 0x90: chr_table[i]=L'\u00c9'; break; /* Capital e-acute */
				case 0x91: chr_table[i]=L'\u00e6'; break; /* Small ae */
				case 0x92: chr_table[i]=L'\u00c6'; break; /* Capital ae */
				case 0x93: chr_table[i]=L'\u00f4'; break; /* Small o-circumflex */
				case 0x94: chr_table[i]=L'\u00f6'; break; /* Small o-umlaut */
				case 0x95: chr_table[i]=L'\u00f2'; break; /* Small o-grave */
				case 0x96: chr_table[i]=L'\u00fb'; break; /* Small u-circumflex */
				case 0x97: chr_table[i]=L'\u00f9'; break; /* Small u-grave */
				case 0x98: chr_table[i]=L'\u00ff'; break; /* Small y-umlaut */
				case 0x99: chr_table[i]=L'\u00d6'; break; /* Capital o-umlaut */
				case 0x9a: chr_table[i]=L'\u00dc'; break; /* Capital u-umlaut */
				case 0x9b: chr_table[i]=L'\u00a2'; break; /* Cent */
				case 0x9c: chr_table[i]=L'\u00a3'; break; /* Pound sign */
				case 0x9d: chr_table[i]=L'\u00a5'; break; /* Yen/Yuan sign */
				case 0x9e: chr_table[i]=L'\u20a7'; break; /* Peseta sign */
				case 0x9f: chr_table[i]=L'\u0192'; break; /* F with a hook */
				case 0xa0: chr_table[i]=L'\u00e1'; break; /* Small a-acute */
				case 0xa1: chr_table[i]=L'\u00ed'; break; /* Small i-acute */
				case 0xa2: chr_table[i]=L'\u00f3'; break; /* Small o-acute */
				case 0xa3: chr_table[i]=L'\u00fa'; break; /* Small u-acute */
				case 0xa4: chr_table[i]=L'\u00f1'; break; /* Small n-tilde */
				case 0xa5: chr_table[i]=L'\u00d1'; break; /* Capital n-tilde */
				case 0xa6: chr_table[i]=L'\u00aa'; break; /* A-ordinal */
				case 0xa7: chr_table[i]=L'\u00ba'; break; /* O-ordinal */
				case 0xa8: chr_table[i]=L'\u00bf'; break; /* Inverted question mark */
				case 0xa9: chr_table[i]=L'\u2310'; break; /* Logic negation */
				case 0xaa: chr_table[i]=L'\u00ad'; break; /* Logic complement */
				case 0xab: chr_table[i]=L'\u00bd'; break; /* One-Half */
				case 0xac: chr_table[i]=L'\u00bc'; break; /* One-Quarter */
				case 0xad: chr_table[i]=L'\u00a1'; break; /* Inverted exclamation mark */
				case 0xae: chr_table[i]=L'\u00ab'; break; /* Left angle quotes */
				case 0xaf: chr_table[i]=L'\u00bb'; break; /* Right angle quotes */
				case 0xb0: chr_table[i]=L'\u2591'; break; /* BLOCK ELEMENT LIGHT SHADE */
				case 0xb1: chr_table[i]=L'\u2592'; break; /* BLOCK ELEMENT MEDIUM SHADE */
				case 0xb2: chr_table[i]=L'\u2593'; break; /* BLOCK ELEMENT DARK SHADE */
				case 0xb3: chr_table[i]=L'\u2502'; break; /* BOX DRAWINGS LIGHT VERTICAL */
				case 0xb4: chr_table[i]=L'\u2524'; break; /* BOX DRAWINGS LIGHT VERTICAL AND LEFT */
				case 0xb5: chr_table[i]=L'\u2561'; break; /* ??? - BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE */
				case 0xb6: chr_table[i]=L'\u2562'; break; /* ??? - BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE */
				case 0xb7: chr_table[i]=L'\u2556'; break; /* ??? - BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE */
				case 0xb8: chr_table[i]=L'\u2555'; break; /* ??? - BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE */
				case 0xb9: chr_table[i]=L'\u2563'; break; /* ??? - BOX DRAWINGS DOUBLE VERTICAL AND DOUBLE LEFT */
				case 0xba: chr_table[i]=L'\u2551'; break; /* BOX DRAWINGS DOUBLE VERTICAL */
				case 0xbb: chr_table[i]=L'\u2557'; break; /* BOX DRAWINGS DOUBLE DOWN AND LEFT */
				case 0xbc: chr_table[i]=L'\u255d'; break; /* BOX DRAWINGS DOUBLE UP AND LEFT */
				case 0xbd: chr_table[i]=L'\u255c'; break; /* ??? - BOX DRAWING SINGLE LEFT AND UP DOUBLE */
				case 0xbe: chr_table[i]=L'\u255b'; break; /* ??? - BOX DRAWING DOUBLE LEFT AND UP SINGLE */
				case 0xbf: chr_table[i]=L'\u2510'; break; /* BOX DRAWINGS LIGHT DOWN AND LEFT */
				case 0xc0: chr_table[i]=L'\u2514'; break; /* BOX DRAWINGS LIGHT UP AND RIGHT */
				case 0xc1: chr_table[i]=L'\u2534'; break; /* BOX DRAWINGS LIGHT UP AND HORIZONTAL */
				case 0xc2: chr_table[i]=L'\u252c'; break; /* BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
				case 0xc3: chr_table[i]=L'\u251c'; break; /* BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
				case 0xc4: chr_table[i]=L'\u2500'; break; /* BOX DRAWINGS LIGHT HORIZONTAL */
				case 0xc5: chr_table[i]=L'\u253c'; break; /* BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
				case 0xc6: chr_table[i]=L'\u255e'; break; /* ??? - BOX DRAWINGS VERTICAL SINGLE AND DOUBLE RIGHT */
				case 0xc7: chr_table[i]=L'\u255f'; break; /* ??? - BOX DRAWINGS VERTICAL DOUBLE AND SINGLE RIGHT */
				case 0xc8: chr_table[i]=L'\u255a'; break; /* BOX DRAWINGS DOUBLE UP AND RIGHT */
				case 0xc9: chr_table[i]=L'\u2554'; break; /* BOX DRAWINGS DOUBLE DOWN AND RIGHT */
				case 0xca: chr_table[i]=L'\u2569'; break; /* ??? - BOX DRAWINGS DOUBLE HORIZONTAL AND UP */
				case 0xcb: chr_table[i]=L'\u2566'; break; /* ??? - BOX DRAWINGS DOUBLE HORIZONTAL AND DOWN */
				case 0xcc: chr_table[i]=L'\u2560'; break; /* BOX DRAWINGS DOUBLE VERTICAL AND RIGHT */
				case 0xcd: chr_table[i]=L'\u2550'; break; /* BOX DRAWINGS DOUBLE HORIZONTAL */
				case 0xce: chr_table[i]=L'\u256c'; break; /* ??? - BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL */
				case 0xcf: chr_table[i]=L'\u2567'; break; /* ??? - BOX DRAWINGS DOUBLE HORIZONTAL AND SINGLE UP */
				case 0xd0: chr_table[i]=L'\u2568'; break; /* ??? - BOX DRAWINGS SINGLE HORIZONTAL AND DOUBLE UP */
				case 0xd1: chr_table[i]=L'\u2564'; break; /* ??? - BOX DRAWINGS DOUBLE HORIZONTAL AND SINGLE DOWN */
				case 0xd2: chr_table[i]=L'\u2565'; break; /* ??? - BOX DRAWINGS SINGLE HORIZONTAL AND DOUBLE DOWN */
				case 0xd3: chr_table[i]=L'\u2559'; break; /* ??? - BOX DRAWINGS DOUBLE UP AND SINGLE RIGHT */
				case 0xd4: chr_table[i]=L'\u2558'; break; /* ??? - BOX DRAWINGS SINGLE UP AND DOUBLE RIGHT */
				case 0xd5: chr_table[i]=L'\u2552'; break; /* ??? - BOX DRAWINGS SINGLE DOWN AND DOUBLE RIGHT */
				case 0xd6: chr_table[i]=L'\u2553'; break; /* ??? - BOX DRAWINGS DOUBLE DOWN AND SINGLE RIGHT */
				case 0xd7: chr_table[i]=L'\u256b'; break; /* ??? - BOX DRAWINGS DOUBLE VERTICAL AND SINGLE HORIZONTAL */
				case 0xd8: chr_table[i]=L'\u256a'; break; /* ??? - BOX DRAWINGS SINGLE VERTICAL AND SOUBLE HORIZONTAL */
				case 0xd9: chr_table[i]=L'\u2518'; break; /* BOX DRAWINGS LIGHT UP AND LEFT */
				case 0xda: chr_table[i]=L'\u250c'; break; /* BOX DRAWINGS LIGHT DOWN AND RIGHT */
				case 0xdb: chr_table[i]=L'\u2588'; break; /* ??? - BOX DRAWINGS SINGLE VERTICAL AND SOUBLE HORIZONTAL */
				case 0xdc: chr_table[i]=L'\u2584'; break; /* ??? - BOX DRAWINGS SINGLE VERTICAL AND SOUBLE HORIZONTAL */
				case 0xdd: chr_table[i]=L'\u258c'; break; /* ??? - LEFT HALF BLOCK */
				case 0xde: chr_table[i]=L'\u2590'; break; /* ??? - RIGHT HALF BLOCK */
				case 0xdf: chr_table[i]=L'\u2580'; break; /* ??? - UPPER HALF BLOCK */
				case 0xe0: chr_table[i]=L'\u03b1'; break; /* small alpha */
				case 0xe1: chr_table[i]=L'\u00df'; break; /* small beta / sharp s */
				case 0xe2: chr_table[i]=L'\u0393'; break; /* small gamma */
				case 0xe3: chr_table[i]=L'\u03c0'; break; /* small pi */
				case 0xe4: chr_table[i]=L'\u03a3'; break; /* Capital sigma */
				case 0xe5: chr_table[i]=L'\u03c3'; break; /* Small sigma */
				case 0xe6: chr_table[i]=L'\u00b5'; break; /* My / mu */
				case 0xe7: chr_table[i]=L'\u03c4'; break; /* Tau */
				case 0xe8: chr_table[i]=L'\u03a6'; break; /* Small Phi */
				case 0xe9: chr_table[i]=L'\u0398'; break; /* Theta */
				case 0xea: chr_table[i]=L'\u03a9'; break; /* Capital Omega */
				case 0xeb: chr_table[i]=L'\u03b4'; break; /* Small Delta / small eth */
				case 0xec: chr_table[i]=L'\u221e'; break; /* Infinity symbol */
				case 0xed: chr_table[i]=L'\u03c6'; break; /* Capitcal Phi */
				case 0xee: chr_table[i]=L'\u03b5'; break; /* Small Epsilon */
				case 0xef: chr_table[i]=L'\u2229'; break; /* Intersection */
				case 0xf0: chr_table[i]=L'\u2261'; break; /* mid char of long peak power level - TRIPPLE BAR */
				case 0xf1: chr_table[i]=L'\u00b1'; break; /* PLUS MINUS */
				case 0xf2: chr_table[i]=L'\u2265'; break; /* BIGGER OR EQUAL */
				case 0xf3: chr_table[i]=L'\u2264'; break; /* SMALLER OR EQUAL */
				case 0xf4: chr_table[i]=L'\u2320'; break; /* Integral symbol, upper part */
				case 0xf5: chr_table[i]=L'\u2321'; break; /* Integral symbol, lower part */
				case 0xf6: chr_table[i]=L'\u00f7'; break; /* Obelus symbol */
				case 0xf7: chr_table[i]=L'\u2248'; break; /* Approximation symbol */
				case 0xf8: chr_table[i]=L'\u00b0'; break; /* degree symbol */
				case 0xf9: chr_table[i]=L'\u2219'; break; /* bullet              - Bullet Operator (square) */
				case 0xfa: chr_table[i]=L'\u00b7'; break; /* bullet              - Middle Dot (circle) */
				case 0xfb: chr_table[i]=L'\u221a'; break; /* Square root sign */
				case 0xfc: chr_table[i]=L'\u207f'; break; /* Superscripted n */
				case 0xfd: chr_table[i]=L'\u00b2'; break; /* square */
				case 0xfe: chr_table[i]=L'\u25a0'; break; /* used by volume bars - Black Square */
			}
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
		iconv_t cd;

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

		cd = iconv_open(temp, OCP_FONT);
		if (cd == (iconv_t)(-1))
		{
			fprintf (stderr, "curses: Failed to make iconv matrix for %s->%s\n", temp, OCP_FONT);
		} else {
			fprintf (stderr, "curses: Converting between %s -> %s\n", temp, OCP_FONT);
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
					case 0x01: t = 'S'; break; /* Surround */
					case 0x04: t = ACS_DIAMOND; break; /* looks good on the header line */
					case 0x1a:
					case 0x10: t = ACS_RARROW;
					case 0x1b:
					case 0x11: t = ACS_LARROW;
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
					if (cd != (iconv_t)(-1))
					{
						char src[1];
						char dst[16];
						char *to=dst, *from=src;
						size_t _to=16, _from=1;
						src[0]=(char)i;
						if (iconv(cd, &from, &_from, &to, &_to) != (size_t)-1)
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
								chr_table[i]=(unsigned char)dst[0];
							}
						}
					}
				}
			}
		}

		if (cd != (iconv_t)-1)
		{
			iconv_close(cd);
		}

		cd = iconv_open("ISO-8859-1", OCP_FONT);
		if (cd == (iconv_t)(-1))
		{
			fprintf (stderr, "curses: Failed to make iconv matrix for ISO-8859-1 -> %s\n", OCP_FONT);
		} else {
			fprintf (stderr, "curses: Converting between ISO-8859-1 -> %s\n", OCP_FONT);
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
				if (cd != (iconv_t)(-1))
				{
					char src[1];
					char dst[16];
					char *to=dst, *from=src;
					size_t _to=16, _from=1;
					src[0]=(char)i;
					if (iconv(cd, &from, &_from, &to, &_to) != (size_t)-1)
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

		if (cd != (iconv_t)-1)
		{
			iconv_close(cd);
		}
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

	conSave();
#if CAN_RESIZE
	signal(SIGWINCH, adjust);        /* arrange interrupts to resize */
#endif
	_displayvoid=displayvoid;
	_displaystrattr=displaystrattr;
	_displaystr=displaystr;
	_displaystrattr_iso8859latin1=displaystrattr_iso8859latin1;
	_displaystr_iso8859latin1=displaystr_iso8859latin1;
	___setup_key(ekbhit, egetch); /* filters in more keys */
	_plSetTextMode=plSetTextMode;
	_drawbar=drawbar;
	_idrawbar=idrawbar;

	_conRestore=conRestore;
	_conSave=conSave;
	_plDosShell=plDosShell;

	_setcur=setcur;
	_setcurshape=setcurshape;

	_plGetDisplayTextModeName = plGetDisplayTextModeName;

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

	plVidType=vidNorm;
	plScrType=0;
	plScrMode=0;
	RefreshScreen();

	Height=plScrHeight=LINES;
	if ((Width=plScrWidth=COLS)>CONSOLE_MAX_X)
		Width=plScrWidth=CONSOLE_MAX_X;
	else if (plScrWidth<80)
		Width=plScrWidth=80; /* If a console gets smaller than THIS, the user-experience will be non-normal */

	conRestore();

	return 0;
}

void curses_done(void)
{
	conRestore();
}
