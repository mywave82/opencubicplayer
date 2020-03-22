#include "config.h"
#include <stdint.h>
#include "types.h"
#include "utf-8.h"

//#define UNKNOWN_UNICODE 0xFFFD
int utf8_decode (const char *_src, size_t srclen, int *inc)
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
