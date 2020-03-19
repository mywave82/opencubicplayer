/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef FIX_BACKSPACE
#include <sys/kd.h>
#include <sys/ioctl.h>
#endif
#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>
#include <iconv.h>

#include "types.h"

#include "poutput-vcsa.h"
#include "boot/console.h"
#include "stuff/poutput.h"
#include "boot/psetting.h"
#include "pfonts.h"

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

/* Replace font stuff goes here */

static unsigned char orgfont[512*32];
static struct console_font_op orgfontdesc;

static void setcurshape(unsigned short shape);

static void set_kernel_sizes(int x, int y)
{
	struct vt_sizes sizes;
	sizes.v_rows=y;
	sizes.v_cols=x;
	sizes.v_scrollsize=x*4;
	if (ioctl(1, VT_RESIZE, &sizes))
		perror("ioctl(1, VT_RESIZE, &sizes)");
}

static void set_plScrType(void)
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

	plScrHeight = scrn.lines;
	plScrWidth = scrn.cols;
	plScrRowBytes = scrn.cols * 2;

	/* first we set the VERY default value */
	if (plScrHeight<50)
		plScrType=0;
	else
		plScrType=2;

	/* this code should cover most vga modes, and matches the old OCP code */
	if (plScrWidth==80)
	{
		switch (plScrHeight)
		{
			case 25: plScrType=0; break;
			case 50: plScrType=2; break;
			case 60: plScrType=3; break;
		}
	} else if((plScrWidth==132)||(plScrWidth==128))
	{
		switch (plScrHeight)
		{
			case 25: plScrType=4; break;
			case 30: plScrType=5; break;
			case 50: plScrType=6; break;
			case 60: plScrType=7; break;
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

	setcurshape(255);
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
}

/* font stuff done */

static void vgaMakePal(void)
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

void displaystr(unsigned short y, unsigned short x, unsigned char attr, const char *str, unsigned short len)
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

void displaystrattr(unsigned short y, unsigned short x, const unsigned short *buf, unsigned short len)
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


/*void displaystrattrdi(unsigned short y, unsigned short x, const unsigned char *txt, const unsigned char *attr, unsigned short len)
{
  char *p=vgatextram+(y*plScrRowBytes+x*2);
  unsigned short i;
  for (i=0; i<len; i++)
  {
    *p++=chr_table[*txt++];
    *p++=plpalette[*attr++];
  }
}*/

void displayvoid(unsigned short y, unsigned short x, unsigned short len)
{
	char *addr=vgatextram+y*plScrRowBytes+x*2;
	while (len--)
	{
		*addr++=0;
		*addr++=plpalette[0];
	}
}

void idrawbar(uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
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

void drawbar(uint16_t x, uint16_t y, uint16_t height, uint32_t value, uint32_t c)
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

static void plSetTextMode(unsigned char x)
{
	unsigned int i;

#ifdef VCSA_DEBUG
	fprintf(stderr, "vcsa: set mode %d\n", x);
#endif
	_plSetGraphMode(-1);
	plScrMode=0;

	if (font_replaced)
	{
		switch (x)
		{
			case 0:
			case 1: /* 80 * 25 */
			case 4: /* 132 * 25 */
			case 5: /* 132 * 30 */
				/*if (((plScrHeight==50)||(plScrHeight==60))&&font_replaced==8)
				{
					if (!set_font(16, 0))
					{
						font_replaced=16;
						plScrHeight/=2;
						set_plScrType();
					}
				}*/
				set_font(16, 0);
				break;
			case 2: /* 80 * 50 */
			case 3: /* 80 * 60 */
			case 6: /* 132 * 50 */
			case 7: /* 132 * 60 */
				/*if (((plScrHeight==25)||(plScrHeight==30))&&font_replaced==16)
				{
					if (!set_font(8, 0))
					{
						font_replaced=8;
						plScrHeight*=2;
						set_plScrType();
					}
				}*/
				set_font(8, 0); /* force reload font */
				break;
		}
	}

	set_plScrType();

	for (i=0;i<plScrHeight;i++)
		displayvoid(i, 0, plScrWidth);
}

static int conactive=0;

static int conRestore(void)
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

static void conSave(void)
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

static int ekbhit(void)
{
	struct pollfd set;

	if (!plScrMode)
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

static int egetch(void)
{
	unsigned char key_buffer[128];
	unsigned char key_len, i=0;
	int result;

	if (!ekbhit())
		return 0;
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
static void setcurshape(unsigned short shape)
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

static void setcur(unsigned char y, unsigned char x)
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

	plScrHeight = scrn.lines;
	plScrWidth = scrn.cols;
	plScrRowBytes = scrn.cols * 2;

	vgamemsize = plScrHeight * plScrWidth * 2;
	vgamemsize*=2;
	vgatextram = calloc(vgamemsize, 1);
	consoleram = calloc(vgamemsize+4, 1);

#ifdef VCSA_VERBOSE
	fprintf(stderr, "vcsa: %dx%d(%d) => %d bytes buffer\n", plScrWidth, plScrHeight, plScrHeight, vgamemsize);
#endif

	_plSetTextMode=plSetTextMode;
	_displaystr=displaystr;
	_setcur=setcur;
	_setcurshape=setcurshape;
	_displaystrattr=displaystrattr;
	_displayvoid=displayvoid;

	_drawbar=drawbar;
	_idrawbar=idrawbar;

	_conRestore=conRestore;
	_conSave=conSave;

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
		fprintf(stderr, "vcsa: Trying to make backspace button uniqe (ctrl-h)\n");

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
	___setup_key(ekbhit, egetch);
	if (init_fonts())
		make_chr_table();
	vgaMakePal();
	set_plScrType();
#ifdef VCSA_VERBOSE
	fprintf(stderr, "vcsa: driver is online\n");
#endif
	plVidType=vidNorm;
	return 0;
}

void vcsa_done(void)
{
	restore_fonts();
	tcsetattr(0, TCSANOW, &orgterm);

	conRestore();
	setcurshape(1);

	free(vgatextram);
	free(consoleram);
	close(vgafd);
	vgafd=-1;
}
