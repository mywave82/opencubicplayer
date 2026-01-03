/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress ZIP reducing method. No test files found, so code
 * is still incomplete and untested
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

#if 0
// I have no sample data to verify the decompression algorithm against

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* tested with archives created with pkzip 1.1 */

#ifdef ZIP_DEBUG
#define DEBUG_PRINT(...) do { if (do_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINT(...) {}
#endif

#ifdef ZIP_VERBOSE
#define VERBOSE_PRINT(...) do { if (do_debug_print) { fprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define VERBOSE_PRINT(...) {}
#endif

struct zip_expand_t
{
	uint32_t bitbuffer;
	uint8_t  bufferfill;

	uint8_t  Follower_Ready;
	int16_t  Follower_Load_State;
	uint8_t  Follower_Load_SubState;

	uint8_t  N[256];
	uint8_t  S[256][64];

	uint8_t  Follower_Decoder_State;

#define Follower_LastCharacter RLE_input /* These two never deviate */
//	uint8_t  Follower_LastCharacter;

	#define RLE_WINDOW_SIZE 8192 /* minimum-size: 15*256+256+1*/
	#define RLE_WINDOW_MASK 8191
	uint8_t  RLE_sliding_window[RLE_WINDOW_SIZE]; /* needs to be zero initialized */
	uint16_t RLE_sliding_window_pos; /* points to the last written character */

	uint8_t  RLE_output_buffer[127 + 255 + 3]; /* each step can produce up to  L() + C + 3 */
	uint16_t RLE_output_buffer_fill;

	uint8_t  RLE_V;
	uint16_t RLE_Len;
	uint8_t  RLE_State; /* initialize to zero */

	uint8_t  RLE_input;

	uint8_t  RLE_L_mask;
	uint8_t  RLE_D_shift;
};

void expand_init (struct zip_expand_t *self, int compression_factor /* 1 - 4 */)
{
	memset (self, 0, sizeof (*self));

	if ((compression_factor < 0) || (compression_factor > 4))
	{
		compression_factor = 1;
	}
	self->RLE_D_shift = 8 - compression_factor;
	self->RLE_L_mask = (uint16_t)0x00ff >> compression_factor;

	self->Follower_Load_State = 255;
}

static uint8_t expand_take (struct zip_expand_t *self, int codesize)
{
	const uint8_t bitmask[] = {0x0000, 0x0001, 0x0003, 0x0007,
	                           0x000f, 0x001f, 0x003f, 0x007f,
	                           0x00ff};
	uint8_t retval = self->bitbuffer & bitmask[codesize];
	DEBUG_PRINT ("bitbuffer: 0x%05x (len=%d) - 0x%03x (codesize=%d bitmask=0x%04x)\n", self->bitbuffer, self->bufferfill, retval, self->codesize, bitmask[codesize]);
	self->bufferfill -= codesize;
	self->bitbuffer >>= codesize;
	return retval;
}

int expand_feed (struct zip_expand_t *self, uint8_t input)
{
	const uint8_t B[] = {
0,                                                                /* no-op */
1,                                                                /* len=1 even with no choice, we need to see if we have a repeat or not */
1,                                                                /* len=2 */
2,2,                                                              /* len={3-4} */
3,3,3,3,                                                          /* len={5-7} */
4,4,4,4,4,4,4,4,                                                  /* len={8-15} */
5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,                                  /* len={16-31} */
6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6}; /* len={32-63}   len can be up to 32, according to documentation */

	DEBUG_PRINT ("bitbuffer: 0x%05x (len=%d) + 0x%02x\n", self->bitbuffer, self->bufferfill, input);
	/* insert input into the buffer */
	self->bitbuffer |= (input << self->bufferfill);
	self->bufferfill += 8;
	DEBUG_PRINT ("bitbuffer: 0x%05x (len=%d)\n", self->bitbuffer, self->bufferfill);

	if (!self->Follower_Ready)
	{
		while (1)
		{
			if (self->Follower_Load_SubState == 0)
			{
				if (self->bufferfill < 6)
				{
					return 0;
				}
				self->N[self->Follower_Load_State] = expand_take (self, 6);
				self->Follower_Load_SubState++;
			} else {
				if (self->bufferfill < 8)
				{
					return 0;
				}
				self->S[self->Follower_Load_State][self->Follower_Load_SubState-1] = expand_take (self, 8);
				self->Follower_Load_SubState++;
			}

			if (self->Follower_Load_SubState > self->N[self->Follower_Load_State])
			{
				self->Follower_Load_SubState = 0;
				self->Follower_Load_State--;
				if (self->Follower_Load_State < 0)
				{
					self->Follower_Ready = 1;
					break;
				}
			}
		}
	}

	while (1)
	{
		if (self->Follower_Decoder_State == 0)
		{
			if (! self->S[self->Follower_LastCharacter] )
			{
				if (self->bufferfill < 8)
				{
					return 0;
				}
				self->RLE_input = self->Follower_LastCharacter = expand_take (self, 8);
				// self->Follower_Decoder_State = 0;   Already true
				break;
			} else {
				if (self->bufferfill < 1)
				{
					return 0;
				}
				self->Follower_Decoder_State = expand_take (self, 1) ? 1 : 2;
				continue;
			}
		} else if (self->Follower_Decoder_State == 1)
		{
			if (self->bufferfill < 8)
			{
				return 0;
			}
			self->RLE_input = self->Follower_LastCharacter = expand_take (self, 8);
			self->Follower_Decoder_State = 0;
			break;
		} else if (self->Follower_Decoder_State == 2)
		{
			uint8_t Need = B[ self->N [ self->Follower_LastCharacter ] ];
			uint8_t I;
			if (self->bufferfill < Need)
			{
				return 0;
			}
			I = expand_take (self, Need);
			self->RLE_input = self->Follower_LastCharacter = self->S [ self->Follower_LastCharacter ] [ I ];
			self->Follower_Decoder_State = 0;
			break;
		}
	}

#define L(X) (X & self->RLE_L_mask)
#define F(X) ((X == self->RLE_L_mask) ? 2 : 3)
#define D(X,Y) ((X >> self->RLE_D_shift) * 256 + Y + 1)
#define C (self->RLE_input)

	switch (self->RLE_State)
	{
		case 0:
			if (C != 144)
			{
				self->RLE_output_buffer[0] = C;
				self->RLE_output_buffer_fill = 1;
				self->RLE_sliding_window[self->RLE_sliding_window_pos] = C;
				self->RLE_sliding_window_pos = (self->RLE_sliding_window_pos + 1) & RLE_WINDOW_MASK;
				return 1;
			} else {
				self->RLE_State = 1;
				return 0;
			}
			break;

		case 1:
			if (C != 0)
			{
				self->RLE_V = C;
				self->RLE_Len   = L(C);
				self->RLE_State = F(self->RLE_Len);
				return 0;
			} else {
				self->RLE_sliding_window[self->RLE_sliding_window_pos] = self->RLE_output_buffer[0] = 144;
				self->RLE_output_buffer_fill = 1;
				self->RLE_sliding_window_pos = (self->RLE_sliding_window_pos + 1) & RLE_WINDOW_MASK;
				return 1;
			}

		case 2:
			self->RLE_Len += C;
			self->RLE_State = 3;
			return 0;

		case 3:
		{
			int i;
			uint16_t pos = (RLE_WINDOW_SIZE + self->RLE_sliding_window_pos - D(self->RLE_V, C)) & RLE_WINDOW_MASK;
			for (i=0; i < self->RLE_Len + 3; i++)
			{
				self->RLE_sliding_window[self->RLE_sliding_window_pos] = self->RLE_output_buffer[i] = self->RLE_sliding_window[pos];
				self->RLE_sliding_window_pos = (self->RLE_sliding_window_pos + 1) & RLE_WINDOW_MASK;
				pos = (pos + 1) & RLE_WINDOW_MASK;
			}
			self->RLE_output_buffer_fill = self->RLE_Len + 3;
			self->RLE_State = 0;
			return self->RLE_output_buffer_fill;
		}
	}
}

#endif
