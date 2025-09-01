/* OpenCP Module Player
 * copyright (c) 2020-'25 Stian Skjelstad <stian.skjelestad@gmail.com>
 *
 * UTF-16 encode/decode functions
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
#include "utf-8.h"
#include "utf-16.h"

/* returns non-zero if stream is broken... *length tells how many words was consumed, even if stream is broken */
uint32_t utf16_decode (const uint16_t *src, size_t srclen, int *inc)
{
	if (!srclen)
	{
		*inc = 0;
		return 0;
	}
	if (src[0] < 0xd800)
	{
		*inc = 1;
		return src[0];
	}
	if (src[0] <= 0xdbff)
	{ // high surrogate, this should appear first
		if ((srclen >= 2) && ((src[1] >= 0xdc00) && (src[1] <= 0xdfff)))
		{ // low surrogate
			*inc = 2;
			return (((src[0] & 0x3ff) << 10) | (src[1] & 0x3ff)) + 0x10000;
		}
		// no space for low surrogate, or low surrogate is missing. Partial recovery
		*inc = 1;
		return ((src[0] & 0x3ff) << 10) + 0x10000;
	}
	if (src[0] <= 0xdfff)
	{ // low surrogate, this should have appear last
		if ((srclen >= 2) && ((src[1] >= 0xd800) && (src[1] <= 0xdbff)))
		{ // high surrogate
			*inc = 2;
			return (((src[1] & 0x3ff) << 10) | (src[0] & 0x3ff)) + 0x10000;
		}
		// Partial recovery
		*inc = 1;
		return (src[0] & 0x3ff) + 0x10000;
	}
	*inc = 1;
	return src[0];
}

/* returns number of words needed, excluding zero-termination */
int utf16_encoded_length (uint32_t codepoint)
{
	if (codepoint == 0) return 0;
	if (codepoint < 0xd800) return 1;
	if (codepoint < 0xe000) return 1;// these points not valid, but windows FILE API allows them
	if (codepoint < 0x10000) return 1;
	if (codepoint <= 0x10ffff) return 2;
	return 0;
}

/* returns number of words used...  up to 2 + NULL terminator */
int utf16_encode (uint16_t *dst, uint32_t codepoint)
{
	if ((codepoint == 0) || (codepoint > 0x10ffff))
	{
		*dst = 0;
		return 0;
	}
	if (codepoint >= 0x10000)
	{
		codepoint -= 0x10000;
		dst[0] = 0xd800 | (codepoint >> 10);
		dst[1] = 0xdc00 | (codepoint & 0x3ff);
		return 2;
	}
	*dst = codepoint;
	return 1;
}

char *utf16_to_utf8(const uint16_t *src)
{
/* convert to unicode first */
	const uint16_t *iter;
	int incr;
	int utf32len;
	uint32_t *temp;
	char *retval;
	size_t utf8len;

	// count number of valid symbols
	utf32len = 0;
	for (iter = src; *iter; iter += incr)
	{
		utf32len += !!utf16_decode (iter, 1 + !!iter[1], &incr);
	}

	// allocate temporary storage
	temp = calloc ((utf32len + 1) * 4, 1);
	if (!temp)
	{
		return 0;
	}

	// convert to utf32 / bare unicode
	utf32len = 0;
	for (iter = src; *iter; iter += incr)
	{
		if ((temp[utf32len] = utf16_decode (iter, 1 + !!iter[1], &incr)))
		{
			utf32len++; // next entry, if decoding was successfull
		}
	}

	// count space needed for utf-8
	utf8len = 0;
	for (utf32len = 0; temp[utf32len]; utf32len++)
	{
		utf8len += utf8_encoded_length (temp[utf32len]);
	}

	// allocate target buffer
	retval = calloc (utf8len + 1, 1);
	if (!retval)
	{
		free (temp);
		return 0;
	}

	// fill target buffer
	utf8len = 0;
	for (utf32len = 0; temp[utf32len]; utf32len++)
	{
		utf8len += utf8_encode (retval + utf8len, temp[utf32len]);
	}

	free (temp);
	return retval;
}

uint16_t *utf8_to_utf16(const char *src)
{
	const char *iter;
	int incr;
	size_t srclen = strlen (src);
	size_t remain;
	size_t utf32len;
	uint32_t *temp;
	uint16_t *retval;
	size_t utf16len;

	// count number of valid symbols
	utf32len = 0;
	for (iter = src, remain = srclen; remain; iter += incr)
	{
		utf32len += !!utf8_decode (iter, remain, &incr);
		remain   -= incr;
	}

	// allocate temporary storage
	temp = calloc ((utf32len + 1) * 4, 1);
	if (!temp)
	{
		return 0;
	}

	// convert to utf32 / bare unicode
	utf32len = 0;
	for (iter = src, remain = srclen; remain; iter += incr)
	{
		if ((temp[utf32len] = utf8_decode (iter, remain, &incr)))
		{
			utf32len++; // next entry, if decoding was successfull
		}
		remain -= incr;
	}

	// count space needed for utf-16
	utf16len = 0;
	for (utf32len = 0; temp[utf32len]; utf32len++)
	{
		utf16len += utf16_encoded_length (temp[utf32len]);
	}

	// allocate target buffer
	retval = calloc ((utf16len + 1) * 2, 1);
	if (!retval)
	{
		free (temp);
		return 0;
	}

	// fill target buffer
	utf16len = 0;
	for (utf32len = 0; temp[utf32len]; utf32len++)
	{
		utf16len += utf16_encode (retval + utf16len, temp[utf32len]);
	}

	free (temp);
	return retval;
}

#if defined(_WIN32) || defined(TESTING)
uint16_t *utf8_to_utf16_LFN(const char *src, const int slashstar)
{
	const char *iter;
	int incr;
	size_t srclen = strlen (src);
	size_t remain;
	size_t utf32len;
	uint32_t *temp;
	uint16_t *retval;
	size_t utf16len;

	// count number of valid symbols
	utf32len = 0;
	for (iter = src, remain = srclen; remain; iter += incr)
	{
		utf32len += !!utf8_decode (iter, remain, &incr);
		remain   -= incr;
	}

	// allocate temporary storage
	temp = calloc ((utf32len + 1) * 4, 1);
	if (!temp)
	{
		return 0;
	}

	// convert to utf32 / bare unicode
	utf32len = 0;
	for (iter = src, remain = srclen; remain; iter += incr)
	{
		if ((temp[utf32len] = utf8_decode (iter, remain, &incr)))
		{
			utf32len++; // next entry, if decoding was successfull
		}
		remain -= incr;
	}

	// count space needed for utf-16
	utf16len = 0;
	for (utf32len = 0; temp[utf32len]; utf32len++)
	{
		utf16len += utf16_encoded_length (temp[utf32len]);
	}

	if (strncmp (src, "\\\\?\\", 4))
	{
		utf16len+=4;
	}

	if (slashstar)
	{
		if ((srclen >= 1) && (src[srclen-1] == '\\'))
		{
			utf16len++;
		} else if ((srclen < 2) || (src[srclen-2] != '\\') || (src[srclen-1] != '*'))
		{
			utf16len+= 2;
		}
	}

	// allocate target buffer
	retval = calloc ((utf16len + 1) * 2, 1);
	if (!retval)
	{
		free (temp);
		return 0;
	}

	// fill target buffer
	utf16len = 0;
	if (strncmp (src, "\\\\?\\", 4))
	{
		retval[0] = '\\';
		retval[1] = '\\';
		retval[2] = '?';
		retval[3] = '\\';
		utf16len += 4;
	}

	for (utf32len = 0; temp[utf32len]; utf32len++)
	{
		utf16len += utf16_encode (retval + utf16len, temp[utf32len]);
	}

	if (slashstar)
	{
		if ((srclen >= 1) && (src[srclen-1] == '\\'))
		{
			retval[utf16len++] = '*';
		} else if ((srclen < 2) || (src[srclen-2] != '\\') || (src[srclen-1] != '*'))
		{
			retval[utf16len++] = '\\';
			retval[utf16len++] = '*';
		}
	}

	free (temp);
	return retval;
}
#endif
