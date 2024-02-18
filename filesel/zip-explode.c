/* OpenCP Module Player
 * copyright (c) 2020-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress ZIP implode method
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
#include <stdlib.h>
#include <string.h>

struct zip_explode_tree_node_t;

struct zip_explode_tree_node_t
{
	struct zip_explode_tree_node_t *_0;
	struct zip_explode_tree_node_t *_1;
	uint8_t                 value;
};

#define ZIP_EXPLODE_NODES_MAX ((256*2-1)+(64*2-1)+(64*2-1))

/* method 1: */
#define ZIP_EXPLODE_MAX_LENGTH_PER_DISTANCE_LENGTH (63+1+255+3) /* this will take minimum 1+1+1 bits */
/* method 2: */
//#define ZIP_EXPLODE_MAX_LENGTH_PER_DISTANCE_LENGTH (63+3) /* this will take minimum 1+1+1+8 bits */

struct zip_explode_t
{
	uint8_t  out_buffer[1024]; /* atleast 3 x ZIP_EXPLODE_MAX_LENGTH_PER_DISTANCE_LENGTH */
	 /* reset after each call to feed */
	uint16_t out_buffer_fill;
	uint8_t *out_buffer_readnext; /* read-head */

	struct zip_explode_tree_node_t node[ZIP_EXPLODE_NODES_MAX];
	uint32_t nodes;

	uint32_t bitbuffer;
	uint8_t  bufferfill;
	uint8_t  K;
	uint8_t  Distance_LowBits;  /* 6 or 7 bits */
	uint8_t  Distance_HighBits; /* 6 bits */
	uint8_t  Length_LowBits;    /* 6 bits */
	uint16_t Length_AUX;        /* 8 bits */

	struct zip_explode_tree_node_t *tree_literate;
	struct zip_explode_tree_node_t *literate_next;

	struct zip_explode_tree_node_t *tree_length;
	struct zip_explode_tree_node_t *length_next;

	struct zip_explode_tree_node_t *tree_distance;
	struct zip_explode_tree_node_t *distance_next;

	uint8_t tree_buffer[257];
	uint8_t tree_codelengths[256];
	uint16_t tree_targetlength;

	uint8_t  treeload_state;
	uint16_t treeload_substate;

#define SLIDING_WINDOW_BUFFER_SIZE 16384
#define SLIDING_WINDOW_BUFFER_MASK 16383
	uint8_t  sliding_window_buffer[SLIDING_WINDOW_BUFFER_SIZE];
	uint16_t sliding_window_pos;

	/* 1 = READ          Tree1-LengthData
	   2 = READ          Tree2-LengthData
	   3 = READ          Tree3-LengthData
	 */
#define EXPLODE_STATE__READ_TREE1_LENGTHDATA 0
#define EXPLODE_STATE__READ_TREE2_LENGTHDATA 1
#define EXPLODE_STATE__READ_TREE3_LENGTHDATA 2
#define EXPLODE_STATE__STATE_1               3 /* bit-stream first-stage parsing */
#define EXPLODE_STATE__STATE_1a              4 /* literate */
#define EXPLODE_STATE__STATE_1b              5 /* 8-bit bypass */
#define EXPLODE_STATE__STATE_2               6 /* Distance low 6/7 bits */
#define EXPLODE_STATE__STATE_3               7 /* Distance high, using distance tree */
#define EXPLODE_STATE__STATE_4a              8 /* Length using tree */
#define EXPLODE_STATE__STATE_4b              9 /* Additional length, 8 bits */

#define EXPLODE_STATE__STATE_5              10 /* Pump juice and return to STATE_1 */

};

static void zip_explode_init (struct zip_explode_t *self, int trees, int K) /* 2 or 3,  4 or 8 */
{
	memset (self, 0, sizeof (*self));

	self->K = K;

	self->treeload_state = (trees == 3) ? EXPLODE_STATE__READ_TREE1_LENGTHDATA : EXPLODE_STATE__READ_TREE2_LENGTHDATA;
	self->tree_targetlength = (trees == 3) ? 256 : 64;
}

static int zip_explode_tree_parse_codelengths (struct zip_explode_t *self)
{
	uint8_t *ptr = self->tree_buffer + 1;
	uint8_t *eptr = ptr + self->tree_buffer[0] + 1;
	int pos = 0;

	for (; ptr < eptr; ptr++)
	{
		int codelen = ((*ptr) & 0x0f) + 1;
		int codes = ((*ptr) >> 4) + 1;

		DEBUG_PRINT ("%d entries with len %d\n", codes, codelen);

		while (codes)
		{
			if (pos >= self->tree_targetlength)
			{
				VERBOSE_PRINT ("Not enough positions in the current tree\n");
				return -1;
			}
			self->tree_codelengths[pos++] = codelen;
			codes--;
		}
	}
	return (pos != self->tree_targetlength);
}

#ifdef ZIP_DEBUG
static uint16_t reversecode (uint16_t num)
{
	num = (((num & 0xaaaa/*aaaa*/) >> 1) | ((num & 0x5555/*5555*/) << 1));

	num = (((num & 0xcccc/*cccc*/) >> 2) | ((num & 0x3333/*3333*/) << 2));

	num = (((num & 0xf0f0/*f0f0*/) >> 4) | ((num & 0x0f0f/*0f0f*/) << 4));

	num = (((num & 0xff00/*ff00*/) >> 8) | ((num & 0x00ff/*00ff*/) << 8));

	/*
	num = ((num >> 16) | (num << 16));
	*/

	return num;
}
#endif

static int zip_explode_generate_add_leaf (struct zip_explode_t *self, struct zip_explode_tree_node_t *iter, uint16_t code, int codelen, int value)
{
	if (!codelen)
	{
		iter->value = value;
		return 0;
	}
	if (code & 0x8000)
	{
		if (!iter->_1)
		{
			if (self->nodes >= ZIP_EXPLODE_NODES_MAX)
			{
				VERBOSE_PRINT ("Ran out of leafs (assertion)\n");
				return -1;
			}
			iter->_1 = &self->node[self->nodes++];
		}
		return zip_explode_generate_add_leaf (self, iter->_1, code << 1, codelen - 1, value);
	} else {
		if (!iter->_0)
		{
			if (self->nodes >= ZIP_EXPLODE_NODES_MAX)
			{
				VERBOSE_PRINT ("Ran out of leafs (assertion)\n");
				return -1;
			}
			iter->_0 = &self->node[self->nodes++];
		}
		return zip_explode_generate_add_leaf (self, iter->_0, code << 1, codelen - 1, value);
	}
}

static int zip_explode_generate_tree (struct zip_explode_t *self, struct zip_explode_tree_node_t **targettree)
{
	int len[18];
	signed int i, j, k;
	int CodeIncrement = 0;
	uint16_t code = 0;

	if (self->nodes >= ZIP_EXPLODE_NODES_MAX)
	{
		VERBOSE_PRINT ("Ran out of leafs (assertion)\n");
		return -1;
	}
	*targettree = &self->node[self->nodes++];


	for (i=0; i < 18; i++)
	{
		len[i] = 0;
	}
	for (i=0; i < self->tree_targetlength; i++)
	{
		len[self->tree_codelengths[i]]++;
	}
	j = 17;
	k = self->tree_targetlength;
	for (i = self->tree_targetlength - 1; i >= 0;)
	{
		code += CodeIncrement;

		while (!len[j])
		{
			k=self->tree_targetlength;
			j--;
			if (!j)
			{
				VERBOSE_PRINT ("Unable to find back enough entries with the correct code-length in tree (assertion)\n");
				return -1;
			}
			CodeIncrement = 1 << (16 - j);
		}

		k--;
		while (self->tree_codelengths[k] != j)
		{
			k--;
		}
		len[j]--;

		zip_explode_generate_add_leaf (self, *targettree, code, j, k);

#ifdef ZIP_DEBUG
		{
			int l;
			int c = reversecode (code);
			DEBUG_PRINT ("%3d: ", k);
			for (l=15; l >= j; l--)
			{
				DEBUG_PRINT (".");
			}
			for (;l >= 0; l--)
			{
				DEBUG_PRINT ("%d", !!(c & (1 << l)));
			}
			DEBUG_PRINT ("\n");
		}
#endif

		i--;
	}

	return 0;
}

static int zip_explode_feed (struct zip_explode_t *self, uint8_t input)
{
	self->out_buffer_readnext = self->out_buffer;
	self->out_buffer_fill = 0;

	if ((self->treeload_state == EXPLODE_STATE__READ_TREE1_LENGTHDATA) ||
	    (self->treeload_state == EXPLODE_STATE__READ_TREE2_LENGTHDATA) ||
	    (self->treeload_state == EXPLODE_STATE__READ_TREE3_LENGTHDATA))
	{
		self->tree_buffer[self->treeload_substate++] = input;

		if (self->treeload_substate == (self->tree_buffer[0] + 2))
		{
			switch (self->treeload_state)
			{
				case EXPLODE_STATE__READ_TREE1_LENGTHDATA:
					DEBUG_PRINT ("Literate Tree\n");
					if (zip_explode_tree_parse_codelengths (self))
					{
						VERBOSE_PRINT ("Parsing code lengths for literate tree failed\n");

						return -1;
					}
					if (zip_explode_generate_tree (self, &self->tree_literate))
					{
						VERBOSE_PRINT ("Generating literate tree failed\n");

						return -1;
					}
					self->literate_next = self->tree_literate;
					break;
				case EXPLODE_STATE__READ_TREE2_LENGTHDATA:
					DEBUG_PRINT ("Length Tree\n");
					if (zip_explode_tree_parse_codelengths (self))
					{
						VERBOSE_PRINT ("Parsing code lengths for length tree failed\n");
						return -1;
					}
					if (zip_explode_generate_tree (self, &self->tree_length))
					{
						VERBOSE_PRINT ("Generating length tree failed\n");
						return -1;
					}
					self->length_next = self->tree_length;
					break;
				case EXPLODE_STATE__READ_TREE3_LENGTHDATA:
					DEBUG_PRINT ("Distance Tree\n");
					if (zip_explode_tree_parse_codelengths (self))
					{
						VERBOSE_PRINT ("Parsing code lengths for distance tree failed\n");
						return -1;
					}
					if (zip_explode_generate_tree (self, &self->tree_distance))
					{
						VERBOSE_PRINT ("Generating distance tree failed\n");
						return -1;
					}
					self->distance_next = self->tree_distance;
					break;
			}
			self->treeload_state++;
			self->treeload_substate = 0;
			self->tree_targetlength = 64; /* if parsing a new tree, both trees after the first one have 64 entries */
		}
		return 0;
	}

	DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) + 0x%02x\n", self->bitbuffer, self->bufferfill, input);
	/* insert input into the buffer */
	self->bitbuffer |= (input << self->bufferfill);
	self->bufferfill += 8;
	DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d)\n", self->bitbuffer, self->bufferfill);

	while (self->bufferfill)
	{
		if (self->treeload_state == EXPLODE_STATE__STATE_1)
		{
			uint8_t currentcode = self->bitbuffer & 0x01;
			self->bufferfill -= 1;
			self->bitbuffer >>= 1;
			DEBUG_PRINT ("buffer: 0x%04x (len=%d) - 0x%01x (codesize=1)\n", self->bitbuffer, self->bufferfill, currentcode);

			if (currentcode == 1)
			{
				if (self->literate_next)
				{
					DEBUG_PRINT ("Going to use literate tree\n");
					/* decode one entry using literate tree */
					self->treeload_state = EXPLODE_STATE__STATE_1a;
				} else {
					DEBUG_PRINT ("Going to bypass 8 bits\n");
					/* send one byte directly to output */
					self->treeload_state = EXPLODE_STATE__STATE_1b;
				}
			} else {
				/* Use Length/Distance */
				DEBUG_PRINT ("Going to use Distance/Length\n");
				self->treeload_state = EXPLODE_STATE__STATE_2;
			}
		} else if (self->treeload_state == EXPLODE_STATE__STATE_1a)
		{ /* decode a literate */
			/* grab the bits we need */
			uint8_t currentcode = self->bitbuffer & 0x01;
			self->bufferfill -= 1;
			self->bitbuffer >>= 1;
			DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) - 0x%01x (codesize=1)\n", self->bitbuffer, self->bufferfill, currentcode);

			DEBUG_PRINT ("Doing literate tree\n");

			if (currentcode)
			{
				if (!self->literate_next->_1)
				{
					VERBOSE_PRINT ("Literate tree has incomplete leaf\n");

					return -1;
				}

				self->literate_next = self->literate_next->_1;

			} else {
				if (!self->literate_next->_0)
				{
					VERBOSE_PRINT ("Literate tree has incomplete leaf\n");
					return -1;
				}

				self->literate_next = self->literate_next->_0;
			}
			if (!(self->literate_next->_0 && self->literate_next->_1))
			{
				self->out_buffer[self->out_buffer_fill++] = self->sliding_window_buffer[self->sliding_window_pos] = self->literate_next->value;
				self->sliding_window_pos = (self->sliding_window_pos + 1) & SLIDING_WINDOW_BUFFER_MASK;

				self->literate_next = self->tree_literate;
				self->treeload_state = EXPLODE_STATE__STATE_1;
			}
		} else if (self->treeload_state == EXPLODE_STATE__STATE_1b)
		{ /* by-pass 8 bit */
			uint8_t currentcode;

			if (self->bufferfill < 8)
			{
				return self->out_buffer_fill;
			}

			currentcode = self->bitbuffer & 0xff;
			self->bufferfill -= 8;
			self->bitbuffer >>= 8;
			DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) - 0x%02x (codesize=8)\n", self->bitbuffer, self->bufferfill, currentcode);

			DEBUG_PRINT ("Doing 8bit-bypass\n");

			self->out_buffer[self->out_buffer_fill++] = self->sliding_window_buffer[self->sliding_window_pos] = currentcode;
			self->sliding_window_pos = (self->sliding_window_pos + 1) & SLIDING_WINDOW_BUFFER_MASK;

			self->treeload_state = EXPLODE_STATE__STATE_1;
		} else if (self->treeload_state == EXPLODE_STATE__STATE_2)
		{ /* read lower bits of distance */
			/* grab the bits we need */
			if (self->K == 4)
			{
				if (self->bufferfill < 6)
				{
					return self->out_buffer_fill;
				}

				self->Distance_LowBits = self->bitbuffer & 0x3f;
				self->bufferfill -= 6;
				self->bitbuffer >>= 6;
				DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) - 0x%02x (codesize=6)\n", self->bitbuffer, self->bufferfill, self->Distance_LowBits);
			} else {
				if (self->bufferfill < 7)
				{
					return self->out_buffer_fill;
				}

				self->Distance_LowBits = self->bitbuffer & 0x7f;
				self->bufferfill -= 7;
				self->bitbuffer >>= 7;
				DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) - 0x%02x (codesize=7)\n", self->bitbuffer, self->bufferfill, self->Distance_LowBits);
			}

			DEBUG_PRINT ("Doing Distance LowBits\n");

			self->treeload_state = EXPLODE_STATE__STATE_3;
		} else if (self->treeload_state == EXPLODE_STATE__STATE_3)
		{ /* read high bits of distance */
			/* grab the bits we need */
			uint8_t currentcode = self->bitbuffer & 0x01;
			self->bufferfill -= 1;
			self->bitbuffer >>= 1;
			DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) - 0x%01x (codesize=1)\n", self->bitbuffer, self->bufferfill, currentcode);

			DEBUG_PRINT ("Doing Distance Tree\n");

			if (currentcode)
			{
				if (!self->distance_next->_1)
				{
					VERBOSE_PRINT ("Distance tree has incomplete leaf\n");
					return -1;
				}

				self->distance_next = self->distance_next->_1;
			} else {
				if (!self->distance_next->_0)
				{
					VERBOSE_PRINT ("Distance tree has incomplete leaf\n");
					return -1;
				}
				self->distance_next = self->distance_next->_0;
			}

			if (!(self->distance_next->_0 && self->distance_next->_1))
			{
				self->Distance_HighBits = self->distance_next->value;
				self->distance_next = self->tree_distance;
				self->treeload_state = EXPLODE_STATE__STATE_4a;
			}
		} else if (self->treeload_state == EXPLODE_STATE__STATE_4a)
		{ /* read length using tree */
			/* grab the bits we need */
			uint8_t currentcode = self->bitbuffer & 0x01;
			self->bufferfill -= 1;
			self->bitbuffer >>= 1;
			DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) - 0x%01x (codesize=1)\n", self->bitbuffer, self->bufferfill, currentcode);

			DEBUG_PRINT ("Doing Length Tree\n");

			if (currentcode)
			{
				if (!self->length_next->_1)
				{
					VERBOSE_PRINT ("Length tree has incomplete leaf\n");
					return -1;
				}

				self->length_next = self->length_next->_1;
			} else {
				if (!self->length_next->_0)
				{
					VERBOSE_PRINT ("Length tree has incomplete leaf\n");
					return -1;
				}
				self->length_next = self->length_next->_0;
			}

			if (!(self->length_next->_0 && self->length_next->_1))
			{
				self->Length_LowBits = self->length_next->value;
				self->length_next = self->tree_length;
				if (self->Length_LowBits == 63)
				{
					self->treeload_state = EXPLODE_STATE__STATE_4b;
				} else {
					self->treeload_state = EXPLODE_STATE__STATE_5;
				}
			}
		} else if (self->treeload_state == EXPLODE_STATE__STATE_4b)
		{ /* extra 8 bit with AUX length */
			if (self->bufferfill < 8)
			{
				return self->out_buffer_fill;
			}

			/* grab the bits we need */
			self->Length_AUX = self->bitbuffer & 0xff;
			self->bufferfill -= 8;
			self->bitbuffer >>= 8;
			DEBUG_PRINT ("bitbuffer: 0x%04x (len=%d) - 0x%02x (codesize=8)\n", self->bitbuffer, self->bufferfill, self->Length_AUX);

			DEBUG_PRINT ("Doing Length AUX Tree\n");

			self->treeload_state = EXPLODE_STATE__STATE_5;
		}

		if (self->treeload_state == EXPLODE_STATE__STATE_5)
		{
			uint16_t Distance, Length, SrcPos, DstPos;

			if (self->K == 4)
			{
				Distance = (self->Distance_LowBits | self->Distance_HighBits << 6);
			} else {
				Distance = (self->Distance_LowBits | self->Distance_HighBits << 7);
			}
			Distance += 1;

			Length = self->Length_LowBits + self->Length_AUX;
			self->Length_AUX = 0;
			if (self->literate_next)
			{
				Length += 3;
			} else {
				Length += 2;
			}

			DEBUG_PRINT ("Doing Length/Distance %d %d\n", Length, Distance);

			DstPos = self->sliding_window_pos;
			SrcPos = (SLIDING_WINDOW_BUFFER_SIZE + DstPos - Distance) & SLIDING_WINDOW_BUFFER_MASK;
			while (Length)
			{
				DEBUG_PRINT ("dstpos=%d srcpos=%d 0x%02x\n", DstPos, SrcPos, self->sliding_window_buffer[SrcPos]);
				self->out_buffer[self->out_buffer_fill++] = self->sliding_window_buffer[DstPos] = self->sliding_window_buffer[SrcPos];
				SrcPos++;
				DstPos++;
				SrcPos&=SLIDING_WINDOW_BUFFER_MASK;
				DstPos&=SLIDING_WINDOW_BUFFER_MASK;
				Length--;
			}
			self->sliding_window_pos = DstPos;

			self->treeload_state = EXPLODE_STATE__STATE_1;
		}
	}

	return self->out_buffer_fill;
}
