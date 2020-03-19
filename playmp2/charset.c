/* OpenCP Module Player
 * copyright (c) '07-'10 Stian Skjelstad <stian@nixia.no>
 *
 * Handle charset conversion and error-recovery (from fancy UTF-stuff into
 * simplified cp437) using iconv(). Also must work around a known bug in the
 * glibc implementation.
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
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iconv.h>

#include "charset.h"
#include "stuff/latin1.h"
#include "stuff/utf-8.h"

#define TOCODE OCP_FONT "//TRANSLIT"

static void glibc_bug_4936_workaround(void);

static uint_fast32_t strlen_8bit(uint8_t *src, uint_fast32_t available_space, int req);
static uint_fast32_t strlen_16bit(uint8_t *src, uint_fast32_t available_space, int req);

static void read_iso8859_1(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static void read_unicode(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static void read_unicode_be(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static void read_utf8(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static int initok = 0;

struct charset_t __attribute__ ((visibility ("internal"))) id3v2_charsets[MAX_CHARSET_N] =
{
	{
		strlen_8bit,
		read_iso8859_1,
		"ISO8859-1"
	},{
		strlen_16bit,
		read_unicode,
		"unicode/utf-16"
	},{
		strlen_16bit,
		read_unicode_be,
		"unicode_be/utf-16be"
	},{
		strlen_8bit,
		read_utf8,
		"utf-8"
	}
};

static uint_fast32_t strlen_8bit(uint8_t *src, uint_fast32_t available_space, int req)
{
	uint_fast32_t retval = 0;
	if ((!available_space)&&req)
		return (uint_fast32_t)(-1);
	while (available_space>0)
	{
		if (!*src)
		{
			src++;
			retval++;
			available_space--;
			break;
		}
		src++;
		retval++;
		available_space--;
	}
	if (req)
		if (src[-1]&&req)
			return (uint_fast32_t)(-1);
	return retval;
}

static uint_fast32_t strlen_16bit(uint8_t *src, uint_fast32_t available_space, int req)
{
	uint_fast32_t retval = 0;
	if ((available_space<2)&&req)
		return (uint_fast32_t)(-1);
	while (available_space>=2)
	{
		if ((!src[0])&&(!src[1]))
		{
			src+=2;
			retval+=2;
			available_space-=2;
			break;
		}
		src+=2;
		retval+=2;
		available_space-=2;
	}
	if (req)
		if ((src[-2]||src[-1])&&req)
			return (uint_fast32_t)(-1);
	return retval;
}

static iconv_t fromunicode;
static iconv_t passunicode;

static iconv_t fromunicode_be;
static iconv_t passunicode_be;

static iconv_t fromutf8;

static void read_iso8859_1(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength)
{
	if (!initok)
		return;

	while (sourcelength && source[0] && targetlength)
	{
		*target = latin1_table[*source];
		target++;
		targetlength--;

		source++;
		sourcelength--;
	}

	/* if there is any more space left, terminate the string */
	if (targetlength)
	{
		*target=0;
	}
}

static void read_unicode(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength)
{
	char *src = (char *)source; /* remove the const statement, iconv() is wierd */
	size_t maxlength = sourcelength;

	char *tgt = target;
	size_t tgsize = targetlength;

	if (!initok)
		return;

	if (maxlength<2)
		return;
	{
		/* configure endian in the passunicode version as well */
		char *tgt = target;
		size_t tgsize=targetlength;
		char *src2 = src;
		size_t srclen = 2;
		iconv(passunicode, &src2, &srclen, &tgt, &tgsize);
	}

	while ((maxlength>=2)&&(src[0]||src[1]))
	{
		size_t res;

		res = iconv(fromunicode, &src, &maxlength, &tgt, &tgsize);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG) /* targetbuffer is full */
				break;
			if (errno==EILSEQ)
			{
				size_t dummytgsize;
				char *oldsrc = src;
				char dummy[32];
				char *tgtdummy = dummy;
				for (dummytgsize=2;(dummytgsize<=32)&&(res==(size_t)(-1));dummytgsize++)
				{
					res=iconv(passunicode, &src, &maxlength, &tgtdummy, &dummytgsize);
					if (src!=oldsrc)
					{
						res=0;
						break;
					}
				}
				if (res==(size_t)(-1))
					break;
				continue;
			}
			break;
		}
	}
	iconv(fromunicode, 0, 0, 0, 0);
	iconv(passunicode, 0, 0, 0, 0);
	glibc_bug_4936_workaround();

	/* if there is any more space left, terminate the string */
	if (tgsize)
	{
		*tgt=0;
	}
}

static void read_unicode_be(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength)
{
	char *src = (char *)source; /* removing the const statement. iconv() is wierd  */
	size_t maxlength = sourcelength;

	char *tgt = target;
	size_t tgsize = targetlength;

	if (!initok)
		return;

	while ((maxlength>=2)&&(src[0]||src[1]))
	{
		size_t res;

		res = iconv(fromunicode_be, &src, &maxlength, &tgt, &tgsize);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG) /* targetbuffer is full */
				break;
			if (errno==EILSEQ)
			{
				size_t dummytgsize;
				char *oldsrc = src;
				char dummy[32];
				char *tgtdummy = dummy;
				for (dummytgsize=2;(dummytgsize<=32)&&(res==(size_t)(-1));dummytgsize++)
				{
					res=iconv(passunicode_be, &src, &maxlength, &tgtdummy, &dummytgsize);
					if (src!=oldsrc)
					{
						res=0;
						break;
					}
				}
				if (res==(size_t)(-1))
					break;
				continue;
			}
			break;
		}
	}
	iconv(fromunicode_be, 0, 0, 0, 0);
	iconv(passunicode_be, 0, 0, 0, 0);

	/* if there is any more space left, terminate the string */
	if (tgsize)
	{
		*tgt=0;
	}
}

static void read_utf8(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength)
{
	char *src = (char *)source; /* removing the const statement. iconv() is wierd  */
	size_t maxlength = sourcelength;

	char *tgt = target;
	size_t tgsize = targetlength;

	if (!initok)
		return;

	while ((maxlength>=1)&&src[0])
	{
		size_t res;

		res = iconv(fromutf8, &src, &maxlength, &tgt, &tgsize);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG) /* targetbuffer is full */
				break;
			if (errno==EILSEQ)
			{
				/* invalid input character, or we have a character that can't be translated, so skip it */
				char dummy[7];
				int length = 0;
				uint32_t codepoint;
				if (maxlength >= 6)
				{
					memcpy (dummy, src, 6);
					dummy[6] = 0;
				} else {
					memcpy (dummy, src, maxlength);
					dummy[maxlength] = 0;
				}
				utf8_decode (src, &codepoint, &length);
				src += length;
				maxlength -= length;
				*tgt = '?';
				tgt++;
				tgsize--;
				continue;
			}
			break;
		}
	}
	iconv(fromutf8, 0, 0, 0, 0);

	/* if there is any more space left, terminate the string */
	if (tgsize)
	{
		*tgt=0;
	}
}

static int glibc_bug_4936_detected = 0;
static void detect_glibc_bug_4936(void)
{
	char bom[2] = {0xff, 0xfe};
	char *src;
	char temp[2];
	char *dst = temp;
	size_t srcsize;
	size_t dstsize = 2;

	iconv(fromunicode, 0, 0, 0, 0);
	srcsize = 2;
	src = bom;
	assert(iconv(fromunicode, &src, &srcsize, &dst, &dstsize)!=(size_t)(-1));
	iconv(fromunicode, 0, 0, 0, 0);
	srcsize = 2;
	src = bom;
	if (iconv(fromunicode, &src, &srcsize, &dst, &dstsize)==(size_t)(-1))
	{
		fprintf(stderr, "glibc bug 4936 detected\n");
		glibc_bug_4936_detected = 1;
		glibc_bug_4936_workaround();
	}
}
static void glibc_bug_4936_workaround(void)
{
	if (!glibc_bug_4936_detected)
		return;
	iconv_close(fromunicode);
	iconv_close(passunicode);
	fromunicode = iconv_open(TOCODE, /*"UTF-16"*/ "UNICODE" /*"ISO-10646/UCS4/"*/ /*"10646-1:1993"*/);
	assert(fromunicode!=(iconv_t)(-1));
	passunicode = iconv_open("UNICODE", "UNICODE");
	assert(passunicode!=(iconv_t)(-1));
}

void  __attribute__((constructor)) id3v2_charset_init(void)
{
        fromunicode = iconv_open(TOCODE, "UTF-16" /*"UNICODE"*/ /*"ISO-10646/UCS4/"*/ /*"10646-1:1993"*/);
	if (fromunicode==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(%s, \"UNICODE\") failed: %s\n", TOCODE, strerror(errno));
		return;
	}
	fromunicode_be = iconv_open(TOCODE, /*"UTF-16"*/ "UNICODEBIG" /*"ISO-10646/UCS4/"*/ /*"10646-1:1993"*/);
	if (fromunicode_be==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(%s, \"UNICODEBIG\") failed: %s\n", TOCODE, strerror(errno));
		iconv_close(fromunicode);
		return;
	}
	fromutf8 = iconv_open(TOCODE, "UTF-8");
	if (fromutf8==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(%s, \"UTF-8\") failed: %s\n", TOCODE, strerror(errno));
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		return;
	}
	passunicode = iconv_open("UNICODE", "UNICODE");
	if (passunicode==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(\"UNICODE\", \"UNICODE\") failed: %s\n", strerror(errno));
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		iconv_close(fromutf8);
		return;
	}
	passunicode_be = iconv_open("UNICODEBIG", "UNICODEBIG");
	if (passunicode_be==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(\"UNICODE\", \"UNICODE\") failed: %s\n", strerror(errno));
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		iconv_close(fromutf8);
		iconv_close(passunicode);
		return;
	}

	detect_glibc_bug_4936();

	initok=1;
}

void  __attribute__((destructor)) id3v2_charset_done(void)
{
	if (!initok)
		return;
	iconv_close(fromunicode);
	iconv_close(fromunicode_be);
	iconv_close(fromutf8);
	iconv_close(passunicode);
	iconv_close(passunicode_be);
	initok=0;
}
