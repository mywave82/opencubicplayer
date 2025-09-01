#include "config.h"
#define TESTING
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

#include "stuff/poutput.h"
#include "utf-16.h"

#include "utf-16.c"

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

int utf16_decode_result (const uint32_t correctcode, const int correctlen, const uint32_t code, const int len)
{
	if ((code == correctcode) && (len == correctlen))
	{
		printf ("OK.");
		return 0;
	} else if (code == correctcode)
	{
		printf ("Correct code, but consumption is %d instead of %d.", len, correctlen);
		return 1;
	} else if (len == correctlen)
	{
		printf ("Incorrect code, got U+%04" PRIX32 ".", code);
		return 1;
	} else {
		printf ("Incorrect code, got U+%04" PRIX32 ", and consumption %d instead of %d.", code, len, correctlen);
		return 1;
	}
}

int test_utf16_decode (const uint16_t *codes, int srclen, const uint32_t correctcode, const int correctlen)
{
	/* using malloc, so valgrind can detect overruns */
	uint16_t *src = malloc (sizeof (uint16_t) * srclen);
	uint32_t dst;
	int inc = 100;

	int retval = 0;

	memcpy (src, codes, sizeof (uint16_t) * srclen);

	dst = utf16_decode (src, srclen, &inc);

	retval = utf16_decode_result (correctcode, correctlen, dst, inc);

	if (memcmp (src, codes, sizeof (uint16_t) * srclen))
	{
		retval++;
		printf (" Source data was changed!!!");
	}
	printf ("\n");

	free (src);
	return retval;
}

int test_1a (void)
{
	const uint16_t src[1] = {0x0024};
	printf ("test 1a: ASCII range 0x0024 => U+0024: ");
	return test_utf16_decode (src, 1, 0x0024, 1);
}

int test_1b (void)
{
	const uint16_t src[1] = {0x20AC};
	printf ("test 1b: BMP range 0x20AC => U+20AC: ");
	return test_utf16_decode (src, 1, 0x20AC, 1);
}

int test_1c (void)
{
	const uint16_t src[2] = {0xD801, 0xDC37};
	printf ("test 1c: two code units 0xD801, 0xDC37 => U+10437: ");
	return test_utf16_decode (src, 2, 0x10437, 2);
}

int test_1d (void)
{
	const uint16_t src[2] = {0xD852, 0xDF62};
	printf ("test 1d: two code units 0xD852, 0xDF62 => U+24B62: ");
	return test_utf16_decode (src, 2, 0x24B62, 2);
}

int test_1e (void)
{
	const uint16_t src[1] = {0xD7FF};
	printf ("test 1e: last unit code before surrogates 0xD7FF => U+D7FF: ");
	return test_utf16_decode (src, 1, 0xD7FF, 1);
}

int test_1f (void)
{
	const uint16_t src[1] = {0xE000};
	printf ("test 1f: first unit code after surrogates 0xE000 => U+E000: ");
	return test_utf16_decode (src, 1, 0xE000, 1);
}

int test_1g (void)
{
	const uint16_t src[1] = {0xFFFF};
	printf ("test 1g: biggest single unit code 0xFFFF => U+FFFF: ");
	return test_utf16_decode (src, 1, 0xFFFF, 1);
}

int test_2a (void)
{
	const uint16_t src[2] = {0x0024, 0x0020};
	printf ("test 2a: ASCII range 0x0024 {,0x0020} => U+0024: ");
	return test_utf16_decode (src, 2, 0x0024, 1);
}

int test_2b (void)
{
	const uint16_t src[2] = {0x20AC, 0x0020};
	printf ("test 2b: BMP range 0x20AC {,0x0020} => U+20AC: ");
	return test_utf16_decode (src, 2, 0x20AC, 1);
}

int test_2c (void)
{
	const uint16_t src[3] = {0xD801, 0xDC37, 0x0020};
	printf ("test 2c: two code units 0xD801, 0xDC37 {,0x0020} => U+10437: ");
	return test_utf16_decode (src, 3, 0x10437, 2);
}

int test_2d (void)
{
	const uint16_t src[3] = {0xD852, 0xDF62, 0x0020};
	printf ("test 2d: two code units 0xD852, 0xDF62 {,0x0020} => U+24B62: ");
	return test_utf16_decode (src, 3, 0x24B62, 2);
}

int test_2e (void)
{
	const uint16_t src[2] = {0xD7FF, 0x0020};
	printf ("test 2e: last unit code before surrogates 0xD7FF {,0x0020} => U+D7FF: ");
	return test_utf16_decode (src, 2, 0xD7FF, 1);
}

int test_2f (void)
{
	const uint16_t src[2] = {0xE000, 0x0020};
	printf ("test 2f: first unit code after surrogates 0xE000 {,0x0020} => U+E000: ");
	return test_utf16_decode (src, 2, 0xE000, 1);
}

int test_2g (void)
{
	const uint16_t src[2] = {0xFFFF, 0x0020};
	printf ("test 1g: biggest single unit code 0xFFFF {,0x0020} => U+FFFF: ");
	return test_utf16_decode (src, 2, 0xFFFF, 1);
}

int test_3a (void)
{
	const uint16_t src[1] = {0xD801};
	printf ("test 2a: single high surrogate (invalid stream) 0xD801 => U+10400: ");
	return test_utf16_decode (src, 1, 0x10400, 1);
}

int test_3b (void)
{
	const uint16_t src[1] = {0xDC01};
	printf ("test 3b: single low surrogate (invalid stream) 0xDC01 => U+10001: ");
	return test_utf16_decode (src, 1, 0x10001, 1);
}

int test_3c (void)
{
	const uint16_t src[2] = {0xD801, 0x0020};
	printf ("test 3c: single high surrogate (invalid stream) 0xD801 {,0x0020} => U+10400: ");
	return test_utf16_decode (src, 2, 0x10400, 1);
}

int test_3d (void)
{
	const uint16_t src[2] = {0xD801, 0xD7FF};
	printf ("test 3d: single high surrogate (invalid stream) 0xD801 {,0xD7FF} => U+10400: ");
	return test_utf16_decode (src, 2, 0x10400, 1);
}

int test_3e (void)
{
	const uint16_t src[2] = {0xD801, 0xD800};
	printf ("test 3e: single high surrogate (invalid stream) 0xD801 {,0xD800} => U+10400: ");
	return test_utf16_decode (src, 2, 0x10400, 1);
}

int test_3f (void)
{
	const uint16_t src[2] = {0xD801, 0xDBFF};
	printf ("test 3f: single high surrogate (invalid stream) 0xD801 {,0xDBFF} => U+10400: ");
	return test_utf16_decode (src, 2, 0x10400, 1);
}

int test_3g (void)
{
	const uint16_t src[2] = {0xD801, 0xE000};
	printf ("test 3g: single high surrogate (invalid stream) 0xD801 {,0xE000} => U+10400: ");
	return test_utf16_decode (src, 2, 0x10400, 1);
}

int test_3h (void)
{
	const uint16_t src[2] = {0xDC01, 0x0020};
	printf ("test 3h: single low surrogate (invalid stream) 0xDC01 {,0x0020} => U+10001: ");
	return test_utf16_decode (src, 2, 0x10001, 1);
}

int test_3i (void)
{
	const uint16_t src[2] = {0xDC01, 0xD7FF};
	printf ("test 3i: single low surrogate (invalid stream) 0xDC01 {,0xD7FF} => U+10001: ");
	return test_utf16_decode (src, 2, 0x10001, 1);
}

int test_3j (void)
{
	const uint16_t src[2] = {0xDC01, 0xDC00};
	printf ("test 3j: single low surrogate (invalid stream) 0xDC01 {,0xDC00} => U+10001: ");
	return test_utf16_decode (src, 2, 0x10001, 1);
}

int test_3k (void)
{
	const uint16_t src[2] = {0xDC01, 0xDFFF};
	printf ("test 3k: single low surrogate (invalid stream) 0xDC01 {,0xDFFF} => U+10001: ");
	return test_utf16_decode (src, 2, 0x10001, 1);
}

int test_3l (void)
{
	const uint16_t src[2] = {0xDC01, 0xE000};
	printf ("test 3l: single low surrogate (invalid stream) 0xDC01 {,0xE000} => U+10001: ");
	return test_utf16_decode (src, 2, 0x10001, 1);
}

int test_4a (void)
{
	const uint16_t src[2] = {0xD800, 0xDC00};
	printf ("test 4a: lowest surrogate paired value 0xD800, 0xDC00 => U+10000: ");
	return test_utf16_decode (src, 2, 0x10000, 2);
}

int test_4b (void)
{
	const uint16_t src[2] = {0xDBFF, 0xDC00};
	printf ("test 4b: high surrogate maxed value 0xDBFF, 0xDC00 => U+10FC00: ");
	return test_utf16_decode (src, 2, 0x10FC00, 2);
}

int test_4c (void)
{
	const uint16_t src[2] = {0xD800, 0xDFFF};
	printf ("test 4c: low surrogate maxed value 0xD800, 0xDC00 => U+103FF: ");
	return test_utf16_decode (src, 2, 0x103FF, 2);
}

int test_4d (void)
{
	const uint16_t src[2] = {0xDBFF, 0xDFFF};
	printf ("test 4d: highest surrogate paired value 0xDBFF, 0xDFFF => U+10FFFF: ");
	return test_utf16_decode (src, 2, 0x10FFFF, 2);
}

int test_utf16_encode (const uint32_t codepoint, const uint16_t high, const uint16_t low)
{
	int correctlength = (!!high) + (!!low);

	int targetlength = utf16_encoded_length (codepoint);
	uint16_t target[2] = {0xc0ca, 0xc07a};

	int consumed = utf16_encode (target, codepoint);

	int retval = 0;

	if (targetlength != correctlength)
	{
		printf ("utf16_encoded_length() returned %d instead of %d.", targetlength, correctlength);
		retval++;
	}
	if (consumed != correctlength)
	{
		printf ("utf16_encode() returned %d instead of %d.", consumed, correctlength);
		retval++;
	}
	if ((consumed >= 1) && (correctlength >= 1) && (target[0] != high))
	{
		printf ("first returned code is 0x%04" PRIX16 " instead of 0x%04" PRIX16 ".", target[0], high);
		retval++;
	}
	if ((consumed >= 2) && (correctlength >= 2) && (target[1] != low))
	{
		printf ("second returned code is 0x%04" PRIX16 " instead of 0x%04" PRIX16 ".", target[1], low);
		retval++;
	}

	if (!retval)
	{
		printf ("OK.");
	}
	printf ("\n");
	return retval;
}

int test_5a(void)
{
	printf ("test 5a: ASCII U+20 => 0x0020: ");
	return test_utf16_encode (0x0020, 0x0020, 0x00);
}

int test_5b(void)
{
	printf ("test 5b: Last single code before surrogates U+D7FF => 0xD7FF: ");
	return test_utf16_encode (0xD7FF, 0xD7FF, 0x00);
}

int test_5c(void)
{
	printf ("test 5c: Invalid unicode U+D800 => 0xD800: ");
	return test_utf16_encode (0xD800, 0xD800, 0x00);
}

int test_5d(void)
{
	printf ("test 5d: Invalid unicode U+DBFF => 0xDBFF: ");
	return test_utf16_encode (0xDBFF, 0xDBFF, 0x00);
}

int test_5e(void)
{
	printf ("test 5e: Invalid unicode U+DC00 => 0xDC00: ");
	return test_utf16_encode (0xDC00, 0xDC00, 0x00);
}

int test_5f(void)
{
	printf ("test 5f: Invalid unicode U+DFFF => 0xDFFF: ");
	return test_utf16_encode (0xDFFF, 0xDFFF, 0x00);
}

int test_5g(void)
{
	printf ("test 5g: First single code after surrogates U+E000 => 0xE000: ");
	return test_utf16_encode (0xE000, 0xE000, 0x00);
}

int test_5h(void)
{
	printf ("test 5h: Biggest code before using surrogates U+FFFF => 0xFFFF: ");
	return test_utf16_encode (0xFFFF, 0xFFFF, 0x00);
}

int test_5i(void)
{
	printf ("test 5i: First code using surrogates U+10000 => 0xD800, 0xDC00: ");
	return test_utf16_encode (0x10000, 0xD800, 0xDC00);
}

int test_5j(void)
{
	printf ("test 5j: LSB bit in code using surrogates U+10001 => 0xD800, 0xDC01: ");
	return test_utf16_encode (0x10001, 0xD800, 0xDC01);
}

int test_5k(void)
{
	printf ("test 5k: Maxing low surrogate U+103FF => 0xD800, 0xDFFF: ");
	return test_utf16_encode (0x103FF, 0xD800, 0xDFFF);
}

int test_5l(void)
{
	printf ("test 5l: Maxing high surrogate U+10FC00 => 0xDBFF, 0xDC00: ");
	return test_utf16_encode (0x10FC00, 0xDBFF, 0xDC00);
}

int test_5m(void)
{
	printf ("test 5m: Maxing both surrogates U+10FFFF => 0xDBFF, 0xDFFF: ");
	return test_utf16_encode (0x10FFFF, 0xDBFF, 0xDFFF);
}

int test_5n(void)
{
	printf ("test 5n: Invalid high codepoint U+1010000 => ");
	return test_utf16_encode (0x1010000, 0x0000, 0x0000);
}

int test_5o(void)
{
	printf ("test 5o: Normal high codepoint U+10437 => ");
	return test_utf16_encode (0x10437, 0xD801, 0xDC37);
}

const uint8_t foobar_utf8[] =
{
	0x66,
	0x6F,
	0x6F,
	0xC3, 0xA6,
	0xC3, 0xB8,
	0xC3, 0xA5,
	0xF0, 0x9F, 0xA6, 0x95,
	0x62,
	0x61,
	0x72,
	0x00,
};

const uint16_t foobar_utf16[] =
{
	0x0066,
	0x006F,
	0x006F,

	0x00e6,
	0x00f8,
	0x00e5,

	0xd83e, 0xdd95,

	0x0062,
	0x0061,
	0x0072,
	0x0000,
};

int test_6a(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8));
	uint16_t *dst;
	assert (sizeof (foobar_utf8) == 17);
	memcpy (src, foobar_utf8, sizeof (foobar_utf8));
	dst = utf8_to_utf16 (src);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16, sizeof (foobar_utf16)))
	{
		printf ("Data returned is not what we expected\n");
		retval++;
	} else {
		printf ("OK");
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_7a(void)
{
	int retval = 0;
	uint16_t *src = malloc (sizeof (foobar_utf16));
	char *dst;
	assert (sizeof (foobar_utf16) == 24);
	memcpy (src, foobar_utf16, sizeof (foobar_utf16));
	dst = utf16_to_utf8 (src);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf8, sizeof (foobar_utf8)))
	{
		printf ("Data returned is not what we expected\n");
		retval++;
	} else {
		printf ("OK");
	}
	free (dst);
	printf ("\n");
	return retval;
}

const uint8_t foobar_utf8_8a[] =
{
	'\\',
	0x00,
};

const uint16_t foobar_utf16_8a[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'\\',
	0x0000,
};

const uint8_t foobar_utf8_8b[] =
{
	'\\',
	'\\',
	'?',
	'\\',
	0x00,
};

const uint16_t foobar_utf16_8b[] =
{
	'\\',
	'\\',
	'?',
	'\\',
	0x0000,
};

const uint8_t foobar_utf8_8c[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'C',
	':',
	'\\',
	0x00,
};

const uint16_t foobar_utf16_8c[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'C',
	':',
	'\\',
	0x0000,
};

const uint8_t foobar_utf8_8d[] =
{
	'C',
	':',
	'\\',
	'T',
	'E',
	'S',
	'T',
	0x00,
};

const uint16_t foobar_utf16_8d[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'C',
	':',
	'\\',
	'T',
	'E',
	'S',
	'T',
	0x0000,
};

const uint8_t foobar_utf8_8e[] =
{
	'\\',
	'\\',
	'?',

	'C',
	':',
	'\\',
	0x00,
};

const uint16_t foobar_utf16_8e[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'\\',
	'\\',
	'?',

	'C',
	':',
	'\\',
	0x0000,
};

const uint8_t foobar_utf8_8f[] =
{
	'C',
	':',
	'\\',
	'F',
	'O',
	'O',
	0x00,
};

const uint16_t foobar_utf16_8f[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'C',
	':',
	'\\',
	'F',
	'O',
	'O',
	'\\',
	'*',
	0x0000,
};

const uint8_t foobar_utf8_8g[] =
{
	'C',
	':',
	'\\',
	'F',
	'O',
	'O',
	'\\',
	0x00,
};

const uint16_t foobar_utf16_8g[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'C',
	':',
	'\\',
	'F',
	'O',
	'O',
	'\\',
	'*',
	0x0000,
};

const uint8_t foobar_utf8_8h[] =
{
	'C',
	':',
	'\\',
	'F',
	'O',
	'O',
	'\\',
	'*',
	0x00,
};

const uint16_t foobar_utf16_8h[] =
{
	'\\',
	'\\',
	'?',
	'\\',

	'C',
	':',
	'\\',
	'F',
	'O',
	'O',
	'\\',
	'*',
	0x0000,
};

int test_8a(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8a));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8a, sizeof (foobar_utf8_8a));
	dst = utf8_to_utf16_LFN (src, 0);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8a, sizeof (foobar_utf16_8a)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected\n");
		printf ("src=\"%s\"\n", foobar_utf8_8a);
		printf ("expected=");
		for (iter = foobar_utf16_8a; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8a ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");

		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8a);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_8b(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8b));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8b, sizeof (foobar_utf8_8b));
	dst = utf8_to_utf16_LFN (src, 0);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8b, sizeof (foobar_utf16_8b)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected\n");
		printf ("src=\"%s\"\n", foobar_utf8_8b);
		printf ("expected=");
		for (iter = foobar_utf16_8b; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8b ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");

		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8b);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_8c(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8c));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8c, sizeof (foobar_utf8_8c));
	dst = utf8_to_utf16_LFN (src, 0);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8c, sizeof (foobar_utf16_8c)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected\n");
		printf ("src=\"%s\"\n", foobar_utf8_8c);
		printf ("expected=");
		for (iter = foobar_utf16_8c; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8c ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");

		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8c);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_8d(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8d));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8d, sizeof (foobar_utf8_8d));
	dst = utf8_to_utf16_LFN (src, 0);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8d, sizeof (foobar_utf16_8d)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected\n");
		printf ("src=\"%s\"\n", foobar_utf8_8d);
		printf ("expected=");
		for (iter = foobar_utf16_8d; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8d ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");

		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8d);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_8e(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8e));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8e, sizeof (foobar_utf8_8e));
	dst = utf8_to_utf16_LFN (src, 0);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8e, sizeof (foobar_utf16_8e)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected:\n");
		printf ("src=\"%s\"\n", foobar_utf8_8e);
		printf ("expected=");
		for (iter = foobar_utf16_8e; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8e ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");
		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8e);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_8f(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8f));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8f, sizeof (foobar_utf8_8f));
	dst = utf8_to_utf16_LFN (src, 1);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8f, sizeof (foobar_utf16_8f)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected:\n");
		printf ("src=\"%s\"\n", foobar_utf8_8f);
		printf ("expected=");
		for (iter = foobar_utf16_8f; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8f ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");
		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8f);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_8g(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8g));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8g, sizeof (foobar_utf8_8g));
	dst = utf8_to_utf16_LFN (src, 1);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8g, sizeof (foobar_utf16_8g)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected:\n");
		printf ("src=\"%s\"\n", foobar_utf8_8g);
		printf ("expected=");
		for (iter = foobar_utf16_8g; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8g ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");
		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8g);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int test_8h(void)
{
	int retval = 0;
	char *src = malloc (sizeof (foobar_utf8_8h));
	uint16_t *dst;
	memcpy (src, foobar_utf8_8h, sizeof (foobar_utf8_8h));
	dst = utf8_to_utf16_LFN (src, 1);
	free (src);
	if (!dst)
	{
		printf ("No data returned\n");
	}
	if (memcmp (dst, foobar_utf16_8h, sizeof (foobar_utf16_8h)))
	{
		const uint16_t *iter;
		printf ("Data returned is not what we expected:\n");
		printf ("src=\"%s\"\n", foobar_utf8_8h);
		printf ("expected=");
		for (iter = foobar_utf16_8h; *iter; iter++)
		{
			printf ("%s%04x", iter != foobar_utf16_8h ? " ":"", *iter);
		}
		printf ("\nresult=  ");
			for (iter = dst; *iter; iter++)
		{
			printf ("%s%04x", iter != dst ? " ":"", *iter);
		}
		printf ("\n");
		retval++;
	} else {
		printf ("%s: OK", foobar_utf8_8h);
	}
	free (dst);
	printf ("\n");
	return retval;
}

int main(int argc, char *argv[])
{
	int retval = 0;
	printf ("utf16_decode()\n");
	retval +=
		test_1a () +
		test_1b () +
		test_1c () +
		test_1d () +
		test_1e () +
		test_1f () +
		test_1g () +
		test_2a () +
		test_2b () +
		test_2c () +
		test_2d () +
		test_2e () +
		test_2f () +
		test_2g () +
		test_3a () +
		test_3b () +
		test_3c () +
		test_3d () +
		test_3e () +
		test_3f () +
		test_3g () +
		test_3h () +
		test_3i () +
		test_3j () +
		test_3k () +
		test_3l () +
		test_4a () +
		test_4b () +
		test_4c () +
		test_4d ();

	printf ("\nutf16_encoded_length() and utf16_encode()\n");

	retval +=
		test_5a () +
		test_5b () +
		test_5c () +
		test_5d () +
		test_5e () +
		test_5f () +
		test_5g () +
		test_5h () +
		test_5i () +
		test_5j () +
		test_5k () +
		test_5l () +
		test_5m () +
		test_5n () +
		test_5o ();

	printf ("\nutf8_to_utf16()\n");
	retval +=
		test_6a ();

	printf ("\nutf16_to_utf8()\n");
	retval +=
		test_7a ();

	printf ("\nutf16_to_utf8_LFN()\n");
	retval +=
		test_8a () +
		test_8b () +
		test_8c () +
		test_8d () +
		test_8e () +
		test_8f () +
		test_8g () +
		test_8h ();

	retval += globalerror;
}
