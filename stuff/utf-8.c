/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelestad@gmail.com>
 *
 * UTF-8 encode/decode functions
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
 */

#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "framelock.h"
#include "poutput.h"
#include "utf-8.h"

//#define UNKNOWN_UNICODE 0xFFFD
uint32_t utf8_decode (const char *_src, size_t srclen, int *inc)
{
	const unsigned char *src = (const unsigned char *)_src;
	int left;
	uint32_t retval;

	if (!srclen)
	{
		*inc = 0;
		return 0; // UNKNOWN_UNICODE;
	}

	*inc = 1;
	if ((src[0] & 0x80) == 0x00)
	{ /* 0xxxxxxx  - ASCII - quick exit */
		return src[0];
	}

	if ((src[0] & 0xfe) == 0xfc)
	{ /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx - 6 byte sequence are not longer allowed in UTF-8 */
		left = 5;
		retval = src[0] & 0x01;
	} else if ((src[0] & 0xfc) == 0xf8)
	{ /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx - 5 byte sequence are not longer allowed in UTF-8 */
		left = 4;
		retval = src[0] & 0x03;
	} else if ((src[0] & 0xf8) == 0xf0)
	{ /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx - 4 byte sequence */
		left = 3;
		retval = src[0] & 0x07;
	} else if ((src[0] & 0xf0) == 0xe0)
	{ /* 1110xxxx 10xxxxxx 10xxxxxx - 3 byte sequence */
		left = 2;
		retval = src[0] & 0x0f;
	} else if ((src[0] & 0xe0) == 0xc0)
	{ /* 110xxxxx 10xxxxxx - 2 byte sequence */
		left = 1;
		retval = src[0] & 0x1f;
	} else if ((src[0] & 0xc0) == 0x80)
	{ /* 10xxxxxx - 1 byte sequnce is not allowed */
		return src[0] & 0x3f;
	} else {
	 /* 11111111 11111110 - invalid UTF-8 code points */
		return src[0];
	}

	--srclen;
	while (left && srclen)
	{
		src++;
		if ((src[0] & 0xC0) != 0x80)
		{
			return retval; //UNKNOWN_UNICODE;
		}
		retval <<= 6;
		retval |= (src[0] & 0x3F);
		srclen--;
		left--;
		(*inc)++;
	}

	return retval;
}

int utf8_encoded_length (uint32_t codepoint)
{
	if (codepoint == 0)
	{
		return 0;
	}
	if (codepoint < 0x7f)
	{
		return 1;
	}
	if (codepoint <= 0x7ff)
	{
		return 2;
	}
	if (codepoint <= 0xffff)
	{
		return 3;
	}
	if (codepoint <= 0x1fffff)
	{
		return 4;
	}
	if (codepoint <= 0x3ffffff)
	{ /* non-standard */
		return 5;
	}

	if (codepoint <= 0x7fffffff)
	{ /* non-standard */
		return 6;
	}

	/* 7 bytes has never been used */
	return 0;
}


int utf8_encode (char *dst, uint32_t codepoint)
{
	if (codepoint == 0)
	{
		dst[0] = 0;
		return 0;
	}

	if (codepoint <= 0x7f)
	{
		dst[0] = codepoint;
		dst[1] = 0;
		return 1;
	}

	if (codepoint <= 0x7ff)
	{
		dst[0] = 0xC0 | (codepoint >> 6);
		dst[1] = 0x80 | (codepoint & 0x3f);
		dst[2] = 0;
		return 2;
	}

	if (codepoint <= 0xffff)
	{
		dst[0] = 0xe0 |  (codepoint >> 12);
		dst[1] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[2] = 0x80 |  (codepoint        & 0x3f);
		dst[3] = 0;
		return 3;
	}

	if (codepoint <= 0x1fffff)
	{
		dst[0] = 0xf0 |  (codepoint >> 18);
		dst[1] = 0x80 | ((codepoint >> 12) & 0x3f);
		dst[2] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[3] = 0x80 |  (codepoint        & 0x3f);
		dst[4] = 0;
		return 4;
	}

	if (codepoint <= 0x3ffffff)
	{ /* non-standard */
		dst[0] = 0xf8 |  (codepoint >> 24);
		dst[1] = 0x80 | ((codepoint >> 18) & 0x3f);
		dst[2] = 0x80 | ((codepoint >> 12) & 0x3f);
		dst[3] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[4] = 0x80 |  (codepoint        & 0x3f);
		dst[5] = 0;
		return 5;
	}

	if (codepoint <= 0x7fffffff)
	{ /* non-standard */
		dst[0] = 0xfc |  (codepoint >> 30);
		dst[1] = 0x80 | ((codepoint >> 24) & 0x3f);
		dst[2] = 0x80 | ((codepoint >> 18) & 0x3f);
		dst[3] = 0x80 | ((codepoint >> 12) & 0x3f);
		dst[4] = 0x80 | ((codepoint >>  6) & 0x3f);
		dst[5] = 0x80 |  (codepoint        & 0x3f);
		dst[6] = 0;
		return 6;
	}

	/* 7 bytes has never been used */
	dst[0] = 0;
	return 0;
}

#ifndef BOOT
void displaystr_utf8_overflowleft (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	const char *tmppos = str;
	int tmpposlen = strlen (str);
	int visuallen = measurestr_utf8 (tmppos, tmpposlen);

	while (visuallen > len)
	{
		int inc = 0;
		utf8_decode (tmppos, tmpposlen, &inc);
		tmppos += inc;
		tmpposlen -= inc;
		visuallen = measurestr_utf8 (tmppos, tmpposlen);
	}
	displaystr_utf8(y, x, attr, tmppos, len);
}

struct VisualCharacter_t
{
	uint32_t codepoint;
	uint8_t visualwidth;
	uint8_t data_length; /* Only used by EditStringUTF8z */
};

/* fixed buffer, zero-terminated */
int EditStringUTF8z (unsigned int y, unsigned int x, unsigned int w, int l, char *s)
{
/* problems:
   each UTF-8 noun might be 1-4 bytes per character   <- use utf8_decode in a loop to cycle positions
   each noun can take 0,1 or 2 cells on the screen    <- use measurestr_utf8
 */
	static struct VisualCharacter_t *workstring_data = 0;
	static int                       workstring_length = 0;
	static int                       workstring_size = 0;

	static char *visualstring_buffer, *ptr;
	static int   state = 0;
	static int   data_length;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */
	static int insmode;
	static int curpos;
	static unsigned int scrolled = 0;

	static uint8_t input_buffer[8];
	static int input_buffer_fill;

	unsigned int visual_length_before_scrolled = 0;
	unsigned int visual_length_before_curpos = 0;
	unsigned int visual_length_at_curpos = 1;
	unsigned int visual_length_after_curpos = 0;
	int i;

	if (state == 0)
	{
		unsigned int left = strlen (s);
		int incr = 0;

		data_length = 0;
		workstring_size = l;
		visualstring_buffer = malloc (l);
		workstring_data = malloc (workstring_size * sizeof (workstring_data[0]));

		for (i = 0, ptr = s; *ptr; i++, ptr += incr)
		{
			workstring_data[i].codepoint = utf8_decode (ptr, left, &incr);
			workstring_data[i].visualwidth = measurestr_utf8 (ptr, incr);
			workstring_data[i].data_length = utf8_encoded_length (workstring_data[i].codepoint);
			data_length += workstring_data[i].data_length;
		}
		curpos = workstring_length = i;

		setcurshape (1);
		insmode = 1;
		state = 1;
		scrolled = 0;
		input_buffer_fill = 0;
	}

	if (scrolled > curpos)
	{
		scrolled = curpos;
	}

	for (i=0; i < workstring_length; i++)
	{
		if (i < scrolled)
		{
			visual_length_before_scrolled += workstring_data[i].visualwidth;
		} else if (i < curpos)
		{
			visual_length_before_curpos   += workstring_data[i].visualwidth;
		} else if (i == curpos)
		{
			visual_length_at_curpos        = workstring_data[i].visualwidth;
		} else {
			visual_length_after_curpos    += workstring_data[i].visualwidth;
		}
	}

	while (   (visual_length_before_curpos + visual_length_at_curpos     > w) ||                                /* if cursor is outside the screen */
	        ( (visual_length_before_curpos + visual_length_at_curpos + 4 > w) && visual_length_after_curpos ) ) /* or cursor is pressed to the very last 4 visible cells on the display */
	{ /* hide more text */
		visual_length_before_scrolled += workstring_data[scrolled].visualwidth;
		visual_length_before_curpos -= workstring_data[scrolled].visualwidth;
		scrolled++;
	}

	while (scrolled && ( (visual_length_before_curpos < 4) ||                                                                                                      /* if cursor is pressed to the very first 4 visible cells on the display */
	                     ((visual_length_before_curpos + visual_length_at_curpos + visual_length_after_curpos + workstring_data[scrolled-1].visualwidth) <= w) ) ) /* or more text can fit on the display */
	{ /* reveal more text */
		scrolled--;
		visual_length_before_scrolled -= workstring_data[scrolled].visualwidth;
		visual_length_before_curpos += workstring_data[scrolled].visualwidth;
	}

	for (i=scrolled, ptr=visualstring_buffer; i < workstring_length; i++)
	{
		ptr += utf8_encode (ptr, workstring_data[i].codepoint);
	}
	*ptr = 0;

	displaystr_utf8 (y, x, 0x8F, visualstring_buffer, w);
	setcur(y, x + visual_length_before_curpos);

	if (state == 2)
	{
		if (cpiKeyHelpDisplay())
		{
			framelock();
			return 1;
		}
		state = 1;
	}
	framelock();

	while (Console.KeyboardHit())
	{
		uint16_t key = Console.KeyboardGetChar ();
		if ((key>=0x20)&&(key<=0xFF))
		{
			uint32_t codepoint;
			int incr = 0;

			/* queue-up UTF-8 characters if needed */
			input_buffer[input_buffer_fill++] = key;
			input_buffer[input_buffer_fill] = 0x80; /* dummy follow... */

			codepoint = utf8_decode ((char *)input_buffer, input_buffer_fill + 1, &incr);

			if (incr > input_buffer_fill)
			{ /* we need more data */
				assert (input_buffer_fill < 6);
				continue;
			}
			input_buffer_fill = 0;

			if ( insmode || ( curpos == workstring_length ) ) /* insert / append */
			{
				/* alloc more space if we need to */
				if (data_length + 2 >= l)
				{
					continue; /* buffer is full..... */
				}

				memmove (workstring_data + curpos + 1, workstring_data + curpos, sizeof (workstring_data[0]) * (workstring_length - curpos));
				workstring_data[curpos].codepoint = codepoint;
				workstring_data[curpos].visualwidth = measurestr_utf8 ((char *)input_buffer, incr);
				workstring_data[curpos].data_length = utf8_encoded_length (workstring_data[curpos].codepoint);
				data_length += workstring_data[curpos].data_length;
				curpos++;
				workstring_length++;
			} else { /* overwrite */
				struct VisualCharacter_t t;

				t.codepoint = codepoint;
				t.visualwidth = measurestr_utf8 ((char *)input_buffer, incr);
				t.data_length = utf8_encoded_length (t.codepoint);

				if ((t.data_length > workstring_data[curpos].data_length) && (data_length + 1 - workstring_data[curpos].data_length + t.data_length) >= l)
				{
					continue; /* buffer is full..... */
				}
				data_length -= workstring_data[curpos].data_length;
				workstring_data[curpos] = t;
				data_length += workstring_data[curpos].data_length;
				curpos++;
			}
		} else switch (key)
		{
			case KEY_LEFT:
				if (curpos)
					curpos--;
				break;
			case KEY_RIGHT:
				if (curpos < workstring_length)
					curpos++;
				break;
			case KEY_HOME:
				curpos = 0;
				break;
			case KEY_END:
				curpos = workstring_length;
				break;
			case KEY_INSERT:
				insmode = !insmode;
				setcurshape (insmode ? 1:2);
				break;
			case KEY_DELETE:
				if (curpos != workstring_length)
				{
					data_length -= workstring_data[curpos].data_length;
					memmove (workstring_data + curpos, workstring_data + curpos + 1, sizeof (workstring_data[0]) * (workstring_length - curpos - 1 /* 0 */));
					workstring_length--;
				}
				break;
			case KEY_BACKSPACE:
				if (curpos)
				{
					data_length -= workstring_data[curpos-1].data_length;
					memmove (workstring_data + curpos - 1, workstring_data + curpos, sizeof (workstring_data[0]) * (workstring_length - curpos /* + 1 */));
					curpos--;
					workstring_length--;
				}
				break;
			case KEY_EXIT:
			case KEY_ESC:
				setcurshape(0);
				free (workstring_data);     workstring_data = 0;
				free (visualstring_buffer); visualstring_buffer = 0;
				state = 0;
				return -1;
			case _KEY_ENTER:
				setcurshape(0);
				/* visualstring_buffer might be out of sync if we have processed more keys in the same run */
				for (i=0, ptr=s; i < workstring_length; i++)
				{
					ptr += utf8_encode (ptr, workstring_data[i].codepoint);
				}
				*ptr = 0;
				free (workstring_data);     workstring_data = 0;
				free (visualstring_buffer); visualstring_buffer = 0;
				state = 0;
				return 0;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Move cursor right");
				cpiKeyHelp(KEY_LEFT, "Move cursor left");
				cpiKeyHelp(KEY_HOME, "Move cursor home");
				cpiKeyHelp(KEY_END, "Move cursor to the end");
				cpiKeyHelp(KEY_INSERT, "Toggle insert mode");
				cpiKeyHelp(KEY_DELETE, "Remove character at cursor");
				cpiKeyHelp(KEY_BACKSPACE, "Remove character left of cursor");
				cpiKeyHelp(KEY_ESC, "Cancel changes");
				cpiKeyHelp(_KEY_ENTER, "Submit changes");
				state = 2;
				return 1;
		}
	}

	return 1;
}


static int _EditStringUTF8(unsigned int y, unsigned int x, unsigned int w, char **s, int ASCIIonly)
{
/* problems:
   each UTF-8 noun might be 1-4 bytes per character   <- use utf8_decode in a loop to cycle positions
   each noun can take 0,1 or 2 cells on the screen    <- use measurestr_utf8
 */
	static struct VisualCharacter_t *workstring_data = 0;
	static int                       workstring_length = 0;
	static int                       workstring_size = 0;

	static char *visualstring_buffer, *ptr;
	static int   state = 0;
	/* 0 - new / idle
	 * 1 - in edit
	 * 2 - in keyboard help
	 */
	static int insmode;
	static int curpos;
	static unsigned int scrolled = 0;

	static uint8_t input_buffer[8];
	static int input_buffer_fill;

	unsigned int visual_length_before_scrolled = 0;
	unsigned int visual_length_before_curpos = 0;
	unsigned int visual_length_at_curpos = 1;
	unsigned int visual_length_after_curpos = 0;
	int i;

	if (state == 0)
	{
		unsigned int left = strlen (*s);
		int incr = 0;

		workstring_size = (left + 128) % ~63; /* use a worse case scenario string length add some headroom and round it off to 64 byte chunks */
		visualstring_buffer = malloc (workstring_size * 4 + 1);
		workstring_data = malloc (workstring_size * sizeof (workstring_data[0]));

		for (i = 0, ptr = s[0]; *ptr; i++, ptr += incr)
		{
			workstring_data[i].codepoint = utf8_decode (ptr, left, &incr);
			workstring_data[i].visualwidth = measurestr_utf8 (ptr, incr);
		}
		curpos = workstring_length = i;

		setcurshape (1);
		insmode = 1;
		state = 1;
		scrolled = 0;

		input_buffer_fill = 0;
	}

	if (scrolled > curpos)
	{
		scrolled = curpos;
	}

	for (i=0; i < workstring_length; i++)
	{
		if (i < scrolled)
		{
			visual_length_before_scrolled += workstring_data[i].visualwidth;
		} else if (i < curpos)
		{
			visual_length_before_curpos   += workstring_data[i].visualwidth;
		} else if (i == curpos)
		{
			visual_length_at_curpos        = workstring_data[i].visualwidth;
		} else {
			visual_length_after_curpos    += workstring_data[i].visualwidth;
		}
	}

	while (   (visual_length_before_curpos + visual_length_at_curpos     > w) ||                                /* if cursor is outside the screen */
	        ( (visual_length_before_curpos + visual_length_at_curpos + 4 > w) && visual_length_after_curpos ) ) /* or cursor is pressed to the very last 4 visible cells on the display */
	{ /* hide more text */
		visual_length_before_scrolled += workstring_data[scrolled].visualwidth;
		visual_length_before_curpos -= workstring_data[scrolled].visualwidth;
		scrolled++;
	}

	while (scrolled && ( (visual_length_before_curpos < 4) ||                                                                                                      /* if cursor is pressed to the very first 4 visible cells on the display */
	                     ((visual_length_before_curpos + visual_length_at_curpos + visual_length_after_curpos + workstring_data[scrolled-1].visualwidth) <= w) ) ) /* or more text can fit on the display */
	{ /* reveal more text */
		scrolled--;
		visual_length_before_scrolled -= workstring_data[scrolled].visualwidth;
		visual_length_before_curpos += workstring_data[scrolled].visualwidth;
	}

	for (i=scrolled, ptr=visualstring_buffer; i < workstring_length; i++)
	{
		ptr += utf8_encode (ptr, workstring_data[i].codepoint);
	}
	*ptr = 0;

	displaystr_utf8 (y, x, 0x8F, visualstring_buffer, w);
	setcur(y, x + visual_length_before_curpos);

	if (state == 2)
	{
		if (cpiKeyHelpDisplay())
		{
			framelock();
			return 1;
		}
		state = 1;
	}
	framelock();

	while (Console.KeyboardHit())
	{
		uint16_t key = Console.KeyboardGetChar();
		if ((key>=0x20)&&(key<=0xFF))
		{
			uint32_t codepoint;
			int incr = 0;

			/* queue-up UTF-8 characters if needed */
			input_buffer[input_buffer_fill++] = key;
			input_buffer[input_buffer_fill] = 0x80; /* dummy follow... */

			codepoint = utf8_decode ((char *)input_buffer, input_buffer_fill + 1, &incr);
			if (incr > input_buffer_fill)
			{ /* we need more data */
				assert (input_buffer_fill < 6);
				continue;
			}
			input_buffer_fill = 0;
			if (ASCIIonly)
			{
				if (codepoint > 127)
				{
					continue;
				}
			}

			if ( insmode || ( curpos == workstring_length ) ) /* insert / append */
			{
				/* alloc more space if we need to */
				if (workstring_size >= workstring_length)
				{
					unsigned int newsize = workstring_size + 64;
					void *temp;
					temp = realloc (workstring_data, newsize * sizeof (workstring_data[0]));
					if (!temp)
					{
						return 1;
					}
					workstring_data = temp;
					temp = realloc (visualstring_buffer, newsize * 4 + 1);
					if (!temp)
					{
						return 1;
					}
					visualstring_buffer = temp;
					workstring_size = newsize;
				}

				memmove (workstring_data + curpos + 1, workstring_data + curpos, sizeof (workstring_data[0]) * (workstring_length - curpos));
				workstring_data[curpos].codepoint = codepoint;
				workstring_data[curpos].visualwidth = measurestr_utf8 ((char *)input_buffer, incr);
				curpos++;
				workstring_length++;
			} else { /* overwrite */
				workstring_data[curpos].codepoint = codepoint;
				workstring_data[curpos].visualwidth = measurestr_utf8 ((char *)input_buffer, incr);
				curpos++;
			}
		} else switch (key)
		{
			case KEY_LEFT:
				if (curpos)
					curpos--;
				break;
			case KEY_RIGHT:
				if (curpos < workstring_length)
					curpos++;
				break;
			case KEY_HOME:
				curpos = 0;
				break;
			case KEY_END:
				curpos = workstring_length;
				break;
			case KEY_INSERT:
				insmode = !insmode;
				setcurshape (insmode ? 1:2);
				break;
			case KEY_DELETE:
				if (curpos != workstring_length)
				{
					memmove (workstring_data + curpos, workstring_data + curpos + 1, sizeof (workstring_data[0]) * (workstring_length - curpos - 1 /* 0 */));
					workstring_length--;
				}
				break;
			case KEY_BACKSPACE:
				if (curpos)
				{
					memmove (workstring_data + curpos - 1, workstring_data + curpos, sizeof (workstring_data[0]) * (workstring_length - curpos /* + 1 */));
					curpos--;
					workstring_length--;
				}
				break;
			case KEY_EXIT:
			case KEY_ESC:
				setcurshape(0);
				free (workstring_data);     workstring_data = 0;
				free (visualstring_buffer); visualstring_buffer = 0;
				state = 0;
				return -1;
			case _KEY_ENTER:
				setcurshape(0);
				free (*s);
				*s = malloc (workstring_length * 4 + 1);
				if (*s)
				{	/* visualstring_buffer might be out of sync if we have processed more keys in the same run */
					for (i=0, ptr=*s; i < workstring_length; i++)
					{
						ptr += utf8_encode (ptr, workstring_data[i].codepoint);
					}
					*ptr = 0;
				}
				free (workstring_data);     workstring_data = 0;
				free (visualstring_buffer); visualstring_buffer = 0;
				state = 0;
				return 0;
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp(KEY_RIGHT, "Move cursor right");
				cpiKeyHelp(KEY_LEFT, "Move cursor left");
				cpiKeyHelp(KEY_HOME, "Move cursor home");
				cpiKeyHelp(KEY_END, "Move cursor to the end");
				cpiKeyHelp(KEY_INSERT, "Toggle insert mode");
				cpiKeyHelp(KEY_DELETE, "Remove character at cursor");
				cpiKeyHelp(KEY_BACKSPACE, "Remove character left of cursor");
				cpiKeyHelp(KEY_ESC, "Cancel changes");
				cpiKeyHelp(_KEY_ENTER, "Submit changes");
				state = 2;
				return 1;
		}
	}

	return 1;
}

int EditStringUTF8(unsigned int y, unsigned int x, unsigned int w, char **s)
{
	return _EditStringUTF8 (y, x, w, s, 0);
}

int EditStringASCII(unsigned int y, unsigned int x, unsigned int w, char **s)
{
	return _EditStringUTF8 (y, x, w, s, 1);
}

struct Table_Single_t
{
	uint32_t Src;
	uint32_t Dst1;
};
struct Table_Double_t
{
	uint32_t Src;
	uint32_t Dst1, Dst2;
};
struct Table_Triple_t
{
	uint32_t Src;
	uint32_t Dst1, Dst2, Dst3;
};

static const struct Table_Single_t Table_Single[] = {
#include "CaseFoldingTable-1.c"
};
static const struct Table_Double_t Table_Double[] = {
#include "CaseFoldingTable-2.c"
};
static const struct Table_Triple_t Table_Triple[] = {
#include "CaseFoldingTable-3.c"
};

char *utf8_casefold (const char *src)
{
	int len = strlen (src);
	const char *iter;
	int remaining;

	char *retval, *retvalnext;

	int newlen = 0;

	iter = src;
	remaining = len;
	while (remaining)
	{
		int i;
		int inc;
		uint32_t codepoint = utf8_decode(iter, remaining, &inc);

		for (i = 0; i < (sizeof (Table_Single) / sizeof (Table_Single[0])); i++)
		{
			if (Table_Single[i].Src == codepoint)
			{
				newlen += utf8_encoded_length (Table_Single[i].Dst1);
				goto prescan_next;
			}
		}
		for (i = 0; i < (sizeof (Table_Double) / sizeof (Table_Double[0])); i++)
		{
			if (Table_Double[i].Src == codepoint)
			{
				newlen += utf8_encoded_length (Table_Double[i].Dst1) +
				          utf8_encoded_length (Table_Double[i].Dst2);
				goto prescan_next;
			}
		}
		for (i = 0; i < (sizeof (Table_Triple) / sizeof (Table_Triple[0])); i++)
		{
			if (Table_Triple[i].Src == codepoint)
			{
				newlen += utf8_encoded_length (Table_Triple[i].Dst1) +
				          utf8_encoded_length (Table_Triple[i].Dst2) +
				          utf8_encoded_length (Table_Triple[i].Dst3);
				goto prescan_next;
			}
		}
		newlen += inc;
prescan_next:

		remaining -= inc;
		iter += inc;
	}

	retvalnext = retval = malloc (newlen + 1);
	if (!retval)
	{
		return 0;
	}

	iter = src;
	remaining = len;
	while (remaining)
	{
		int i;
		int inc;
		uint32_t codepoint = utf8_decode(iter, remaining, &inc);

		for (i = 0; i < (sizeof (Table_Single) / sizeof (Table_Single[0])); i++)
		{
			if (Table_Single[i].Src == codepoint)
			{
				retvalnext += utf8_encode (retvalnext, Table_Single[i].Dst1);
				goto rebuild_next;
			}
		}
		for (i = 0; i < (sizeof (Table_Double) / sizeof (Table_Double[0])); i++)
		{
			if (Table_Double[i].Src == codepoint)
			{
				retvalnext += utf8_encode (retvalnext, Table_Double[i].Dst1);
				retvalnext += utf8_encode (retvalnext, Table_Double[i].Dst2);
				goto rebuild_next;
			}
		}
		for (i = 0; i < (sizeof (Table_Triple) / sizeof (Table_Triple[0])); i++)
		{
			if (Table_Triple[i].Src == codepoint)
			{
				retvalnext += utf8_encode (retvalnext, Table_Triple[i].Dst1);
				retvalnext += utf8_encode (retvalnext, Table_Triple[i].Dst2);
				retvalnext += utf8_encode (retvalnext, Table_Triple[i].Dst3);
				goto rebuild_next;
			}
		}
		memcpy (retvalnext, iter, inc);
		retvalnext += inc;
rebuild_next:
		remaining -= inc;
		iter += inc;
	}

	*retvalnext = 0;
	return retval;
}

#endif

#if 0

#include <stdio.h>
#include <string.h>

void decode (char *src)
{
	char *iter;
	int i, j, length;
	uint32_t codepoint;
	for (iter = src, i = 0; i < 6; i++)
	{
		if (*iter)
		{
			printf ("%02X ", (unsigned char)*iter);
			iter++;
		} else {
			printf ("   ");
		}
	}
	printf (" | ");
	for (iter = src, i = 0; i < 6; i++)
	{
		if (*iter)
		{
			printf ("%d%d%d ", ((unsigned char)*iter) >> 6, (((unsigned char)*iter) >> 3) & 0x07, ((unsigned char)*iter) & 0x07);
			iter++;
		} else {
			printf ("    ");
		}
	}
	printf (" | ");
	for (iter = src, i = 0; i < 6; i++)
	{
		if (*iter)
		{
			printf ("%d%d%d%d%d%d%d%d ", ((unsigned char)*iter) >> 7, (((unsigned char)*iter) >> 6) & 0x01, (((unsigned char)*iter) >> 5) & 0x01, (((unsigned char)*iter) >> 4) & 0x01, (((unsigned char)*iter) >> 3) & 0x01, (((unsigned char)*iter) >> 2) & 0x01, (((unsigned char)*iter) >> 1) & 0x01, ((unsigned char)*iter) & 0x01);
			iter++;
		} else {
			printf ("         ");
		}
	}

	printf ("|| ");

	codepoint = utf8_decode (src, strlen(src), &length);

	j=0;
	for (i = 31; i >= 0; i--)
	{
		if ((codepoint & (1<<i)) || j)
		{
			j = 1;
			printf ("%d", !!(codepoint & (1<<i)));
		} else {
			printf (" ");
		}
	}
	printf (" | ");

	j=0;
	for (i = 6; i >= 0; i--)
	{
		if (((codepoint >> (i*3)) & 7) || j)
		{
			j = 1;
			printf ("%d", (codepoint >> (i*3)) & 7);
		} else {
			printf (" ");
		}
	}
	printf (" | U+%06X\n", codepoint);
}

void encode (uint32_t codepoint)
{
	char target[7];
	char *iter;
	int i;

	utf8_encode (target, codepoint);

	printf ("U+%06X | ", codepoint);

	for (iter = target, i = 0; i < 6; i++)
	{
		if (*iter)
		{
			printf ("%d%d%d ", ((unsigned char)*iter) >> 6, (((unsigned char)*iter) >> 3) & 0x07, ((unsigned char)*iter) & 0x07);
			iter++;
		} else {
			printf ("    ");
		}
	}
	printf (" | ");
	for (iter = target, i = 0; i < 6; i++)
	{
		if (*iter)
		{
			printf ("%02X ", (unsigned char)*iter);
			iter++;
		} else {
			printf ("   ");
		}
	}
	printf ("\n");

}

int main (int argc, char *argv[])
{
	decode ("\x24");
	decode ("\xc2\xa2");
	decode ("\xe0\xa4\xb9");
	decode ("\xe2\x82\xac");
	decode ("\xed\x95\x9c");
	decode ("\xf0\x90\x8d\x88");

	encode (0x000024);
	encode (0x0000A2);
	encode (0x000939);
	encode (0x0020AC);
	encode (0x00D55C);
	encode (0x010348);
}

#endif

#if 0
int globalerror = 0;

void framelock(void)
{
	printf ("(Sporadic call to framelock())");
	globalerror++;
}

void cpiKeyHelp(uint16_t key, const char *shorthelp)
{
	printf ("(Sporadic call to cpiKeyHelp()");
	globalerror++;
}

int cpiKeyHelpDisplay(void)
{
	printf ("(Sporadic call to cpiKeyHelpDisplay()");
	globalerror++;
	return 0;
}

void cpiKeyHelpClear(void)
{
	printf ("(Sporadic call to cpiKeyHelpClear()");
	globalerror++;
}

struct console_t Console;

int main (int argc, char *argv[])
{
	char *a;
	fprintf (stderr, "a B c Æ ø Å\n%s\n\n", a  = utf8_casefold ("a B c Æ ø Å")); free (a);

	fprintf (stderr, "ss ß sS\n%s\n\n", a  = utf8_casefold ("ss ß sS")); free (a);

	fprintf (stderr, "ffi ﬃ FFI\n%s\n\n", a  = utf8_casefold ("ffi ﬃ FFI")); free (a);

	return 0;
}
#endif
