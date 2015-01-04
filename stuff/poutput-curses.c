/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

#include <curses.h>
#include <errno.h>
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

#if defined(SIGWINCH) && defined(TIOCGWINSZ) && HAVE_RESIZETERM
#define CAN_RESIZE 1
#else
#undef CAN_RESIZE
#endif

#if CAN_RESIZE
static void adjust(int sig);
#endif

static chtype attr_table[256];
static chtype chr_table[256];

static int Width, Height;

static int fixbadgraphic;

static void displayvoid(unsigned short y, unsigned short x, unsigned short len)
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

static void displaystr(unsigned short y, unsigned short x, unsigned char attr, const char *buf, unsigned short len)
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


static void displaystrattr(unsigned short y, unsigned short x, const uint16_t *buf, unsigned short len)
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
			} else
				output=chr_table['X']|attr_table[plpalette[(attr&0xf0)+((attr>>4)&0xf)]];
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

static unsigned char bartops[18]="  ___...---===**#";
static unsigned char ibartops[18]="  ```^^^~~~===**#";
static void idrawbar(uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
{
	char buf[60];
	unsigned int i;
	uint16_t yh1=(height+2)/3;
	uint16_t yh2=(height+yh1+1)/2;

	if (value>((unsigned int)(height*16)-4))
		value=(height*16)-4;

	for (i=0; i<height; i++)
	{
		if (value>=16)
		{
			buf[i]=ibartops[16];
			value-=16;
		} else {
			buf[i]=ibartops[value];
			value=0;
		}
	}
	y-=height-1;
	for (i=0; i<yh1; i++)
	{
		displaystr(y, x, c&0xff, buf+i, 1);
		y++;
	}
	c>>=8;
	for (i=yh1; i<yh2; i++)
	{
		displaystr(y, x, c&0xff, buf+i, 1);
		y++;
	}
	c>>=8;
	for (i=yh2; i<height; i++)
	{
		displaystr(y, x, c&0xff, buf+i, 1);
		y++;
	}
}
static void drawbar(uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
{
	char buf[60];
	unsigned int i;
	uint16_t yh1=(height+2)/3;
	uint16_t yh2=(height+yh1+1)/2;

	if (value>((unsigned int)(height*16)-4))
		value=(height*16)-4;

	for (i=0; i<height; i++)
	{
		if (value>=16)
		{
			buf[i]=bartops[16];
			value-=16;
		} else {
			buf[i]=bartops[value];
			value=0;
		}
	}
	for (i=0; i<yh1; i++)
	{
		displaystr(y, x, c&0xff, buf+i, 1);
		y--;
	}
	c>>=8;
	for (i=yh1; i<yh2; i++)
	{
		displaystr(y, x, c&0xff, buf+i, 1);
		y--;
	}
	c>>=8;
	for (i=yh2; i<height; i++)
	{
		displaystr(y, x, c&0xff, buf+i, 1);
		y--;
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

	fprintf(stderr, "Initing curses... (%s)\n",  curses_version());
	if ((fixbadgraphic=cfGetProfileBool("curses", "fixbadgraphic", 0, 0)))
		fprintf(stderr, "curses: fixbadgraphic is enabled in config\n");

	if (!initscr())
	{
		fprintf(stderr, "curses failed to init\n");
		return -1;
	}
	conSave();
#if CAN_RESIZE
	signal(SIGWINCH, adjust);        /* arrange interrupts to resize */
#endif
	_displayvoid=displayvoid;
	_displaystrattr=displaystrattr;
	_displaystr=displaystr;
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

		if (i&0x08)
			attr_table[i]|=A_BOLD;
		if (i&0x80)
			attr_table[i]|=A_STANDOUT;

		if (i<32)
			chr_table[i]=i+32;
		else if (i>127)
			chr_table[i]='_';
		else
			chr_table[i]=i;
	}

	chr_table[0x00]=' ';
	chr_table[0x01]='S'; /* Surround */
	chr_table[0x04]=ACS_DIAMOND; /* looks good on the header line */
	chr_table[0x08]='?'; /* ??? */
	chr_table[0x09]='?'; /* ??? */

	chr_table[0x0a]='@'; /* when the heck does this occure */
	chr_table[0x07]='@'; /* and this?*/

	chr_table[0x0d]='@'; /* we want a note */
	chr_table[0x10]=ACS_RARROW;
	chr_table[0x11]=ACS_LARROW;
	chr_table[0x12]=ACS_PLMINUS; /* we want an up+down arrow */
/*
	chr_table[0x12]='&'; *//* retrigger */
	chr_table[0x18]=ACS_UARROW;
	chr_table[0x19]=ACS_DARROW;
	chr_table[0x1a]='`'; /* pitch? */
	chr_table[0x1b]='\''; /* pitch? */
	chr_table[0x1d]=ACS_PLUS; /* speed pitch lock */
	chr_table[(unsigned char)0x81]='u'; /* ?? */
	chr_table[(unsigned char)0xb3]=ACS_VLINE;
	chr_table[(unsigned char)0xba]=ACS_VLINE;
	chr_table[(unsigned char)0xbf]=ACS_URCORNER;
	chr_table[(unsigned char)0xc0]=ACS_LLCORNER;
	chr_table[(unsigned char)0xc1]=ACS_BTEE;
	chr_table[(unsigned char)0xc2]=ACS_TTEE;
	chr_table[(unsigned char)0xc3]=ACS_LTEE;
	chr_table[(unsigned char)0xc4]=ACS_HLINE;
	chr_table[(unsigned char)0xd9]=ACS_LRCORNER;
	chr_table[(unsigned char)0xda]=ACS_ULCORNER;
	chr_table[(unsigned char)0xdd]='#';
	chr_table[(unsigned char)0xf0]='#'; /* mid char of long peak power level */
	chr_table[(unsigned char)0xfa]=ACS_BULLET;
	chr_table[(unsigned char)0xf9]=ACS_BULLET/*|A_BOLD*/;
	chr_table[(unsigned char)0xfe]=ACS_BLOCK; /* used by volume bars */

	plVidType=vidNorm;
	plScrType=0;
	plScrMode=0;
	RefreshScreen();

	Height=plScrHeight=LINES;
	if ((Width=plScrWidth=COLS)>CONSOLE_MAX_X)
		Width=plScrWidth=CONSOLE_MAX_X;
	else if (plScrWidth<80)
		Width=plScrWidth=80; /* If a console gets smaller than THIS, the user deserves to get fucked up his or her ASS */

	conRestore();

	return 0;
}

void curses_done(void)
{
	conRestore();
}
