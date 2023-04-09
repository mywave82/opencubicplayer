/* OpenCP Module Player
 * copyright (c) 2021-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code is partially based on unlzw implementation from gzip 1.6. That code
 * again was directly derived from the public domain 'compress' written by
 * Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
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

#define WSIZE 0x8000      /* window size--must be a power of two, and */
                          /*  at least 32K for 15 bit methods.        */

#define DIST_BUFSIZE 0x8000 /* we can look back 15 bits */

typedef int_fast32_t    code_int;

#ifndef MAX_BITS
#  define MAX_BITS 16
#endif
#define INIT_BITS 9             /* Initial number of bits per code */

struct lzw_handle_t
{
	int state;

	uint8_t block_mode; /* flags */
	uint8_t max_bits;   /* 9 - 15 */

	uint32_t bitbuffer;
	int      bufferfill;

	int      writecodes;
	int      readcodes;
	uint16_t codes[8]; /* input is aligned to 8 codes at a time. When increasing n_bits or CLEAR command, the remaining codes in the buffer is discarted */

	int        finchar;
	code_int   oldcode;
	unsigned   bitmask;
	code_int   free_ent;
	code_int   maxcode;
	code_int   maxmaxcode;
	int        n_bits;

	uint16_t tab_prefix[1L<<MAX_BITS];
	uint8_t  tab_suffix[2L*WSIZE]; // window

	int     outpos;
	int	outlen;
	uint8_t outbuf[DIST_BUFSIZE-1];
};

#define LZW_MAGIC  "\037\235"   /* Magic header for lzw files, 1F 9D */

#define BIT_MASK    0x1f        /* Mask for 'number of compression bits' */
//          MASK    0x20        /* reserved to mean a fourth header byte */
//          MASK    0x40        /* unused, but no checked in original software */
#define LZW_RESERVED 0x60       /* reserved bits */
#define BLOCK_MODE  0x80        /* Block compression: if table is full and
                                   compression rate is dropping, clear the
                                   dictionary. */

#define CLEAR  256       /* flush the dictionary */
#define FIRST  (CLEAR+1) /* first free entry */

#define MAXCODE(n)      (1L << (n))

static void unlzw_init (struct lzw_handle_t *h)
{
	h->block_mode = BLOCK_MODE; /* block compress mode -C compatible with 2.0 */
	h->state      = 0;
	h->bitbuffer  = 0;
	h->bufferfill = 0;
	h->oldcode    = -1;
	h->finchar    = 0;
	h->outlen     = 0;
	h->n_bits     = 9;
	h->writecodes = 0;
	h->readcodes  = 8;
}

static signed int unlzw_feed (struct lzw_handle_t *h, uint8_t input)
{
	int code;

	switch (h->state)
	{
		default:
		case 0:
			if (input & LZW_RESERVED)
			{ /* reserved bits set */
				return -1;
			}
			h->block_mode = !!(input & BLOCK_MODE);
			h->max_bits = input & BIT_MASK;
			h->maxmaxcode = MAXCODE ( h->max_bits );
			if ((h->max_bits > MAX_BITS) || (h->max_bits < 9))
			{ /* too many bits */
				return -1;
			}

			h->n_bits = INIT_BITS;
			h->maxcode = MAXCODE ( h->n_bits ) - 1;
			h->bitmask = ( 1 << h->n_bits ) - 1;

			h->free_ent = h->block_mode ? FIRST : 256;

			memset (h->tab_prefix, 0, 256*sizeof (h->tab_prefix[0]));
			for (code = 0 ; code < 256 ; code++)
			{
				h->tab_suffix[code] = (uint8_t)code;
			}
			h->state = 1;
			return 0;
		case 1:
			if (h->bufferfill > 0)
			{
				h->bitbuffer |= (input << h->bufferfill);
			} else {
				h->bitbuffer = input;
			}
			h->bufferfill += 8;

			if(h->bufferfill >= h->n_bits)
			{
				h->codes[h->writecodes++] = h->bitbuffer & h->bitmask;
				h->bitbuffer >>= h->n_bits;
				h->bufferfill -= h->n_bits;

				if (h->writecodes >= 8)
				{
					/* each time this happens, bufferfill will be zero, since we 8 codes always is a multiply of 8 bits, also known as a byte */
					h->readcodes = 0;
					return 1;
				}
			}
			return 0;
	}
}

static void unlzw_flush (struct lzw_handle_t *h)
{
	h->readcodes = 0;
}

static signed int unlzw_digest (struct lzw_handle_t *h)
{
	uint_fast32_t code;
	code_int incode;

	h->outpos = 0;
	h->outlen = 0;

again:
	if (h->readcodes >= h->writecodes)
	{
		if (h->writecodes == 8)
		{
			h->writecodes = 0;
		}
		h->outlen = 0;
		return 0;
	}

	code = h->codes[h->readcodes++];

	if (h->oldcode == -1)
	{
		if (code >= 256)
		{ /* corrupt input */
			return -1;
		}
		h->oldcode = code;
		h->finchar = code;
		h->outbuf[0] = code;
		h->outpos = 0;
		h->outlen = 1;

		return 1;
	}

	if (code == CLEAR && h->block_mode)
	{
		h->readcodes = 8; /* discard remaining codes in the buffer */

		memset (h->tab_prefix, 0, 256*sizeof (h->tab_prefix[0]));
		h->free_ent = FIRST - 1;

		h->n_bits = INIT_BITS;
		h->maxcode = MAXCODE ( h->n_bits ) - 1;
		h->bitmask = ( 1 << h->n_bits ) - 1;

		goto again;
	}

	incode = code;
	h->outpos = sizeof (h->outbuf);
	h->outlen = 0;

	if (code > h->free_ent)
	{ /* corrupt input */
		return -1;
	} else if (code == h->free_ent)
	{ /* Special case for KwKwK string. */
		h->outpos--;
		h->outbuf[h->outpos] = (uint8_t)h->finchar;
		h->outlen++;
		code = h->oldcode;
	}

	while (code >= 256)
	{
		h->outpos--;
		h->outbuf[h->outpos] = h->tab_suffix[code];
		h->outlen++;
		code = h->tab_prefix[code];
	}
	h->finchar = h->tab_suffix[code];
	h->outpos--;
	h->outbuf[h->outpos] = (uint8_t)(h->finchar);
	h->outlen++;

	if ((code = h->free_ent) < h->maxmaxcode)
	{ /* Generate the new entry. */

                h->tab_prefix[code] = (uint16_t)h->oldcode;
                h->tab_suffix[code] = (uint8_t)h->finchar;
                h->free_ent = code + 1;
	}
	h->oldcode = incode;   /* Remember previous code.  */

	if (h->free_ent > h->maxcode)
	{
		h->readcodes = 8; /* discard remaining codes in the buffer, if any */

		(h->n_bits)++;
		if (h->n_bits >= h->max_bits)
		{
			h->n_bits = h->max_bits;
			h->maxcode = h->maxmaxcode;
		} else {
			h->maxcode = MAXCODE(h->n_bits)-1;
		}

		h->bitmask = (1 << h->n_bits)-1;
	}

	return 1;
}
