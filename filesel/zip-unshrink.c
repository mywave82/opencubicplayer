/* OpenCP Module Player
 * copyright (c) 2020-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress ZIP shrink method
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

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* tested with archives created with pkzip 1.1 */

#define ZIP_UNSHRINK_CODESIZE 8192

#define ZIP_UNSHRINK_ESCAPE_CODE   256
#define ZIP_UNSHRINK_CODE_INCREASE   1
#define ZIP_UNSHRINK_CODE_CLEAR      2

#define ZIP_UNSHRINK_NO_PARENT      0x2000
#define ZIP_UNSHRINK_INVALID_PARENT 0x4000 /* not used yet */
#define ZIP_UNSHRINK_HAS_CHILD      0x8000 /* used internally by clear */

struct zip_unshrink_t
{
	uint8_t  *out_buffer_readnext; /* points into table_outbuf after each zip_unshrink_feed(); */
	int       out_buffer_fill; /* uint16_t would do the trick, but this way is compatible with the other ones */

	uint16_t lastchar;
	int16_t  outbuf_pos;
	uint8_t  table_outbuf[ZIP_UNSHRINK_CODESIZE]; /* if output is longer than this, we have ended up in an endless loop, should never happen. !!!!This buffer is RIGHT aligned!!!! */
	uint8_t  table_value [ZIP_UNSHRINK_CODESIZE];
	uint16_t table_parent[ZIP_UNSHRINK_CODESIZE];
	uint8_t  codesize, inescape;

	/* input */
	uint32_t in_buffer;
	uint8_t  in_buffer_fill;

	uint16_t prevcode, lastfreecode;
};

static void zip_unshrink_init (struct zip_unshrink_t *self)
{
	unsigned int i;
	for (i=0; i < ZIP_UNSHRINK_ESCAPE_CODE; i++)
	{
		self->table_value[i]  = i;
		self->table_parent[i] = ZIP_UNSHRINK_NO_PARENT;
	}
	for (i = ZIP_UNSHRINK_ESCAPE_CODE + 1; i < ZIP_UNSHRINK_CODESIZE; i++)
	{
		self->table_value[i]  = 0;
		self->table_parent[i] = ZIP_UNSHRINK_INVALID_PARENT;
	}
	self->codesize = 9;
	self->inescape = 0;
	self->in_buffer = 0;
	self->in_buffer_fill = 0;
	self->prevcode = ZIP_UNSHRINK_INVALID_PARENT;
	self->lastchar = ZIP_UNSHRINK_INVALID_PARENT;
	self->lastfreecode = ZIP_UNSHRINK_ESCAPE_CODE + 1;

	self->out_buffer_fill = 0;
}

static void zip_unshrink_clear (struct zip_unshrink_t *self)
{
	uint16_t codeiter;

	for (codeiter = ZIP_UNSHRINK_ESCAPE_CODE + 1; codeiter < ZIP_UNSHRINK_CODESIZE; codeiter++)
	{
		if (self->table_parent[codeiter] == ZIP_UNSHRINK_NO_PARENT)
		{
			continue;
		}
		if (self->table_parent[codeiter] == ZIP_UNSHRINK_INVALID_PARENT)
		{
			continue;
		}
		self->table_parent[self->table_parent[codeiter] & (ZIP_UNSHRINK_CODESIZE - 1)] |= ZIP_UNSHRINK_HAS_CHILD;
	}
	for (codeiter = 0; codeiter < ZIP_UNSHRINK_ESCAPE_CODE; codeiter++)
	{
		self->table_parent[codeiter] &= ~ZIP_UNSHRINK_HAS_CHILD;
	}
	for (codeiter = ZIP_UNSHRINK_ESCAPE_CODE + 1; codeiter < ZIP_UNSHRINK_CODESIZE; codeiter++)
	{
		if (!(self->table_parent[codeiter] & ZIP_UNSHRINK_HAS_CHILD))
		{
			self->table_parent[codeiter] = ZIP_UNSHRINK_INVALID_PARENT;
		} else {
			self->table_parent[codeiter] &= ~ZIP_UNSHRINK_HAS_CHILD;
		}
	}
	self->lastfreecode = ZIP_UNSHRINK_ESCAPE_CODE + 1;
}

static int zip_unshrink_feed (struct zip_unshrink_t *self, uint8_t input)
{
	const uint16_t bitmask[] = {0x0000, 0x0001, 0x0003, 0x0007,
	                            0x000f, 0x001f, 0x003f, 0x007f,
	                            0x00ff, 0x01ff, 0x03ff, 0x07ff,
	                            0x0fff, 0x1fff
	                         /*,0x3fff, 0x7fff, 0xffff */ };
	uint16_t currentcode, itercode;

	self->out_buffer_fill = 0;

	DEBUG_PRINT ("in_buffer: 0x%05x (len=%d) + 0x%02x\n", self->in_buffer, self->in_buffer_fill, input);
	/* insert input into the buffer */
	self->in_buffer |= (input << self->in_buffer_fill);
	self->in_buffer_fill += 8;
	DEBUG_PRINT ("in_buffer: 0x%05x (len=%d)\n", self->in_buffer, self->in_buffer_fill);
	/* do we have enough bits ?, if no, we wait */
	if (self->in_buffer_fill < self->codesize)
	{
		return 0;
	}
	/* grab the bits we need */
	currentcode = self->in_buffer & bitmask[self->codesize];
	DEBUG_PRINT ("in_buffer: 0x%05x (len=%d) - 0x%03x (codesize=%d bitmask=0x%04x)\n", self->in_buffer, self->in_buffer_fill, currentcode, self->codesize, bitmask[self->codesize]);
	self->in_buffer_fill -= self->codesize;
	self->in_buffer >>= self->codesize;

	if (self->inescape)
	{
		self->inescape = 0;
		if (currentcode == 1)
		{
			DEBUG_PRINT ("Increase codesize\n");
			self->codesize++;
			if (self->codesize > 13)
			{
				VERBOSE_PRINT ("Unshrink: codesize grew too big\n");
				return -1;
			}
			return 0;
		}
		if (currentcode == 2)
		{
			DEBUG_PRINT ("Clear codes\n");
			zip_unshrink_clear (self);
			return 0;
		}
		VERBOSE_PRINT ("Unshrink: unknown escape code\n");
		return -1;
	}
	if (currentcode == 256)
	{
		self->inescape = 1;
		return 0;
	}

	/* write out the sequence for the given code */
	itercode = currentcode;
	self->outbuf_pos = sizeof (self->table_outbuf);
	do {
		if (self->outbuf_pos <= 0)
		{
			VERBOSE_PRINT ("Unshrink: circular code, should not happend\n");
			return -1;
		}

		self->outbuf_pos--;

		if (self->table_parent[itercode] == ZIP_UNSHRINK_INVALID_PARENT)
		{
			VERBOSE_PRINT ("Unshrink: a non-initialized code hit\n");
			if ((self->prevcode == ZIP_UNSHRINK_INVALID_PARENT) || (self->lastchar == ZIP_UNSHRINK_INVALID_PARENT))
			{
				return -1;
			}
/* corner-case, repeat last code, but add tail, we can predict how the last code will look */
			self->table_outbuf[self->outbuf_pos] = self->lastchar;
			itercode = self->prevcode;
			continue;
		}

		DEBUG_PRINT ("Adding: %02x\n", self->table_value[itercode]);
		self->table_outbuf[self->outbuf_pos] = self->table_value[itercode];
		itercode = self->table_parent[itercode];
	} while (itercode != ZIP_UNSHRINK_NO_PARENT);

	if (self->prevcode != ZIP_UNSHRINK_INVALID_PARENT)
	{
		/* add leaf */

		for (itercode = self->lastfreecode; self->table_parent[itercode] != ZIP_UNSHRINK_INVALID_PARENT; itercode++)
		{
			if (itercode >= ZIP_UNSHRINK_CODESIZE)
			{
				VERBOSE_PRINT ("Unshrink: ran out of free codes\n");
				return -1;
			}
		}
		self->table_value[itercode]  = self->table_outbuf[self->outbuf_pos];
		self->table_parent[itercode] = self->prevcode;
		self->lastfreecode = itercode + 1;
		DEBUG_PRINT ("leaf[0x%03x] value=0x%02x parent=0x%03x\n", itercode, self->table_value[itercode], self->table_parent[itercode]);
	}
	self->prevcode = currentcode;

	self->out_buffer_readnext = self->table_outbuf + self->outbuf_pos;
	self->out_buffer_fill = sizeof (self->table_outbuf) - self->outbuf_pos;
	self->lastchar = *self->out_buffer_readnext;

	{
		int i;
		for (i=0; i < self->out_buffer_fill; i++)
		{
			DEBUG_PRINT ("Output 0x%02x\n", self->out_buffer_readnext[i]);
		}
	}

	return self->out_buffer_fill;
}
