/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Variables that are needed globally, and even by the very basic libs.
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
 */

#define _CONSOLE_DRIVER 1
#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "console.h"
#include "cpiface/gif.h"
#include "cpiface/jpeg.h"
#include "cpiface/png.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"
#include "stuff/poutput-keyboard.h"
#include "stuff/utf-8.h"

static void dummySetTextMode (uint8_t x)
{
	fprintf (stderr, "SetTextMode() called premature\n");
}

static void dummyDisplaySetupTextMode (void)
{
	fprintf (stderr, "DisplaySetupTextMode() called premature\n");
}

static const char *dummyGetDisplayTextModeName (void)
{
	fprintf (stderr, "GetDisplayTextModeName() called premature\n");
	return "";
}
static int dummyMeasureStr_utf8 (const char *src, int srclen)
{
	fprintf (stderr, "MeasureStr_utf8() called premature\n");
	return 0;
}

static void dummyDisplayStr_utf8 (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	fprintf (stderr, "DisplayStr_utf8() called premature\n");
}

static void dummyDisplayChr (uint16_t y, uint16_t x, uint8_t attr, char chr, uint16_t len)
{
	fprintf (stderr, "dummyDisplayChr() called premature\n");
}

static void dummyDisplayStr (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	fprintf (stderr, "DisplayStr() called premature\n");
}

static void dummyDisplayStrAttr (uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	fprintf (stderr, "DisplayStrAttr() called premature\n");
}

static void dummyDisplayVoid (uint16_t y, uint16_t x, uint16_t len)
{
	fprintf (stderr, "DisplayVoid() called premature\n");
}

static void dummyDrawBar (uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	fprintf (stderr, "DrawBar() called premature\n");
}

static void dummyiDrawBar (uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	fprintf (stderr, "iDrawBar() called premature\n");
}

static int dummyHasKey (uint16_t key)
{
	fprintf (stderr, "HasKey() called premature\n");
	return 0;
}

static void dummySetCursorPosition (uint16_t y, uint16_t x)
{
	fprintf (stderr, "SetCursorPosition() called premature\n");
}

static void dummySetCursorShape (uint16_t shape)
{
	fprintf (stderr, "SetCursorShape() called premature\n");
}

static int dummyconsoleRestore (void)
{
	fprintf (stderr, "consoleRestore() called premature\n");
	return 0;
}

static void dummyconsoleSave (void)
{
	fprintf (stderr, "consoleSave() called premature\n");
}

static void dummyDosShell (void)
{
	fprintf (stderr, "dummyDosShell() called premature\n");
}

const struct consoleDriver_t dummyConsoleDriver =
{
	0, /* vga13 */
	dummySetTextMode,
	dummyDisplaySetupTextMode,
	dummyGetDisplayTextModeName,
	dummyMeasureStr_utf8,
	dummyDisplayStr_utf8,
	dummyDisplayChr,
	dummyDisplayStr,
	dummyDisplayStrAttr,
	dummyDisplayVoid,
	dummyDrawBar,
	dummyiDrawBar,
	0, /* TextOverlayAddBGRA */
	0, /* TextOverlayRemove */
	0, /* SetGraphMode */
	0, /* gDrawChar */
	0, /* gDrawCharP */
	0, /* gDrawChar8 */
	0, /* gDrawChar8P */
	0, /* gDrawStr */
	0, /* gUpdateStr */
	0, /* gUpdatePal */
	0, /* gFlushPal */
	dummyHasKey,
	dummySetCursorPosition,
	dummySetCursorShape,
	dummyconsoleRestore,
	dummyconsoleSave,
	dummyDosShell
};

unsigned int plScrHeight = 80;
unsigned int plScrWidth = 25;
enum vidType plVidType;
unsigned char plScrType;
int plScrMode;
uint8_t *plVidMem;

int plScrTextGUIOverlay;

static void display_nprintf (uint16_t y, uint16_t x, uint8_t color, uint16_t width, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	while ((*fmt) && width)
	{
		int spaceflag = 0;
		int minusflag = 0;
		int plusflag = 0;
		int zeroflag = 0;
		int hashflag = 0;
		int requested_width = INT_MAX;
		int requested_precision = INT_MAX;
		unsigned long long llu;
		signed long long lls;
		unsigned char c;

		if (fmt[0] != '%')
		{
			const char *next = strchr (fmt, '%');
			int len;
			if (!next)
			{
				next = fmt + strlen (fmt);
			}
			len = next - fmt;
			if (len > width)
			{
				len = width;
			}
			Console.Driver->DisplayStr (y, x, color, fmt, len);
			x += len;
			width -= len;
			fmt = next;
			continue;
		}
		fmt++;
		if ((*fmt) == '%')
		{
			Console.Driver->DisplayChr (y, x, color, '%', 1);
			x++;
			width--;
			fmt++;
			continue;
		}
before_dot:
		if (!*fmt)
		{
			break;
		}
		switch (*fmt)
		{
			case '-':
				minusflag = 1;
				fmt++;
				goto before_dot;
			case '+':
				plusflag = 1;
				fmt++;
				goto before_dot;
			case ' ':
				spaceflag = 1;
				fmt++;
				goto before_dot;
			case '#':
				hashflag = 1;
				fmt++;
				goto before_dot;
			case '*':
				requested_width = va_arg(ap, int);
				fmt++;
				goto before_dot;
			case '.':
				fmt++;
				goto after_dot;
			case '0':
				zeroflag = 1;
				fmt++;
				goto before_dot;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				requested_width = strtol (fmt, (char **)&fmt, 10);
				goto before_dot;
			default:
				break; /* fall into the after dot loop */
		}
after_dot:
		if (!*fmt)
		{
			break;
		}
		switch (*fmt)
		{
			default:
				va_end(ap);
				return; /* FAILURE */
			case '*':
				requested_precision = va_arg(ap, int);
				fmt++;
				goto after_dot;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				requested_precision = strtol (fmt, (char **)&fmt, 10);
				goto after_dot;
			case 'C':
				if (!fmt[1])
				{
					va_end (ap);
					return;
				}
				fmt++;
				c = *fmt;
				goto got_c; /* got_c will to the last fmt++ */
			case 'c':
				{
					c = (unsigned char)va_arg (ap, int);
				got_c:
					if (requested_width == INT_MAX)
					{
						requested_width = 1;
					}
					if (requested_precision==INT_MAX)
					{
						requested_precision = requested_width;
					}
					if (requested_precision > width)
					{
						requested_precision = width;
					}
					if (requested_width > width)
					{
						requested_width = width;
					}
					if (requested_precision > requested_width)
					{
						requested_precision = requested_width;
					}
					if ((requested_width > requested_precision) && minusflag)
					{
						Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
						x += requested_width - requested_precision;
					}
					Console.Driver->DisplayChr (y, x, color, c, requested_precision);
					x += requested_precision;
					if ((requested_width > requested_precision) && !minusflag)
					{
						Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
						x += requested_width - requested_precision;
					}
					width -= requested_width;
					fmt++;
					break;
				}
			case 'o':
				if (requested_width == INT_MAX)
				{
					if (zeroflag)
					{
						color &= 0x0f;
					}
				} else {
					color &= 0x0f;
					color |= requested_width << 4;
				}
				if (requested_precision != INT_MAX)
				{
					color &= 0xf0;
					color |= (requested_precision & 15);
				}
				fmt++;
				break;
			case 's':
				{
					const char *src = va_arg (ap, const char *);

					if (requested_precision==INT_MAX)
					{
						requested_precision = strlen (src);
					}
					if (requested_width==INT_MAX)
					{
						requested_width = requested_precision;
					}
					if (requested_precision > requested_width)
					{
						requested_precision = requested_width;
					}
					if (requested_precision > strnlen (src, requested_precision))
					{
						requested_precision = strnlen (src, requested_precision);
					}
					if ((requested_width > requested_precision) && minusflag)
					{
						Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
						x += requested_width - requested_precision;
					}
					Console.Driver->DisplayStr (y, x, color, src, requested_precision);
					x += requested_precision;
					if ((requested_width > requested_precision) && !minusflag)
					{
						Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
						x += requested_width - requested_precision;
					}
					width -= requested_width;
					fmt++;
					break;
				}
			case 'S':
				{
					const char *src = va_arg (ap, const char *);
					int src_width = Console.Driver->MeasureStr_utf8 (src, strlen (src));
					if (requested_precision==INT_MAX)
					{
						requested_precision = src_width;
					}
					if (requested_width==INT_MAX)
					{
						requested_width = requested_precision;
					}
					if (requested_precision > requested_width)
					{
						requested_precision = requested_width;
					}
					if (requested_precision > src_width)
					{
						requested_precision = src_width;
					}
					if ((requested_width > requested_precision) && minusflag)
					{
						Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
						x += requested_width - requested_precision;
					}
					if (spaceflag)
					{
						Console.Driver->DisplayStr_utf8 (y, x, color, src, requested_precision);
					} else {
						displaystr_utf8_overflowleft (y, x, color, src, requested_precision);
					}
					x += requested_precision;
					if ((requested_width > requested_precision) && !minusflag)
					{
						Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
						x += requested_width - requested_precision;
					}
					width -= requested_width;
					fmt++;
					break;
				}
			case 'd':
				lls = va_arg (ap, signed int);
				fmt++;
				goto got_lls;
			case 'u':
				llu = va_arg (ap, unsigned int);
				fmt++;
				goto got_llu;
			case 'x':
				llu = va_arg (ap, unsigned int);
				fmt++;
				goto got_llx;
			case 'X':
				llu = va_arg (ap, unsigned int);
				fmt++;
				goto got_llX;
			case 'l':
				switch (fmt[1])
				{
					default:
						va_end(ap);
						return; /* FAILURE */
					case 'd':
						lls = va_arg (ap, signed long int);
						fmt+=2;
						goto got_lls;
					case 'u':
						llu = va_arg (ap, unsigned long int);
						fmt+=2;
						goto got_llu;
					case 'x':
						llu = va_arg (ap, unsigned long int);
						fmt+=2;
						goto got_llx;
					case 'X':
						llu = va_arg (ap, unsigned long int);
						fmt+=2;
						goto got_llX;
					case 'l':
					{
						int src_width;
						char buffer[21];
						switch (fmt[2])
						{
							default:
								va_end(ap);
								return; /* FAILURE */
							case 'd':
								lls = va_arg (ap, signed long long int);
								fmt+=3;
						got_lls:
								if (spaceflag)
								{
									snprintf (buffer, sizeof (buffer), "% lld", lls);
								} else if (plusflag)
								{
									snprintf (buffer, sizeof (buffer), "%+lld", lls);
								} else {
									snprintf (buffer, sizeof (buffer), "%lld", lls);
								}
						ready_lls:
								src_width = strlen(buffer);

								if (requested_precision==INT_MAX)
								{
									requested_precision = src_width;
								}
								if (requested_width==INT_MAX)
								{
									requested_width = requested_precision;
								}
								if (requested_precision > requested_width)
								{
									requested_precision = requested_width;
								}
								if (requested_precision > src_width)
								{
									requested_precision = src_width;
								}
								if (src_width > requested_precision)
								{
									int i;
									for (i=0; buffer[i]; i++)
									{
										if ((buffer[i] != '-') && (buffer[i] != '+') && (buffer[i] != ' '))
										{
											buffer[i] = '9';
										}
									}
									buffer[requested_width]=0;
									requested_precision = requested_width;
									src_width = requested_width;
								}
								while ((requested_width > requested_precision) && (requested_width <= 20) && zeroflag)
								{
									if ((buffer[0] == '-') || (buffer[0] == '+'))
									{
										memmove (buffer + 2, buffer + 1, strlen (buffer + 1) + 1);
										buffer[1] = '0';
									} else {
										memmove (buffer + 1, buffer, strlen (buffer) + 1);
										buffer[0] = '0';
									}
								}
								if ((requested_width > requested_precision) && minusflag)
								{
									Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
									x += requested_width - requested_precision;
								}
								Console.Driver->DisplayStr (y, x, color, buffer, requested_precision);
								x += requested_precision;
								if ((requested_width > requested_precision) && !minusflag)
								{
									Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
									x += requested_width - requested_precision;
								}
								width -= requested_width;
								break;
							case 'u':
								llu = va_arg (ap, unsigned long long int);
								fmt+=3;
						got_llu:
								snprintf (buffer, sizeof (buffer), "%llu", llu);
								goto ready_lls;
							case 'x':
								llu = va_arg (ap, unsigned long long int);
								fmt+=3;
						got_llx:
								if (hashflag)
								{
									snprintf (buffer, sizeof (buffer), "%#llx", llu);
								} else {
									snprintf (buffer, sizeof (buffer), "%llx", llu);
								}
						ready_llx:
								src_width = strlen(buffer);
								if (requested_precision==INT_MAX)
								{
									requested_precision = src_width;
								}
								if (requested_width==INT_MAX)
								{
									requested_width = requested_precision;
								}
								if (requested_precision > requested_width)
								{
									requested_precision = requested_width;
								}
								if (requested_precision > src_width)
								{
									requested_precision = src_width;
								}
								if (src_width > requested_precision)
								{
									int i;
									for (i=0; buffer[i]; i++)
									{
										if ((buffer[i] != '-') && (buffer[i] != '+') && (buffer[i] != ' '))
										{
											buffer[i] = '9';
										}
									}
									buffer[requested_width]=0;
									requested_precision = requested_width;
									src_width = requested_width;
								}
								while ((requested_width > requested_precision) && (requested_width <= 20) && zeroflag)
								{
									if ((buffer[0] != '0') && ((buffer[1] != 'x') || (buffer[1] != 'X')))
									{
										memmove (buffer + 3, buffer + 2, strlen (buffer + 2) + 1);
										buffer[2] = '0';
									} else {
										memmove (buffer + 1, buffer, strlen (buffer) + 1);
										buffer[0] = '0';
									}
								}
								if ((requested_width > requested_precision) && minusflag)
								{
									Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
									x += requested_width - requested_precision;
								}
								Console.Driver->DisplayStr (y, x, color, buffer, requested_precision);
								x += requested_precision;
								if ((requested_width > requested_precision) && !minusflag)
								{
									Console.Driver->DisplayChr (y, x, color, ' ', requested_width - requested_precision);
									x += requested_width - requested_precision;
								}
								width -= requested_width;
								break;

							case 'X':
								llu = va_arg (ap, unsigned long long int);
								fmt+=3;
							got_llX:
								if (hashflag)
								{
									snprintf (buffer, sizeof (buffer), "%#llX", llu);
								} else {
									snprintf (buffer, sizeof (buffer), "%llX", llu);
								}
								goto ready_llx;
						} // switch (fmt[2])
					} // case 'l':
					break;
				} // switch (fmt[1])
			// case 'l':
			break;
		} // switch (fmt[0])
	} // while
	if (width)
	{
		Console.Driver->DisplayChr (y, x, color, ' ', width);
	}
	va_end(ap);
}

static void writenum (uint16_t *buf, uint16_t ofs, uint8_t attr, unsigned long num, uint8_t radix, uint16_t len, int clip0)
{
	char convbuf[20];
	uint16_t *p=buf+ofs;
	char *cp=convbuf+len;
	int i;
	for (i=0; i<len; i++)
	{
		*--cp="0123456789ABCDEF"[num%radix];
		num/=radix;
	}
	for (i=0; i<len; i++)
	{
		if (clip0&&(convbuf[i]=='0')&&(i!=(len-1)))
		{
			*p++=' '|(attr<<8);
			cp++;
		} else {
			*p++=(*cp++)|(attr<<8);
			clip0=0;
		}
	}
}

static void writestring (uint16_t *buf, uint16_t ofs, uint8_t attr, const char *str, uint16_t len)
{
	uint16_t *p=buf+ofs;
	int i;
	for (i=0; i<len; i++)
	{
		*p++=(*((unsigned char *)(str)))|(attr<<8);
		if (*str)
			str++;
	}
}

static void writestringattr (uint16_t *buf, uint16_t ofs, const uint16_t *str, uint16_t len)
{
	memcpy (buf+ofs, (void *)str, len*2);
}

struct console_t Console =
{
	&dummyConsoleDriver,
	display_nprintf,
	writenum,
	writestring,
	writestringattr,
	ekbhit,
	egetch,
	framelock,
#ifdef HAVE_LZW
	GIF87_try_open_bgra,
#else
	0,
#endif
	try_open_jpeg,
	try_open_png,
	80,      /* TextHeight */
	25,      /* TextWidth */
	0,       /* TextGUIOverlay */
	vidNorm, /* VidType */
	0,       /* LastTextMode */
	0,       /* CurrentMode */
	0,       /* VidMem */
	0,       /* GraphBytesPerLine */
	0,       /* GraphLines */
	_8x16    /* CurrentFont */
};
