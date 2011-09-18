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

static char *TOCODE;

static void glibc_bug_4936_workaround(void);

static uint_fast32_t strlen_8bit(uint8_t *src, uint_fast32_t available_space, int req);
static uint_fast32_t strlen_16bit(uint8_t *src, uint_fast32_t available_space, int req);
/*
void __attribute__ ((visibility ("internal"))) print_iso8859_1(uint8_t *src, uint_fast32_t length);
static void print_unicode(uint8_t *src, uint_fast32_t length);
static void print_unicode_be(uint8_t *src, uint_fast32_t length);
static void print_utf8(uint8_t *src, uint_fast32_t length);
*/
static void read_iso8859_1(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static void read_unicode(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static void read_unicode_be(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static void read_utf8(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
static int initok = 0;

struct charset_t __attribute__ ((visibility ("internal"))) id3v2_charsets[MAX_CHARSET_N] =
{
	{
		strlen_8bit,
		/* print_iso8859_1,*/
		read_iso8859_1,
		"ISO8859-1"
	},{
		strlen_16bit,
		/* print_unicode,*/
		read_unicode,
		"unicode/utf-16"
	},{
		strlen_16bit,
		/* print_unicode_be,*/
		read_unicode_be,
		"unicode_be/utf-16be"
	},{
		strlen_8bit,
		/* print_utf8,*/
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
static iconv_t fromiso8859_1;
static iconv_t passiso8859_1;

static iconv_t fromunicode;
static iconv_t passunicode;

static iconv_t fromunicode_be;
static iconv_t passunicode_be;

static iconv_t fromutf8;
static iconv_t passutf8;

#if 0
void __attribute__ ((visibility ("internal"))) print_iso8859_1(uint8_t *src_, uint_fast32_t length)
{
	char target[33];
	size_t maxlength=length;
	char *src = (char *)src_;

	while (*src&&maxlength)
	{
		size_t res;
		char *tgt = target;
		size_t tgsize;

		tgsize=32;
		res = iconv(fromiso8859_1, &src, &maxlength, &tgt, &tgsize);

		*tgt=0;
		printf("%s", target);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG)
			{
				/* we just rerun, since we need more buffer */
				fprintf(stderr, "E2BIG\n");
				continue;
			}
			if (errno==EILSEQ)
			{
				/* skip a char */
				fprintf(stderr, "EILSEQ iso8859-1\n");
				tgsize=1;
				iconv(passiso8859_1, &src, &maxlength, &tgt, &tgsize);
				printf("(skipped a character)");
				continue;
			}
			printf("\nleft=%d res=%d errno=%d E2BIG=%d error=%s\n", (int)maxlength, (int)res, errno, E2BIG, strerror(errno));
			fflush(stdout);
			fprintf(stderr, "FAILED iso8859-1");
			_exit(1);
		}
	}
	iconv(fromiso8859_1, 0, 0, 0, 0);
	iconv(passiso8859_1, 0, 0, 0, 0);
}

static void print_unicode(uint8_t *src_, uint_fast32_t length)
{
	char target[33];
	char *src = (char *)src_;
	size_t maxlength = length;
	if (maxlength<2)
		return;
	{
		/* configure endian in the passunicode version as well */
		char *tgt = target;
		size_t tgsize=32;
		char *src2 = src;
		size_t srclen = 2;
		iconv(passunicode, &src2, &srclen, &tgt, &tgsize);
	}

	while ((maxlength>=2)&&(src[0]||src[1]))
	{
		size_t res;
		char *tgt = target;
		size_t tgsize;

		tgsize=32;
		res = iconv(fromunicode, &src, &maxlength, &tgt, &tgsize);

		*tgt=0;
		printf("%s", target);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG)
			{
				/* we just rerun, since we need more buffer */
				fprintf(stderr, "E2BIG\n");
				continue;
			}
			if (errno==EILSEQ)
			{
				/* skip a char */
				fprintf(stderr, "EILSEQ unicode\n");
				for (tgsize=2;(tgsize<=32)&&(res==(size_t)(-1));tgsize+=2)
				{
					char *oldsrc = src;
					size_t oldtgsize=tgsize;
					res=iconv(passunicode, &src, &maxlength, &tgt, &tgsize);
					if (src!=oldsrc)
					{
						res=0;
						tgsize=oldtgsize;
						break;
					}
				/*
					if (res==(size_t)(-1))
						perror("iconv()");*/
				}
				if ((res==(size_t)(-1))/*&&(errno==EILSEQ)*/)
				{
					printf("(invalid string)");
					fprintf(stderr, "Gave up to ignore char\n");
					break;
				}
				printf("(skipped a character, %d bytes big)", (int)tgsize);
				continue;
			}
			printf("\nleft=%d res=%d errno=%d E2BIG=%d error=%s\n", (int)maxlength, (int)res, errno, E2BIG, strerror(errno));
			fflush(stdout);
			fprintf(stdout, "FAILED unicode\n");
			_exit(1);
		}
	}
	iconv(fromunicode, 0, 0, 0, 0);
	iconv(passunicode, 0, 0, 0, 0);
	glibc_bug_4936_workaround();
}

static void print_unicode_be(uint8_t *src_, uint_fast32_t length)
{
	char target[33];
	char *src = (char *)src_;
	size_t maxlength = length;

	while ((maxlength>=2)&&(src[0]||src[1]))
	{
		size_t res;
		char *tgt = target;
		size_t tgsize;

		tgsize=32;
		res = iconv(fromunicode_be, &src, &maxlength, &tgt, &tgsize);

		*tgt=0;
		printf("%s", target);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG)
			{
				/* we just rerun, since we need more buffer */
				fprintf(stderr, "E2BIG\n");
				continue;
			}
			if (errno==EILSEQ)
			{
				/* skip a char */
				fprintf(stderr, "EILSEQ unicode_be\n");
				for (tgsize=2;(tgsize<=32)&&(res==(size_t)(-1));tgsize+=2)
				{
					char *oldsrc = src;
					size_t oldtgsize=tgsize;
					res=iconv(passunicode_be, &src, &maxlength, &tgt, &tgsize);
					if (src!=oldsrc)
					{
						res=0;
						tgsize=oldtgsize;
						break;
					}
				/*
					if (res==(size_t)(-1))
						perror("iconv()");*/
				}
				if ((res==(size_t)(-1))/*&&(errno==EILSEQ)*/)
				{
					printf("(invalid string)");
					fprintf(stderr, "Gave up to ignore char\n");
					break;
				}
				printf("(skipped a character, %d bytes big)", (int)tgsize);
				continue;
			}
			printf("\nleft=%d res=%d errno=%d E2BIG=%d error=%s\n", (int)maxlength, (int)res, errno, E2BIG, strerror(errno));
			fflush(stdout);
			fprintf(stdout, "FAILED unicode_be\n");
			_exit(1);
		}
	}
	iconv(fromunicode_be, 0, 0, 0, 0);
	iconv(passunicode_be, 0, 0, 0, 0);
}

static void print_utf8(uint8_t *src_, uint_fast32_t length)
{
	char target[33];
	char *src = (char *)src_;
	size_t maxlength = length;

	while ((maxlength>=1)&&(src[0])
	{
		size_t res;
		char *tgt = target;
		size_t tgsize;

		tgsize=32;
		res = iconv(fromutf8, &src, &maxlength, &tgt, &tgsize);

		*tgt=0;
		printf("%s", target);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG)
			{
				/* we just rerun, since we need more buffer */
				fprintf(stderr, "E2BIG\n");
				continue;
			}
			if (errno==EILSEQ)
			{
				/* skip a char */
				fprintf(stderr, "EILSEQ utf8\n");
				for (tgsize=1;(tgsize<=32)&&(res==(size_t)(-1));tgsize++)
				{
					char *oldsrc = src;
					size_t oldtgsize=tgsize;
					res=iconv(passutf8, &src, &maxlength, &tgt, &tgsize);
					if (src!=oldsrc)
					{
						res=0;
						tgsize=oldtgsize;
						break;
					}
				/*
					if (res==(size_t)(-1))
						perror("iconv()");*/
				}
				if ((res==(size_t)(-1))/*&&(errno==EILSEQ)*/)
				{
					printf("(invalid string)");
					fprintf(stderr, "Gave up to ignore char\n");
					break;
				}
				printf("(skipped a character, %d bytes big)", (int)tgsize);
				continue;
			}
			printf("\nleft=%d res=%d errno=%d E2BIG=%d error=%s\n", (int)maxlength, (int)res, errno, E2BIG, strerror(errno));
			fflush(stdout);
			fprintf(stdout, "FAILED utf8\n");
			_exit(1);
		}
	}
	iconv(fromutf8, 0, 0, 0, 0);
	iconv(passutf8, 0, 0, 0, 0);
}
#endif

static void read_iso8859_1(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength)
{
	char *src = (char *)source; /* removing the const statement. iconv() is wierd */
	size_t maxlength = sourcelength;

	char *tgt = target;
	size_t tgsize = targetlength;

	if (!initok)
		return;

	while ((maxlength>=1)&&src[0])
	{
		size_t res;

		res = iconv(fromiso8859_1, &src, &maxlength, &tgt, &tgsize);

		if (res==(size_t)(-1))
		{
			if (errno==E2BIG)
				break;
			if (errno==EILSEQ)
			{
				/* skip a char */
				size_t dummytgsize = 1;
				char dummy[1];
				char *tgtdummy = dummy;
				res=iconv(passiso8859_1, &src, &maxlength, &tgtdummy, &dummytgsize);
				if (res==(size_t)(-1))
					break;
				continue;
			}
			break;
		}
	}
	iconv(fromiso8859_1, 0, 0, 0, 0);
	iconv(passiso8859_1, 0, 0, 0, 0);

	/* if there is any more space left, terminate the string */
	if (tgt<(target+targetlength))
		*tgt=0;
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
	if (tgt<(target+targetlength))
		*tgt=0;
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
	if (tgt<(target+targetlength))
		*tgt=0;
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
				size_t dummytgsize;
				char *oldsrc = src;
				char dummy[32];
				char *tgtdummy = dummy;
				for (dummytgsize=1;(dummytgsize<=32)&&(res==(size_t)(-1));dummytgsize++)
				{
					res=iconv(passutf8, &src, &maxlength, &tgtdummy, &dummytgsize);
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
	iconv(fromutf8, 0, 0, 0, 0);
	iconv(passutf8, 0, 0, 0, 0);

	/* if there is any more space left, terminate the string */
	if (tgt<(target+targetlength))
		*tgt=0;
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
	const char *temp;

	if ((temp = getenv("CHARSET")))
		TOCODE = strdup(temp);
	else
		TOCODE = strdup("CP437");

        fromiso8859_1 = iconv_open(TOCODE, "ISO8859-1");
	if (fromiso8859_1==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(%s, \"ISO8859-1\") failed: %s\n", TOCODE, strerror(errno));
		return;
	}
        fromunicode = iconv_open(TOCODE, /*"UTF-16"*/ "UNICODE" /*"ISO-10646/UCS4/"*/ /*"10646-1:1993"*/);
	if (fromunicode==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(%s, \"UNICODE\") failed: %s\n", TOCODE, strerror(errno));
		iconv_close(fromiso8859_1);
		return;
	}
	fromunicode_be = iconv_open(TOCODE, /*"UTF-16"*/ "UNICODEBIG" /*"ISO-10646/UCS4/"*/ /*"10646-1:1993"*/);
	if (fromunicode_be==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(%s, \"UNICODEBIG\") failed: %s\n", TOCODE, strerror(errno));
		iconv_close(fromiso8859_1);
		iconv_close(fromunicode);
		return;
	}
	fromutf8 = iconv_open(TOCODE, "UTF-8");
	if (fromutf8==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(%s, \"UTF-8\") failed: %s\n", TOCODE, strerror(errno));
		iconv_close(fromiso8859_1);
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		return;
	}
        passiso8859_1 = iconv_open("ISO8859-1", "ISO8859-1");
	if (passiso8859_1==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(\"ISO8859-1\", \"ISO8859-1\") failed: %s\n", strerror(errno));
		iconv_close(fromiso8859_1);
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		iconv_close(fromutf8);
		return;
	}
	passunicode = iconv_open("UNICODE", "UNICODE");
	if (passunicode==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(\"UNICODE\", \"UNICODE\") failed: %s\n", strerror(errno));
		iconv_close(fromiso8859_1);
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		iconv_close(fromutf8);
		iconv_close(passiso8859_1);
		return;
	}
	passunicode_be = iconv_open("UNICODEBIG", "UNICODEBIG");
	if (passunicode_be==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(\"UNICODE\", \"UNICODE\") failed: %s\n", strerror(errno));
		iconv_close(fromiso8859_1);
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		iconv_close(fromutf8);
		iconv_close(passiso8859_1);
		iconv_close(passunicode);
		return;
	}
	passutf8 = iconv_open("UTF-8", "UTF-8");
	if (passutf8==(iconv_t)(-1))
	{
		fprintf(stderr, "iconv_open(\"UNICODE\", \"UNICODE\") failed: %s\n", strerror(errno));
		iconv_close(fromiso8859_1);
		iconv_close(fromunicode);
		iconv_close(fromunicode_be);
		iconv_close(fromutf8);
		iconv_close(passiso8859_1);
		iconv_close(passunicode);
		iconv_close(passunicode_be);
		return;
	}

	detect_glibc_bug_4936();

	initok=1;
}

void  __attribute__((destructor)) id3v2_charset_done(void)
{
	if (!initok)
		return;
	iconv_close(fromiso8859_1);
	iconv_close(fromunicode);
	iconv_close(fromunicode_be);
	iconv_close(fromutf8);
	iconv_close(passiso8859_1);
	iconv_close(passunicode);
	iconv_close(passunicode_be);
	iconv_close(passutf8);
	initok=0;
	free(TOCODE);
}
