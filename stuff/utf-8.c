#include "config.h"
#include <stdint.h>
#include "types.h"
#include "utf-8.h"

int utf8_decode (const char *_src, uint32_t *codepoint, int *length)
{
	const unsigned char *src = (const unsigned char *)_src;

	*length = 0;
	if (src[0] == 0x00)
	{ /* 00000000 */
		*codepoint = 0;
		return 0;
	}

	*length = 1;
	if ((src[0] & 0x80) == 0x00)
	{ /* 0xxxxxxx */
		*codepoint = src[0];
		return 0;	
	}

	if ((src[0] & 0xc0) == 0x80)
	{ /* 10xxxxxx - should not appear alone */
		/* this codepoint is strictly not legal */
		*codepoint = src[0] & 0x3f;
		return 1;
	}

	if ((src[0] & 0xe0) == 0xc0)
	{ /* 110xxxxx - expect one more byte */
		*codepoint = (src[0] & 0x1f) << 6;
		if ((src[1] & 0xc0) == 0x80)
		{ /* 110xxxxx 10xxxxxx */
			*codepoint |= src[1] & 0x3f;
			*length = 2;
			return 0;
		}
		/* this codepoint is broken */
		return 1;
	}

	if ((src[0] & 0xf0) == 0xe0)
	{ /* 1110xxxx - expect two more bytes */
		*codepoint = ((src[0] & 0x0f)<<12);
		if ((src[1] & 0xc0) == 0x80)
		{ /* 1110xxxx 10xxxxxx - expect one more bytes */
			*codepoint |= (src[1] & 0x3f) << 6;
			*length = 2;
			if ((src[2] & 0xc0) == 0x80)
			{ /* 1110xxxx 10xxxxxx 10xxxxxx */
				*codepoint |= src[2] & 0x3f;
				*length = 3;
				return 0;
			}
			return 1;
		}
		/* this codepoint is broken */
		return 1;
	}

	if ((src[0] & 0xf8) == 0xf0)
	{ /* 11110xxx - expect three more bytes */
		*codepoint = ((src[0] & 0x07)<<18);
		if ((src[1] & 0xc0) == 0x80)
		{ /* 11110xxx 10xxxxxx - expect two more bytes */
			*codepoint |= (src[1] & 0x3f) << 12;
			*length = 2;
			if ((src[2] & 0xc0) == 0x80)
			{ /* 11110xxx 10xxxxxx 10xxxxxx - expect one more byte */
				*codepoint |= (src[2] & 0x3f) << 6;
				*length = 3;
				if ((src[3] & 0xc0) == 0x80)
				{ /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
					*codepoint |= src[3] & 0x3f;
					*length = 4;
					return 0;
				}
				return 1;
			}
			return 1;
		}
		/* this codepoint is broken */
		return 1;
	}

	if ((src[0] & 0xfc) == 0xf8)
	{ /* 111110xx - expect four more bytes .. - 5 byte sequency are not longer allowed in UTF-8 */
		*codepoint = ((src[0] & 0x03)<<24);
		if ((src[1] & 0xc0) == 0x80)
		{ /* 111110xx 10xxxxxx - expect three more bytes */
			*codepoint |= (src[1] & 0x3f) << 18;
			*length = 2;
			if ((src[2] & 0xc0) == 0x80)
			{ /* 111110xx 10xxxxxx 10xxxxxx - expect two more byte */
				*codepoint |= (src[2] & 0x3f) << 12;
				*length = 3;
				if ((src[3] & 0xc0) == 0x80)
				{ /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx - expect one more byte */
					*codepoint |= (src[3] & 0x3f) << 6;
					*length = 4;
					if ((src[3] & 0xc0) == 0x80)
					{ /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
						*codepoint |= src[4] & 0x3f;
						*length = 5;
						return 0; // this lengt his not allowed....
					}
					return 1;
				}
				return 1;
			}
			return 1;
		}
		/* this codepoint is broken */
		return 1;
	}

	if ((src[0] & 0xfe) == 0xfc)
	{ /* 1111110x - expect five more bytes .. - 6 byte sequency are not longer allowed in UTF-8 */
		*codepoint = ((src[0] & 0x03)<<30);
		if ((src[1] & 0xc0) == 0x80)
		{ /* 1111110x 10xxxxxx - expect four more bytes */
			*codepoint |= (src[1] & 0x3f) << 24;
			*length = 2;
			if ((src[2] & 0xc0) == 0x80)
			{ /* 1111110x 10xxxxxx 10xxxxxx - expect three more bytes */
				*codepoint |= (src[2] & 0x3f) << 18;
				*length = 3;
				if ((src[3] & 0xc0) == 0x80)
				{ /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx - expect two more bytes */
					*codepoint |= (src[3] & 0x3f) << 12;
					*length = 4;
					if ((src[4] & 0xc0) == 0x80)
					{ /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx - expect one more byte */
						*codepoint |= (src[4] & 0x3f) << 6;
						*length = 5;
						if ((src[5] & 0xc0) == 0x80)
						{ /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
							*codepoint |= (src[5] & 0x3f) << 6;
							*length = 6;
							return 0; // this lengt his not allowed....
						}
						return 1;
					}
					return 1;
				}
				return 1;
			}
			return 1;
		}
		/* this codepoint is broken */
		return 1;
	}

	*codepoint = 0;
	return 1;
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

	utf8_decode (src, &codepoint, &length);

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
