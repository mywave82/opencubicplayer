/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2015-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
 *  -ss040615   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -doj040914  Dirk Jagdmann  <doj@cubic.org>
 *    -trust the framebuffers smem_len
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -devfs/kernel 2.6 framebuffer names
 */

#define _CONSOLE_DRIVER

#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <linux/version.h>
#include "types.h"

#include "poutput-fb.h"
#include "boot/console.h"
#include "poutput.h"

static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo orgmode;
static struct fb_var_screeninfo lowres;
static struct fb_var_screeninfo highres;
static struct fb_cmap colormap;
static uint16_t red[256]=  {0x0000, 0x0000, 0x0000, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0x5555, 0x5555, 0x5555, 0x5555, 0xffff, 0xffff, 0xffff, 0xffff};
static uint16_t green[256]={0x0000, 0x0000, 0xaaaa, 0xaaaa, 0x0000, 0x0000, 0x5555, 0xaaaa, 0x5555, 0x5555, 0xffff, 0xffff, 0x5555, 0x5555, 0xffff, 0xffff};
static uint16_t blue[256]= {0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff};

static int fd;
static uint8_t *fbmem;

static int get_static_info(void)
{
	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix))
	{
		perror("fb: ioctl(1, FBIOGET_FSCREENINFO, &fix)");
		return -1;
	}
	return 0;
}

static void show_cursor(void)
{
#ifdef FBIOGET_CURSORSTATE
	struct fb_cursorstate cur;
	if (ioctl(fd, FBIOGET_CURSORSTATE, &cur, sizeof(cur)))
	{
		perror("fb: ioctl(1, FBIOGET_CURSORSTATE, &cursor)");
		return;
	}
	cur.mode=FB_CURSOR_FLASH;
	if (ioctl(fd, FBIOPUT_CURSORSTATE, &cur, sizeof(cur)))
	{
		perror("fb: ioctl(1, FBIOPUT_CURSORSTATE, &cursor)");
		return;
	}
#endif
}

static void hide_cursor(void)
{
#ifdef FBIOGET_CURSORSTATE
	struct fb_cursorstate cur;
	if (ioctl(fd, FBIOGET_CURSORSTATE, &cur))
	{
		perror("fb: ioctl(1, FBIOGET_CURSORSTATE, &cursor)");
		return;
	}
	cur.mode=FB_CURSOR_OFF;
	if (ioctl(fd, FBIOPUT_CURSORSTATE, &cur))
	{
		perror("fb: ioctl(1, FBIOGET_CURSORSTATE, &cursor)");
		return;
	}
#endif
}

static int test_mode(struct fb_var_screeninfo *info)
{
	int old;
	old=info->activate;
	info->activate=FB_ACTIVATE_TEST;
	if (ioctl(fd, FBIOPUT_VSCREENINFO, info))
	{
		perror("fb: ioctl(1, FBIOPUT_VSCREENINFO, info)");
		info->activate=old;
		return -1;
	}
	info->activate=old;
	return 0;
}

static void fb_gUpdatePal (uint8_t color, uint8_t _red, uint8_t _green, uint8_t _blue)
{
	red[color]=_red<<10;
	green[color]=_green<<10;
	blue[color]=_blue<<10;
}

static void fb_gFlushPal (void)
{
	if (ioctl(fd, FBIOPUTCMAP, &colormap))
	{
		perror("fb: ioctl(fb, FBIOGETCMAP, &colormap)");
	}
}

static int fb_SetGraphMode (int high)
{
/*
	struct fb_fix_screeninfo fix2;
*/

#ifdef DEBUG_FRAMEBUFFER
	if (high==-1)
		fprintf(stderr, "fb: set normale mode\n");
	else if (high)
		fprintf(stderr, "fb: set 1024x768\n");
	else
		fprintf(stderr, "fb: set 640x480\n");
#endif
	if (high==-1)
	{
		show_cursor ();
		Console.VidMem = 0;
		ioctl(fd, FBIOPUT_VSCREENINFO, &orgmode);
		return 0;
	}

	hide_cursor ();

	if (high)
	{
		if (!highres.xres)
			return -1;
		Console.CurrentMode = 101;
		Console.TextWidth = 128;
		Console.TextHeight = 60;
		ioctl(fd, FBIOPUT_VSCREENINFO, &highres);
		Console.GraphBytesPerLine = 1024; /* not good, but my framebuffer bugs too much */
	} else {
		if (!lowres.xres)
			return -1;
		Console.CurrentMode = 100;
		Console.TextWidth = 80;
		Console.TextHeight = 60;
		ioctl(fd, FBIOPUT_VSCREENINFO, &lowres);
		Console.GraphBytesPerLine = 640; /* not good, but my framebuffer bugs too much */
	}

	Console.VidMem = fbmem;
	bzero (fbmem, fix.smem_len);

	colormap.start=0;
	colormap.len=256;
	colormap.red=red;
	colormap.green=green;
	colormap.blue=blue;
	/* Since some framebuffer-drivers fails give out the current palette with the bits scaled correct, we can't really
	 * use the FBIOGETCMAP. That sucks!

	if (ioctl(fd, FBIOGETCMAP, &colormap))
		perror("fb: ioctl(fb, FBIOGETCMAP, &colormap)");

	 * And tridentfb atleast is broken when reporting fix.line_length... hehe.. FUCK that driver!!!!

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix2))
		perror("fb: ioctl(1, FBIOGET_FSCREENINFO, &fix)");
	fix2.line_length=fix.line_length;
	fprintf(stderr, "DEBUG LINES: current %d, org %d, new %d\n", Console.GraphBytesPerLine, fix.line_length, fix2.line_length);
	Console.GraphBytesPerLine = fix2.line_length;
	 */
	return 0;
}

int fb_init (int minor, struct consoleDriver_t *driver)
{
	struct fb_var_screeninfo var2;
	char *temp;

	bzero (&lowres, sizeof(lowres));
	bzero (&lowres, sizeof(highres));

	if ((temp=getenv("FRAMEBUFFER")))
	{
		if ((fd=open(temp, O_RDWR))<0)
		{
			perror("fb: open($FRAMEBUFFER)");
			return -1;
		}
	} else {
		if ((fd=open("/dev/fb", O_RDWR))<0)
		{
			perror("fb: open(/dev/fb)");
			if ((fd=open("/dev/fb/0", O_RDWR))<0)
			{
				perror("fb: open(/dev/fb/0)");
				return -1;
			}
		}
	}
	if (get_static_info())
	{
		close(fd);
		fd=-1;
		return -1;
	}
	Console.GraphBytesPerLine = fix.line_length;
#ifdef VERBOSE_FRAMEBUFFER
	fprintf(stderr, "fb: FIX SCREEN INFO\n");
	fprintf(stderr, "fb:  id=%s\n", fix.id);
	fprintf(stderr, "fb:  smem_start=0x%08lx\n", fix.smem_start);
	fprintf(stderr, "fb:  smem_len=0x%08x\n", fix.smem_len);
	fprintf(stderr, "fb:  stype=");
	switch (fix.type)
	{
		case FB_TYPE_PACKED_PIXELS:
			fprintf(stderr, "Packed Pixels\n");
			break;
		case FB_TYPE_PLANES:
			fprintf(stderr, "Non interleaved planes\n");
			break;
		case FB_TYPE_INTERLEAVED_PLANES:
			fprintf(stderr, "Interleaved planes\n");
			break;
		case FB_TYPE_TEXT:
			fprintf(stderr, "Text/attributes\nfb:  type_aux=");
			switch (fix.type_aux)
			{
				case FB_AUX_TEXT_MDA:
					fprintf(stderr, "Monochrome text\n");
					break;
				case FB_AUX_TEXT_CGA:
					fprintf(stderr, "CGA/EGA/VGA Color text\n");
					break;
				case FB_AUX_TEXT_S3_MMIO:
					fprintf(stderr, "S3 MMIO fasttext\n");
					break;
				case FB_AUX_TEXT_MGA_STEP16:
					fprintf(stderr, "MGA Millennium I: text, attr, 14 reserved bytes\n");
					break;
				case FB_AUX_TEXT_MGA_STEP8:
					fprintf(stderr, "other MGAs:      text, attr,  6 reserved bytes\n");
					break;
				default:
					fprintf(stderr, "Unknown\n");
			}
			break;
		case FB_TYPE_VGA_PLANES:
			fprintf(stderr, "EGA/VGA planes\nfb:   type_aux=");
			switch (fix.type_aux)
			{
				case FB_AUX_VGA_PLANES_VGA4:
					fprintf(stderr, "16 color planes (EGA/VGA)\n");
					break;
				case FB_AUX_VGA_PLANES_CFB4:
					fprintf(stderr, "CFB4 in planes (VGA)\n");
					break;
				case FB_AUX_VGA_PLANES_CFB8:
					fprintf(stderr, "CFB8 in planes (VGA)\n");
					break;
				default:
					fprintf(stderr, "Unknown\n");
			}
			break;
		default:
			fprintf(stderr, "Unknown\n");
			break;
	}
	fprintf(stderr, "fb:   visual=");
	switch (fix.visual)
	{
		case FB_VISUAL_MONO01:
			fprintf(stderr, "Monochr. 1=Black 0=White\n");
			break;
		case FB_VISUAL_MONO10:
			fprintf(stderr, "Monochr. 1=White 0=Black\n");
			break;
		case FB_VISUAL_TRUECOLOR:
			fprintf(stderr, "True color\n");
			break;
		case FB_VISUAL_PSEUDOCOLOR:
			fprintf(stderr, "Pseudo color (like atari)\n");
			break;
		case FB_VISUAL_DIRECTCOLOR:
			fprintf(stderr, "Direct color\n");
			break;
		case FB_VISUAL_STATIC_PSEUDOCOLOR:
			fprintf(stderr, "Pseudo color readonly\n");
			break;
		default:
			fprintf(stderr, "Unknown\n");
	}
	fprintf(stderr, "fb:  xpanstep=");
	if (fix.xpanstep)
		fprintf(stderr, "%d\n", fix.xpanstep);
	else
		fprintf(stderr, "Not supported\n");
	fprintf(stderr, "fb:  ypanstep=");
	if (fix.ypanstep)
		fprintf(stderr, "%d\n", fix.ypanstep);
	else
		fprintf(stderr, "Not supported\n");
	fprintf(stderr, "fb:  ywrapstep=");
	if (fix.ywrapstep)
		fprintf(stderr, "%d\n", fix.ywrapstep);
	else
		fprintf(stderr, "Not supported\n");
	fprintf(stderr, "fb:  line_length=%d\n", fix.line_length);
	fprintf(stderr, "fb:  mmio_start=0x%08lx\n", fix.mmio_start);
	fprintf(stderr, "fb:  mmio_len=0x%08x\n", fix.mmio_len);
	fprintf(stderr, "fb:  accel=%d\n", fix.accel);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	fprintf(stderr, "fb:  capabilities=0x%04x\n", fix.capabilities);
	fprintf(stderr, "fb:  reserved0=0x%04x reserved1=0x%04x\n", fix.reserved[0], fix.reserved[1]);
#else
	fprintf(stderr, "fb:  reserved0=0x%04x reserved1=0x%04x reserved2=0x%04x\n", fix.reserved[0], fix.reserved[1], fix.reserved[2]);
#endif
#endif
	if (ioctl(fd, FBIOGET_VSCREENINFO, &orgmode))
	{
		perror("fb: ioctl(1, FBIOGET_VSCREENINFO, &orgmode)");
		close(fd);
		fd=-1;
		return -1;
	}
	orgmode.activate=FB_ACTIVATE_NOW;
#if VERBOSE_FRAMEBUFFER
	fprintf(stderr, "VAR SCREEN INFO\n");
	fprintf(stderr, "xres=%d\n", orgmode.xres);
	fprintf(stderr, "yres=%d\n", orgmode.yres);
	fprintf(stderr, "xres_virtual=%d\n", orgmode.xres_virtual);
	fprintf(stderr, "yres_virtual=%d\n", orgmode.yres_virtual);
	fprintf(stderr, "xoffset=%d\n", orgmode.xoffset);
	fprintf(stderr, "yoffsett=%d\n", orgmode.yoffset);
	fprintf(stderr, "bits_per_pixel=%d\n", orgmode.bits_per_pixel);
	fprintf(stderr, "grayscale=%d\n", orgmode.grayscale);
	/* R, G, B, Alpha goes here */
	fprintf(stderr, "nonstd=%d\n", orgmode.nonstd);
	fprintf(stderr, "(activate=%d)\n", orgmode.activate);
	/* height / width goes here */
	/* accel flags goes here */
#endif
	var2.xres=var2.xres_virtual=640;
	var2.yres=var2.yres_virtual=480;
	var2.xoffset=var2.yoffset=0;
	var2.bits_per_pixel=8;
	var2.grayscale=0;
	var2.nonstd=0;
	var2.height=orgmode.height;
	var2.width=orgmode.width;
	var2.accel_flags=0;
	var2.pixclock=32052;
	var2.left_margin=128;
	var2.right_margin=24;
	var2.upper_margin=28;
	var2.lower_margin=9;
	var2.hsync_len=40;
	var2.vsync_len=3;
	var2.sync=orgmode.sync;
	var2.vmode=0;
	if (test_mode(&var2))
	{
		memcpy(&var2, &orgmode, sizeof(orgmode));
		var2.activate=FB_ACTIVATE_TEST;
	} else {
		var2.activate=FB_ACTIVATE_NOW;
	}
	if ((var2.xres==640)&&(var2.yres==480))
	{
		fprintf(stderr, "fb:  640x480 is available\n");
		memcpy(&lowres, &var2, sizeof(var2));
	} else
		fprintf(stderr, "fb:  640x480 is not available\n");

	var2.xres=var2.xres_virtual=1024;
	var2.yres=var2.yres_virtual=768;
	var2.xoffset=var2.yoffset=0;
	var2.bits_per_pixel=8;
	var2.grayscale=0;
	var2.nonstd=0;
	var2.height=orgmode.height;
	var2.width=orgmode.width;
	var2.accel_flags=0;
	var2.pixclock=15385;
	var2.left_margin=160;
	var2.right_margin=24;
	var2.upper_margin=29;
	var2.lower_margin=3;
	var2.hsync_len=136;
	var2.vsync_len=6;
	var2.sync=orgmode.sync;
	var2.vmode=0;
	if (test_mode(&var2))
	{
		memcpy(&var2, &orgmode, sizeof(orgmode));
		var2.activate=FB_ACTIVATE_TEST;
	} else {
		var2.activate=FB_ACTIVATE_NOW;
	}
	if ((var2.xres==1024)&&(var2.yres==768))
	{
		fprintf(stderr, "fb:  1024x768 is available\n");
		memcpy(&highres, &var2, sizeof(var2));
	} else
		fprintf(stderr, "fb:  1024x768 is not available\n");
	if ((!highres.xres)&&(!lowres.xres))
	{
		close(fd);
		fd=-1;
		return -1;
	}

	if ((fbmem=mmap(0, fix.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0))==MAP_FAILED)
	{
		perror("fb: mmap()");
		close(fd);
		fd = -1;
		return -1;
	}

	driver->SetGraphMode = fb_SetGraphMode;
	driver->gDrawChar16  = generic_gdrawchar;
	driver->gDrawChar16P = generic_gdrawcharp;
	driver->gDrawChar8   = generic_gdrawchar8;
	driver->gDrawChar8P  = generic_gdrawchar8p;
	driver->gDrawStr     = generic_gdrawstr;
	driver->gUpdateStr   = generic_gupdatestr;
	driver->gUpdatePal   = fb_gUpdatePal;
	driver->gFlushPal    = fb_gFlushPal;

	Console.VidType = vidVESA;

	return 0;
}

void fb_done (void)
{
	show_cursor();
	munmap(fbmem, fix.smem_len);
	if (fd < 0)
	{
		return;
	}
	ioctl(fd, FBIOPUT_VSCREENINFO, &orgmode);
	close(fd);
	fd = -1;
}
