/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2011-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * /dev/vcsa* console driver
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
 * revision history: (please note changes here)
 *
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040919   Stian Skjelstad <stian@nixia.no>
 *    -Finally setcur/setcurshape that works
 */

#define _CONSOLE_DRIVER

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/vt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef FIX_BACKSPACE
#include <sys/kd.h>
#include <sys/ioctl.h>
#endif
#include <sys/poll.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <iconv.h>

#include "types.h"

#include "boot/console.h"
#include "stuff/pfonts.h"
#include "stuff/poutput.h"
#include "stuff/poutput-fb.h"
#include "stuff/poutput-keyboard.h"
#include "stuff/poutput-vcsa.h"
#include "stuff/utf-8.h"
#include "boot/psetting.h"

/* TODO ioctl KDMAPDISP, GIO_SCRNMAP */
static unsigned short plScrRowBytes;
static char *vgatextram, *consoleram;
static int vgamemsize;
static int vgafd;
static struct {unsigned char lines, cols, x, y;} scrn;

static char chr_table[256];

static struct termios orgterm, ocpterm;

static unsigned char bartops[18]="\xB5\xB6\xB6\xB7\xB7\xB8\xBD\xBD\xBE\xC6\xC6\xC7\xC7\xCF\xCF\xD7\xD7";
static unsigned char ibartops[18]="\xB5\xD0\xD0\xD1\xD1\xD2\xD2\xD3\xD3\xD4\xD4\xD5\xD5\xD6\xD6\xD7\xD7";
static int font_replaced=0;

static const char *activecharset;
static iconv_t activecharset_cd = (iconv_t)(-1);

/* Replace font stuff goes here */

static unsigned char orgfont[512*32];
static struct console_font_op orgfontdesc;

static void vcsa_SetCursorShape (uint16_t shape);

static struct consoleDriver_t vcsaConsoleDriver;

static void set_kernel_sizes(int x, int y)
{
	struct vt_sizes sizes;
	sizes.v_rows=y;
	sizes.v_cols=x;
	sizes.v_scrollsize=x*4;
	if (ioctl(1, VT_RESIZE, &sizes))
		perror("ioctl(1, VT_RESIZE, &sizes)");
}

static void vcsa_setplScrType(void)
{
	lseek(vgafd, 0, SEEK_SET);
	while (read(vgafd, &scrn, sizeof(scrn))<0)
	{
		if (errno==EAGAIN)
			continue;
		if (errno==EINTR)
			continue;
		fprintf(stderr, __FILE__ " read() failed #1\n");
		exit(1);
	}

	set_kernel_sizes(scrn.cols, scrn.lines);

	Console.TextHeight = scrn.lines;
	Console.TextWidth  = scrn.cols;
	plScrRowBytes = scrn.cols * 2;

	/* first we set the VERY default value */
	if (Console.TextHeight < 50)
	{
		Console.LastTextMode = 0;
	} else {
		Console.LastTextMode = 2;
	}

	/* this code should cover most vga modes, and matches the old OCP code */
	if (Console.TextWidth == 80)
	{
		switch (Console.TextHeight)
		{
			case 25: Console.LastTextMode = 0; break;
			case 50: Console.LastTextMode = 2; break;
			case 60: Console.LastTextMode = 3; break;
		}
	} else if((Console.TextWidth==132) || (Console.TextWidth==128))
	{
		switch (Console.TextHeight)
		{
			case 25: Console.LastTextMode = 4; break;
			case 30: Console.LastTextMode = 5; break;
			case 50: Console.LastTextMode = 6; break;
			case 60: Console.LastTextMode = 7; break;
		}
	}
}

static int set_font(int lines, int verbose)
{
	static struct console_font_op newfontdesc;
	static unsigned char newfont[256*32];
	int i;

	newfontdesc.op=KD_FONT_OP_SET;
	newfontdesc.flags=0;
	newfontdesc.height=8;
	newfontdesc.width=8;
	newfontdesc.charcount=256;
	newfontdesc.data=newfont;
	memset(newfont, 0, sizeof(newfont));
	if ((newfontdesc.height=lines)==8)
	{
		for (i=0;i<256;i++)
			memcpy(newfont+i*32, plFont88[i], 8);
	} else {
		for (i=0;i<256;i++)
			memcpy(newfont+i*32, plFont816[i], 16);
	}
	if (ioctl(1, KDFONTOP, &newfontdesc))
	{
#ifdef VCSA_VERBOSE
		if (verbose)
			perror("ioctl(1, KDFONTOP, &newfontdesc)");
#endif
		return -1;
	}
	/* KERNEL-BUGS ARE SO FUN!!!.. if you get 16 lines, using 8 line font or the oposite way around, the stuff below helps */
/*
	newfontdesc.height=16;
	ioctl(1, KDFONTOP, &newfontdesc);
	newfontdesc.height=lines;
	ioctl(1, KDFONTOP, &newfontdesc);*/

	vcsa_SetCursorShape (255);
	font_replaced=lines;
	return 0;
}

static int init_fonts(void)
{
	int i;

#ifdef VCSA_VERBOSE
	fprintf(stderr, "vcsa: Storing the original font.. ");
#endif

	orgfontdesc.op=KD_FONT_OP_GET;
	orgfontdesc.flags=0;
	orgfontdesc.height=32;
	orgfontdesc.width=8;
	orgfontdesc.charcount=512;
	orgfontdesc.data=orgfont;
	if (ioctl(1, KDFONTOP, &orgfontdesc))
	{
#ifdef VCSA_VERBOSE
		perror("ioctl(1, KDFONTOP, &orgfontdesc)");
		fprintf(stderr, "failed\n");
#endif
		return -1;
	}
#ifdef VCSA_VERBOSE
	fprintf(stderr, "vcsa: Attempting to upload new fonts.. ");
#endif
	if ((orgfontdesc.height!=8)&&(orgfontdesc.height!=16))
		return -1;
	/* TODO Store UNICODE tables from kernel */
#ifdef VCSA_VERBOSE
	fprintf(stderr, "%d lines font.. ", orgfontdesc.height);
#endif
	chr_table[0]=32;

	if (set_font(orgfontdesc.height, 1))
	{
#ifdef VCSA_VERBOSE
		fprintf(stderr, " ..Failed\n");
#endif
		return -1;
	}
	for (i=1;i<256;i++)
		chr_table[i]=i;

#ifdef VCSA_VERBOSE
	fprintf(stderr, "Ok\n");
#endif

	activecharset = OCP_FONT;
	activecharset_cd = iconv_open (OCP_FONT "//TRANSLIT", "UTF-8");
	if ((iconv_t)(-1)==activecharset_cd)
	{
		activecharset_cd = iconv_open (OCP_FONT, "UTF-8");
	}
	return 0;
}

void restore_fonts(void)
{
	if (!font_replaced)
		return;
	font_replaced=0;
	orgfontdesc.op=KD_FONT_OP_SET;
	if (ioctl(1, KDFONTOP, &orgfontdesc))
	{
#ifdef VCSA_VERBOSE
		perror("\nioctl(1, KDFONTOP, &orgfontdesc)");
#endif
		return;
	}
}

/* this stuff is if we fail to replace fonts */
static void make_chr_table(void)
{
	/* TODO ioctl(GIO_UNISCRNMAP) */
	char src[256];
	int i;
	iconv_t cd;
	char *to=chr_table, *from=src;
	size_t _to=256, _from=256;

#ifdef VCSA_VERBOSE
	fprintf(stderr, "vcsa: Making iconv conversion for characters to display\n");
#endif

	for (i=0;i<256;i++)
	{
		src[i]=i;
		chr_table[i]=i; /* in case we bail out */
	}
	cd = iconv_open(VCSA_FONT "//TRANSLIT", OCP_FONT);
	if ((iconv_t)(-1)==cd)
	{
#ifdef VCSA_VERBOSE
		fprintf(stderr, "vcsa: Failed to make iconv matrix for %s->%s\n", OCP_FONT, VCSA_FONT);
#endif
		return;
	}
	while (1)
	{
		iconv(cd, &from, &_from, &to, &_to);
		if (_to&&_from)
		{
			if (((unsigned char)*from)==0xfe)
				*from='#';
			*(to++)=*(from++);
			_to--; _from--;
		}
		if (!(_to&&_from))
			break;
	}
	iconv_close(cd);

	chr_table[0]=32; /* quick dirty hack */

	strcpy((char *)bartops, "  ___...---===**X");

	activecharset = VCSA_FONT;
	activecharset_cd = iconv_open (VCSA_FONT "//TRANSLIT", "UTF-8");
	if ((iconv_t)(-1)==activecharset_cd)
	{
		activecharset_cd = iconv_open (VCSA_FONT, "UTF-8");
	}
}

/* font stuff done */

static void vgaMakePal (void)
{
	int pal[16];
	char palstr[1024];
	int bg,fg;
	char scol[4];
	char const *ps2=palstr;

	strcpy(palstr,cfGetProfileString2(cfScreenSec, "screen", "palette", "0 1 2 3 4 5 6 7 8 9 A B C D E F"));

	for (bg=0; bg<16; bg++)
		pal[bg]=bg;

	bg=0;
	while (cfGetSpaceListEntry(scol, &ps2, 2) && bg<16)
		pal[bg++]=strtol(scol,0,16)&0x0f;

	for (bg=0; bg<16; bg++)
		for (fg=0; fg<16; fg++)
			plpalette[16*bg+fg]=16*pal[bg]+pal[fg];
}

static void vcsa_DisplayChr (uint16_t y, uint16_t x, uint8_t attr, char chr, uint16_t len)
{
	char *p=vgatextram+(y*plScrRowBytes+x*2);
	unsigned short i;

	attr=plpalette[attr];
	for (i=0; i<len; i++)
	{
		*p++=chr_table[(uint8_t)chr];
		*p++=attr;
	}
}

static void vcsa_DisplayStr (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	char *p=vgatextram+(y*plScrRowBytes+x*2);
	unsigned short i;

	attr=plpalette[attr];
	for (i=0; i<len; i++)
	{
		*p++=chr_table[(unsigned char)*str];
		if (*str)
			str++;
		*p++=attr;
	}
}

static void vcsa_DisplayStrAttr (uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	char *p=vgatextram+(y*plScrRowBytes+x*2);
	unsigned char *b=(unsigned char *)buf;
	int i;
	for (i=0; i<len*2; i+=2)
	{
		p[i]=chr_table[b[i]];
		p[i+1]=plpalette[b[i+1]];
	}
}

static void vcsa_DisplayStr_utf8 (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	if ((iconv_t)(-1)==activecharset_cd)
	{
		vcsa_DisplayStr (y, x, attr, str, len);
		return;
	} else {
		char   tobuffer[CONSOLE_MAX_X+1];
		char  *to = tobuffer;
		size_t tolen = len;
		char  *from = (char *)str;
		size_t fromlen = strlen (str);
		while (fromlen)
		{
			iconv (activecharset_cd, &from, &fromlen, &to, &tolen);
			if (tolen&&fromlen)
			{
				int inc = 0;

				utf8_decode (from, fromlen, &inc);

				*to='?';
				tolen--;

				fromlen -= inc;
				from += inc;
			} else {
				break;
			}
		}
		if (tolen)
		{
			*to = 0;
		}
		vcsa_DisplayStr (y, x, attr, tobuffer, len);
	}
}

static int vcsa_MeasureStr_utf8 (const char *str, int srclen)
{
	int retval;
	for (retval = 0; str[retval] && retval < srclen; retval++)
	{
	}
	return retval;
}


static void vcsa_DisplayVoid (uint16_t y, uint16_t x, uint16_t len)
{
	char *addr=vgatextram+y*plScrRowBytes+x*2;
	while (len--)
	{
		*addr++=0;
		*addr++=plpalette[0];
	}
}

static void vcsa_iDrawBar (uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
{
	unsigned int i;
	char *scrptr;
	uint16_t yh1=(height+2)/3;
	uint16_t yh2=(height+yh1+1)/2;

	y-=height-1;

	if (value>((unsigned int)(height*16)-4))
	{
		value=(height*16)-4;
	}

	scrptr=vgatextram+(2*x+y*plScrRowBytes);

	for (i=0; i<yh1; i++, scrptr+=plScrRowBytes)
	{
		uint32_t v = ( value >= 16 ) ? 16 : value;
		value -= v;
		scrptr[0]=chr_table[ibartops[v]];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh1; i<yh2; i++, scrptr+=plScrRowBytes)
	{
		uint32_t v = ( value >= 16 ) ? 16 : value;
		value -= v;
		scrptr[0]=chr_table[ibartops[v]];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh2; i<height; i++, scrptr+=plScrRowBytes)
	{
		uint32_t v = ( value >= 16 ) ? 16 : value;
		value -= v;
		scrptr[0]=chr_table[ibartops[v]];
		scrptr[1]=plpalette[c&0xFF];
	}
}

static void vcsa_DrawBar (uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
{
	unsigned int i;
	char *scrptr;
	uint16_t yh1=(height+2)/3;
	uint16_t yh2=(height+yh1+1)/2;

	if (value>((unsigned int)(height*16)-4))
	{
		value=(height*16)-4;
	}

	scrptr=vgatextram+(2*x+y*plScrRowBytes);

	for (i=0; i<yh1; i++, scrptr-=plScrRowBytes)
	{
		uint32_t v = ( value >= 16 ) ? 16 : value;
		value -= v;
		scrptr[0]=chr_table[bartops[v]];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh1; i<yh2; i++, scrptr-=plScrRowBytes)
	{
		uint32_t v = ( value >= 16 ) ? 16 : value;
		value -= v;
		scrptr[0]=chr_table[bartops[v]];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh2; i<height; i++, scrptr-=plScrRowBytes)
	{
		uint32_t v = ( value >= 16 ) ? 16 : value;
		value -= v;
		scrptr[0]=chr_table[bartops[v]];
		scrptr[1]=plpalette[c&0xFF];
	}
}

static void vcsa_SetTextMode (uint8_t x)
{
	unsigned int i;

#ifdef VCSA_DEBUG
	fprintf(stderr, "vcsa: set mode %d\n", x);
#endif
	if (vcsaConsoleDriver.SetGraphMode)
	{
		vcsaConsoleDriver.SetGraphMode (-1);
	}
	Console.CurrentMode = 0;

	if (font_replaced)
	{
		switch (x)
		{
			case 0:
			case 1: /* 80 * 25 */
			case 4: /* 132 * 25 */
			case 5: /* 132 * 30 */
				/*if (((Console.TextHeight==50) || (Console.TextHeight==60)) && font_replaced==8)
				{
					if (!set_font(16, 0))
					{
						font_replaced=16;
						Console.TextHeight /= 2;
						vcsa_setplScrType();
					}
				}*/
				set_font(16, 0);
				break;
			case 2: /* 80 * 50 */
			case 3: /* 80 * 60 */
			case 6: /* 132 * 50 */
			case 7: /* 132 * 60 */
				/*if (((Console.TextHeight==25) || (Console.TextHeight==30)) && font_replaced==16)
				{
					if (!set_font(8, 0))
					{
						font_replaced=8;
						Console.TextHeight *= 2;
						vcsa_setplScrType();
					}
				}*/
				set_font(8, 0); /* force reload font */
				break;
		}
	}

	vcsa_setplScrType ();

	for (i = 0; i < Console.TextHeight; i++)
	{
		vcsa_DisplayVoid (i, 0, Console.TextWidth);
	}
}

static void vcsa_DisplaySetupTextMode (void)
{
	/* dummy */
}

static const char *vcsa_GetDisplayTextModeName (void)
{
	static char mode[16];
	snprintf(mode, sizeof(mode), "%d x %d", scrn.cols, scrn.lines);
	return mode;
}

static int conactive=0;

static int vcsa_consoleRestore (void)
{
	if (!conactive)
		return 0;
	tcsetattr(0, TCSANOW, &orgterm);
	lseek(vgafd, 0, SEEK_SET);
	while (write(vgafd, consoleram, vgamemsize+4) < 0)
	{
		if (errno==EAGAIN)
			continue;
		if (errno==EINTR)
			continue;
		fprintf(stderr, __FILE__ " write() failed #1\n");
		exit(1);
	}
	conactive=0;
	return 0;
}

static void vcsa_consoleSave (void)
{
	if (conactive)
		return;
	fflush(stderr);
	lseek(vgafd, 0, SEEK_SET);
	while (read(vgafd, consoleram, vgamemsize+4) < 0)
	{
		if (errno==EAGAIN)
			continue;
		if (errno==EINTR)
			continue;
		fprintf(stderr, __FILE__ " read() failed #2\n");
		exit(1);
	}
	tcsetattr(0, TCSANOW, &ocpterm);
	conactive=1;
}

static void vcsa_DosShell (void)
{
#warning implement me
	return;
}

static int sigintcounter = 0;
 
static int ekbhit_linux(void)
{
	struct pollfd set;

	if (!Console.CurrentMode)
	{
		lseek(vgafd, 4, SEEK_SET);
		while (write(vgafd, vgatextram, vgamemsize) < 0)
		{
			if (errno==EAGAIN)
				continue;
			if (errno==EINTR)
				continue;
			fprintf(stderr, __FILE__ " write() failed #2\n");
			exit(1);
		}
	}

	set.fd=0;
	set.events=POLLIN;
	poll(&set, 1, 0);

	return !!set.revents;
}

static int egetch_linux(void)
{
	unsigned char key_buffer[128];
	unsigned char key_len, i=0;
	int result;

	if (sigintcounter)
	{
		sigintcounter--;
		return 27;
	}

	if (!ekbhit_linux())
	{
		return 0;
	}

	result=read(0, key_buffer, 128);
	if (result>0)
		key_len=result;
	else
		return 0;
	for (i=0;i<key_len;i++)
	{
		if (key_buffer[i]==127)
			___push_key(KEY_DELETE);
		else
			___push_key(key_buffer[i]);
	}
	return 0;
}
static void vcsa_SetCursorShape (uint16_t shape)
{
	char *buffer="";
	int l;

	switch (shape)
	{
		case 0: buffer="\033[?1c"; break;
		case 1: buffer="\033[?5c"; break;
		case 2: buffer="\033[?15c"; break;
		default: break;
	}
	l=strlen(buffer);
	while (1)
	{
		if (write(1, buffer, l)==l)
			break;
		if (errno!=EINTR)
			break;
	}
}

static void vcsa_SetCursorPosition (uint16_t y, uint16_t x)
{
	scrn.x=x;
	scrn.y=y;
	lseek(vgafd, 0, SEEK_SET);
	while (write(vgafd, &scrn, sizeof(scrn)) < 0)
	{
		if (errno==EAGAIN)
			continue;
		if (errno==EINTR)
			continue;
		fprintf(stderr, __FILE__ " write() failed #3\n");
		exit(1);
	}
}

static void sigint(int signal)
{
	sigintcounter++;
	if (sigintcounter > 2) /* program is frozen and we already have two presses in the queue */
	{
		kill (getpid(), SIGQUIT);
	}
}

static int need_fb_done = 0;

int vcsa_init(int minor)
{
	char path[128];

	if (minor)
		snprintf(path, 128, "%s%d", VCSA_PATH, minor);
	else
		snprintf(path, 128, "%s", VCSA_PATH);

	if ((vgafd=open(path, O_RDWR))<0)
#ifdef VCSA_VERBOSE
	{
		char tmp[22+128];
		snprintf(tmp, sizeof (tmp), "vcsa: open(%s, O_RDWR)", path);
		perror(tmp);
		return -1;
	} else {
		fprintf(stderr, "vcsa: Successfully opened %s:\n", path);
	}
#else
		return -1;
#endif

	while (read(vgafd, &scrn, sizeof(scrn)) < 0)
	{
		if (errno==EAGAIN)
			continue;
		if (errno==EINTR)
			continue;
		fprintf(stderr, __FILE__ " read() failed #3\n");
		exit(1);
	}

	Console.TextHeight = scrn.lines;
	Console.TextWidth  = scrn.cols;
	plScrRowBytes = scrn.cols * 2;

	vgamemsize = Console.TextHeight * Console.TextWidth * 2;
	vgamemsize *= 2;
	vgatextram = calloc(vgamemsize, 1);
	consoleram = calloc(vgamemsize+4, 1);

#ifdef VCSA_VERBOSE
	fprintf(stderr, "vcsa: %dx%d => %d bytes buffer\n", Console.TextWidth, Console.TextHeight, vgamemsize);
#endif

	{
		struct kbentry k;
#ifdef VCSA_VERBOSE
		fprintf(stderr, "vcsa: Setting up non-blocking keyboard..\n");
#endif
		if (tcgetattr(0, &orgterm))
		{
#ifdef VCSA_VERBOSE
			perror("tcgetattr()");
#endif
			close(vgafd);
			return -1;
		}
		memcpy(&ocpterm, &orgterm, sizeof(struct termios));
		cfmakeraw(&ocpterm);
		memset(ocpterm.c_cc, 0, sizeof(ocpterm.c_cc));
		if (tcsetattr(0, TCSANOW, &ocpterm))
		{
#ifdef VCSA_VERBOSE
			perror("vcsa: tcsetattr()");
#endif
			close(vgafd);
			return -1;
		}
		tcsetattr(0, TCSANOW, &orgterm),

#ifdef FIX_BACKSPACE
		/* On a lot of linux distros, the backspace and delete keys are
		 * merged together. This fixes the backspace key to make
		 * character 8, and leaves the delete button to continue make
		 * the usual strange 0x7f / CTRL-? code.
		 *
		 * - Stian, 2004
		 */
		fprintf(stderr, "vcsa: Trying to make backspace button unique (ctrl-h)\n");

		k.kb_table=0;
		k.kb_index=14;
		k.kb_value=8;

		if (ioctl(0, KDSKBENT, &k))
		{
#ifdef VCSA_VERBOSE
			perror("vcsa: ioctl(0, KDSKBENT, {0, BS_KEY, 8})");
#endif
		}
#endif
	}

	Console.Driver = &vcsaConsoleDriver;
	___setup_key(ekbhit_linux, egetch_linux);

	if (init_fonts())
		make_chr_table();
	vgaMakePal();
	vcsa_setplScrType();

	signal(SIGINT, sigint); /* emulate ESC key on ctrl-c, but multiple presses + OCP deadlocked will make it quit by force */

#ifdef VCSA_VERBOSE
	fprintf(stderr, "vcsa: driver is online\n");
#endif
	Console.VidType = vidNorm;

	if (!fb_init (minor, &vcsaConsoleDriver))
	{
		need_fb_done = 1;
	}
	return 0;
}

void vcsa_done(void)
{
	if (need_fb_done)
	{
		fb_done ();
		need_fb_done = 0;
	}
	restore_fonts();
	tcsetattr(0, TCSANOW, &orgterm);

	vcsa_consoleRestore ();
	vcsa_SetCursorShape (1);

	free(vgatextram);
	free(consoleram);
	close(vgafd);
	vgafd=-1;

	if (activecharset_cd != (iconv_t)(-1))
	{
		iconv_close (activecharset_cd);
		activecharset_cd = (iconv_t)(-1);
	}
}

static struct consoleDriver_t vcsaConsoleDriver =
{
	0,   /* vga13 */
	vcsa_SetTextMode,
	vcsa_DisplaySetupTextMode,
	vcsa_GetDisplayTextModeName,
	vcsa_MeasureStr_utf8,
	vcsa_DisplayStr_utf8,
	vcsa_DisplayChr,
	vcsa_DisplayStr,
	vcsa_DisplayStrAttr,
	vcsa_DisplayVoid,
	vcsa_DrawBar,
	vcsa_iDrawBar,
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
	consoleHasKey,
	vcsa_SetCursorPosition,
	vcsa_SetCursorShape,
	vcsa_consoleRestore,
	vcsa_consoleSave,
	vcsa_DosShell
};
