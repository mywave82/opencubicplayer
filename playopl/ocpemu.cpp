/* OpenCP Module Player
 * copyright (c) 2005-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * This file is based on AdPlug project, released under GPLv2
 * with permission from Simon Peter.
 *
 * AdPlug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2002 Simon Peter <dn.tlp@gmx.net>, et al.
 *
 * File is changed in letting FM_OPL be public
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

#include <assert.h>
#include "config.h"
#include "types.h"
#include "ocpemu.h"
#include "oplRetroWave.h"
#include <stdint.h>
#include <string.h>

static const int8_t offset_to_operator[32] =
{ // OPL2 register to operator, OPL3 bank to repeats the pattern
	 0,  1,  2,  3, /* offset 0x00, 0x01, 0x02, 0x03 */
	 4,  5, -1, -1, /* offset 0x04, 0x05, 0x06, 0x07 */
	 6,  7,  8,  9, /* offset 0x08, 0x09, 0x0a, 0x0b */
	10, 11, -1, -1, /* offset 0x0c, 0x0d, 0x0e, 0x0f */
	12, 13, 14, 15, /* offset 0x10, 0x11, 0x12, 0x13 */
	16, 17, -1, -1, /* offset 0x14, 0x15, 0x16, 0x17 */
	-1, -1, -1, -1, /* offset 0x18, 0x19, 0x1a, 0x1b */
	-1, -1, -1, -1  /* offset 0x1c, 0x1d, 0x1e, 0x1f */
};

static const int8_t operator_to_offset[18] = /* reverse of offset_to_operator */
{
	0x00, 0x01, 0x02, 0x03,
	0x04, 0x05, 0x08, 0x09,
	0x0a, 0x0b, 0x0c, 0x0d,
	0x10, 0x11, 0x12, 0x13,
	0x14, 0x14
};

static const int8_t channel_to_two_operator[9][2]
{ // channel 1-9 represented at index 0-8. OPL2 uses these first 9 channels only, while DUAL_OPL2 and OPL3 repeats the same pattern and we call this the second bank
	{ 0,  3},
	{ 1,  4},
	{ 2,  5},
	{ 6,  9},
	{ 7, 10},
	{ 8, 11},
	{12, 15}, // always 2-op or percussion
	{13, 16}, // always 2-op or percussion
	{14, 17}, // always 2-op or percussion
#if 0
/* second bank is repeat of first bank */
	{18, 21},
	{19, 22},
	{20, 23},
	{24, 27},
	{25, 28},
	{26, 29},
	{30, 33}, // always 2-op (or percussion in DUAL_OPL2 mode)
	{31, 34}, // always 2-op (or percussion in DUAL_OPL2 mode)
	{32, 35}, // always 2-op (or percussion in DUAL_OPL2 mode)
#endif
};

static const int8_t op2_to_channel[18] = /* reverse of channel_to_two_operator */
{
	0, 1, 2,
	0, 1, 2,

	3, 4, 5,
	3, 4, 5,

	6, 7, 8,
	6, 7, 8
};

static const int8_t op4_to_channel[18] =
{
	0, 1, 2,
	0, 1, 2,
	0, 1, 2,
	0, 1, 2,

	-1, -1, -1,
	-1, -1, -1
};

#if 0
static const int8_t channel_to_operator_four_operator[18][4]
{
	{ 0,  3,  6,  9}, bit 0, channel 0 + 3
	{ 1,  4,  7, 10}, bit 1, channel 1 + 4
	{ 2,  5,  8, 11}, bit 2, channel 2 + 5
	{-1, -1, -1, -1},
	{-1, -1, -1, -1},
	{-1, -1, -1, -1},
	{12, 15, -1, -1},
	{13, 16, -1, -1},
	{14, 17, -1, -1},
	{18, 21, 24, 27}, bit 3, channel, 9 + 12
	{19, 22, 25, 28}, bit 4, channel 10 + 13
	{20, 23, 26, 29}, bit 5, channel 11 + 14
	{-1, -1, -1, -1},
	{-1, -1, -1, -1},
	{-1, -1, -1, -1},
	{30, 33, -1, -1},
	{31, 34, -1, -1},
	{32, 35, -1, -1}
};
#endif

Cocpemu::Cocpemu(Copl *realopl, int rate, int isRetroWave) : realopl(realopl), isRetroWave(isRetroWave)
{ /* these rates does not take KSR (Envelope Scaling) into the processing, but works well enough for channel statuses */
	steprate[ 0] = 0; // fixed for-ever
	steprate[ 1] = (((uint_fast32_t)65536) * 64000) / (rate * 1132) + 1;
	steprate[ 2] = (((uint_fast32_t)65536) * 64000) / (rate * 567)  + 1;
	steprate[ 3] = (((uint_fast32_t)65536) * 64000) / (rate * 284)  + 1;
	steprate[ 4] = (((uint_fast32_t)65536) * 64000) / (rate * 135)  + 1;
	steprate[ 5] = (((uint_fast32_t)65536) * 64000) / (rate * 70)   + 1;
	steprate[ 6] = (((uint_fast32_t)65536) * 64000) / (rate * 32)   + 1;
	steprate[ 7] = (((uint_fast32_t)65536) * 64000) / (rate * 17)   + 1;
	steprate[ 8] = (((uint_fast32_t)65536) * 64000) / (rate * 13)   + 1;
	steprate[ 9] = (((uint_fast32_t)65536) * 64000) / (rate * 9)    + 1;
	steprate[10] = (((uint_fast32_t)65536) * 64000) / (rate * 5)    + 1;
	steprate[11] = (((uint_fast32_t)65536) * 64000) / (rate * 4)    + 1;
	steprate[12] = (((uint_fast32_t)65536) * 64000) / (rate * 3)    + 1;
	steprate[13] = (((uint_fast32_t)65536) * 64000) / (rate * 2)    + 1;
	steprate[14] = (((uint_fast32_t)65536) * 64000) / (rate * 1)    + 1;
	steprate[15] = 0x400000; /* instant */

	currType = realopl->gettype();
	init();
}

Cocpemu::~Cocpemu()
{
	delete realopl;
}

static int update_op_sub (uint32_t *pos, uint32_t target, uint32_t steprate, unsigned int *samples)
{
	uint32_t max = *samples * steprate;
	if (steprate >= 0x400000)
	{
		max = 0x400000;
	}
	if (!steprate)
	{
		*samples = 0;
		return 0;
	}
	if (*pos == target)
	{
		return 1;
	}
	if (*pos < target)
	{
		uint32_t diff = target - *pos;
		if (max >= diff)
		{
			*pos = target;
			*samples -= diff / steprate;
			return 1;
		} else {
			*pos += max;
			*samples = 0;
			return 0;
		}
	} else {
		uint32_t diff = *pos - target;
		if (max >= diff)
		{
			*pos = target;
			*samples -= diff / steprate;
			return 1;
		} else {
			*pos -= max;
			*samples = 0;
			return 0;
		}
	}
}

void Cocpemu::update_op (const int ch, const int o, unsigned int samples)
{
	while (samples)
	{
		switch (s.channel[ch].op[o].EnvelopeState)
		{
			case 0: return;
			case 1: if (update_op_sub(&s.channel[ch].op[o].EnvelopePosition, 0x400000, steprate[s.channel[ch].op[o].attack_rate], &samples))
				{
					s.channel[ch].op[o].EnvelopeState++;
				}
				break;
			case 2: if (update_op_sub(&s.channel[ch].op[o].EnvelopePosition, s.channel[ch].op[o].sustain_level << 17, steprate[s.channel[ch].op[o].decay_rate], &samples))
				{
					s.channel[ch].op[o].EnvelopeState++;
				}
				break;
			case 3:	if (s.channel[ch].op[o].sustain_enabled)
				{
					return;
				}
				s.channel[ch].op[o].EnvelopeState++;
				/* pass-through */
			case 4: if (update_op_sub(&s.channel[ch].op[o].EnvelopePosition, 0, steprate[s.channel[ch].op[o].release_rate], &samples))
				{
					s.channel[ch].op[o].EnvelopeState = 0;
				}
				return;
		}
	}

}

void Cocpemu::update(short *buf, int samples, uint32_t ratescale)
{
	int ch, o;
	for (ch=0; ch < 18; ch++)
	{
		for (o=0; o < 2; o++)
		{
			update_op (ch, o, samples);
		}
	}

	if (isRetroWave)
	{
		((oplRetroWave *)realopl)->ratescale = ratescale;
	}
	realopl->update(buf, samples);
}

void Cocpemu::setchip(int n)
{
	realopl->setchip(n);
	Copl::setchip(n);
}

void Cocpemu::register_channel_2_op_drum (const int chan, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	/* channel is already in percussion mode */
	s.channel[userchannel].op[0].EnvelopeState = STATE_ATTACK;
	s.channel[userchannel].op[1].EnvelopeState = STATE_ATTACK;
}

void Cocpemu::unregister_channel_2_op_drum (const int chan, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	s.channel[userchannel].op[1].EnvelopeState = STATE_RELEASE;
}

void Cocpemu::register_channel_1_op_drum (const int chan, const int op, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	/* channel is already in percussion mode */
	s.channel[userchannel].op[op].EnvelopeState = STATE_ATTACK;
}

void Cocpemu::unregister_channel_1_op_drum (const int chan, const int op, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	/* channel is already in percussion mode */
	s.channel[userchannel].op[op].EnvelopeState = STATE_RELEASE;
}

void Cocpemu::register_channel_4_op (const int chan, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	if (regcache[chip][0xc0 + chan] & 0x01)
	{ /* 1 x */
		if (regcache[chip][0xc3 + chan] & 0x01)
		{ /* 1 1 */
			s.channel[userchannel].CM = CM_4OP_AM_AM;
		} else { /* 1 0 */
			s.channel[userchannel].CM = CM_4OP_AM_FM;
		}
	} else { /* 0 x */
		if (regcache[chip][0xc3 + chan] & 0x01)
		{ /* 0 1 */
			s.channel[userchannel].CM = CM_4OP_FM_AM;
		} else { /* 0 0 */
			s.channel[userchannel].CM = CM_4OP_FM_FM;
		}
	}
	s.channel[userchannel+3].CM = CM_disabled;

	s.channel[userchannel  ].op[0].EnvelopeState = STATE_ATTACK;
	s.channel[userchannel  ].op[1].EnvelopeState = STATE_ATTACK;
	s.channel[userchannel+3].op[0].EnvelopeState = STATE_ATTACK;
	s.channel[userchannel+3].op[1].EnvelopeState = STATE_ATTACK;
}

void Cocpemu::unregister_channel_4_op (const int chan, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	s.channel[userchannel  ].op[0].EnvelopeState = STATE_RELEASE;
	s.channel[userchannel  ].op[1].EnvelopeState = STATE_RELEASE;
	s.channel[userchannel+3].op[0].EnvelopeState = STATE_RELEASE;
	s.channel[userchannel+3].op[1].EnvelopeState = STATE_RELEASE;
}

void Cocpemu::register_channel_2_op (const int chan, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	if (regcache[chip][0xc0 + chan] & 0x01)
	{ /* 1 */
		s.channel[userchannel].CM = CM_2OP_AM;
	} else { /* 0 */
		s.channel[userchannel].CM = CM_2OP_FM;
	}

	s.channel[userchannel].op[0].EnvelopeState = STATE_ATTACK;
	s.channel[userchannel].op[1].EnvelopeState = STATE_ATTACK;
}

void Cocpemu::unregister_channel_2_op (const int chan, const int chip)
{
	const int userchannel = chan + (chip?9:0);

	s.channel[userchannel].op[0].EnvelopeState = STATE_RELEASE;
	s.channel[userchannel].op[1].EnvelopeState = STATE_RELEASE;
}

void Cocpemu::write(int reg, int val)
{
	int OPL3_enabled = 0;
	int OPL3_disabled = 0;
	int OP4_chan_enabled = 0;
	int OP4_chan_disabled = 0;

	/* detect ALL channel key-on/key-off */
	if ((reg >= 0xb0) && (reg <= 0xb8)) /* key-on, either banks */
	{
		/* inhibit keyon/off for channels that have percurssion enabled */
		if ((currType != TYPE_OPL3) && (currChip == 1)) /* OPL2_DUAL has percussions in currChip 1 */
		{
			if (regcache[1][0xbd] & 0x20)
			{
				if ((reg >= 0xb6) && (reg <= 0xb8))
				{
					goto complete;
				}
			}
		} else if (currChip == 0)
		{
			if (regcache[0][0xbd] & 0x20) /* TYPE_DUAL_OPL2 and TYPE_OPL3 have percussions in currChip 0 */
			{
				if ((reg >= 0xb6) && (reg <= 0xb8))
				{
					goto complete;
				}
			}
		}

		if ((regcache[currChip][reg] ^ val) & 0x20) /* did key-on toggle? */
		{
			if (val & 0x20) /* keyon */
			{
				/* handle 4 channel operations first */
				if ((currType == TYPE_OPL3) && (regcache[1][0x05] & 0x01) && (reg <= 0xb5))
				{
					int chan = (currChip?3:0) + ((reg & 0x0f)%3);

					if (regcache[1][0x04] & (0x01 << chan))
					{
						if (reg <= 0xb2) /* ignore operation on the second half of the channel */
						{
							register_channel_4_op (reg & 0x0f, currChip);
						}
						goto complete;
					}
				}
				/* handle 2 channel operations */
				register_channel_2_op ((reg & 0x0f), currChip);
				goto complete;
			} else { /* keyoff */
				/* handle 4 channel operations first */
				if ((currType == TYPE_OPL3) && (regcache[1][0x05] & 0x01) && (reg <= 0xb5))
				{
					int chan = (currChip?3:0) + ((reg & 0x0f)%3);

					if (regcache[1][0x04] & (0x01 << chan))
					{
						if (reg <= 0xb2) /* ignore operation on the second half of the channel */
						{
							unregister_channel_4_op (reg & 0x0f, currChip);
						}
						goto complete;
					}
				}
				/* handle 2 channel operations */
				unregister_channel_2_op ((reg & 0x0f), currChip);
				goto complete;
			}
		}
	} else if (reg == 0xbd) /* percussion */
	{ /* detect percussion enable/disable, and their key-on events */
		uint8_t on = 0;
		uint8_t off = 0;
		uint8_t targetchip = 0;
		uint8_t toggles = 0;

		if (currType != TYPE_OPL3)
		{
			s.Tremolo[currChip] = !!(val & 0x80);
			s.Vibrato[currChip] = !!(val & 0x40);
		} else {
			s.Tremolo[0] = s.Tremolo[1] = !!(val & 0x48);
			s.Vibrato[0] = s.Vibrato[1] = !!(val & 0x40);
		}

		if ((currType != TYPE_OPL3) && (currChip == 1)) /* OPL2_DUAL have percussions in currChip == 1 */
		{
			/* (oldval ^ newval) &   newval                                  <= detects if value goes from off to on
			 * (oldval ^ newval) & (~newval)                                 <= detects if value goes from on to off

			 * #############################    (val & 0x20)                 <= detects if percussions are enabled
			 * ############################# & (############ ? 0x1f : 0x00)  <= only if percussions are enabled, we keep the on-events that are relevant to keep
			 */
			toggles = regcache[1][reg] ^ val;
			const uint8_t mask    = (val & 0x20) ? 0x1f : 0x00;

			on  = toggles &   val  & mask;
			off = toggles & (~val) & mask;
			targetchip = 1;
		} else { /* OPL3 mirrors both percussions into percussionA, and OPL2_dual is currChip==0 here */
			toggles = regcache[0][reg] ^ val;
			const uint8_t mask    = (val & 0x20) ? 0x1f : 0x00;

			on  = toggles &   val  & mask;
			off = toggles & (~val) & mask;
			targetchip = 0;
		}

		/* (oldval ^ newval) & 0x20 <= detects if percussions are toggled */
		if (toggles & 0x20)
		{
			off = 0x1f;
			if (val & 0x20)
			{
				s.channel[6+(targetchip?9:0)].CM = CM_PERCUSSION;
				s.channel[7+(targetchip?9:0)].CM = CM_PERCUSSION;
				s.channel[8+(targetchip?9:0)].CM = CM_PERCUSSION;
			} else {
				s.channel[6+(targetchip?9:0)].CM = CM_disabled;
				s.channel[7+(targetchip?9:0)].CM = CM_disabled;
				s.channel[8+(targetchip?9:0)].CM = CM_disabled;
			}
		}

		if (off)
		{
			if (off & 0x10) /* BD */ unregister_channel_2_op_drum (6,    targetchip);
			if (off & 0x08) /* SD */ unregister_channel_1_op_drum (7, 1, targetchip);
			if (off & 0x04) /* TT */ unregister_channel_1_op_drum (8, 0, targetchip);
			if (off & 0x02) /* CY */ unregister_channel_1_op_drum (8, 1, targetchip);
			if (off & 0x01) /* HH */ unregister_channel_1_op_drum (7, 0, targetchip);
		}
		if (on)
		{
			if (on & 0x10) /* BD */ register_channel_2_op_drum (6,    targetchip);
			if (on & 0x08) /* SD */ register_channel_1_op_drum (7, 1, targetchip);
			if (on & 0x04) /* TT */ register_channel_1_op_drum (8, 0, targetchip);
			if (on & 0x02) /* CY */ register_channel_1_op_drum (8, 1, targetchip);
			if (on & 0x01) /* HH */ register_channel_1_op_drum (7, 0, targetchip);
		}

		if ((currType == TYPE_OPL3) && (currChip == 1)) /* OPL3, bank0 and bank1 are shared */
		{ /* mirror into bank 0 */
			regcache[0][reg] = val;
		}
	}

complete:
	/* store the value into the cache */
	if ((currType == TYPE_OPL3) && (currChip == 1))
	{
		if (reg == 0x05)
		{ /* Did we enable OPL3 mode? */
			uint_fast8_t temp = (regcache[1][0x05] ^ val) & 0x01;
			OPL3_enabled = temp & val;
			OPL3_disabled = temp & (val ^ 0x01);
		}
		if ((reg == 0x04) && (regcache[1][0x05] & 0x01))
		{ /* Did we enable/disable 4-operator mode on any channels ? */
			uint_fast8_t temp = (regcache[1][0x04] ^ val) & 0x3f;
			OP4_chan_enabled = temp & val;
			OP4_chan_disabled = temp & (val ^ 0x3f);
		}
	}
	regcache[currChip][reg] = val;

/* update channels and operators */
	if ((reg >= 0x20) && (reg <= 0x35))
	{
		int op = offset_to_operator[reg & 0x1f];
		if (op >= 0)
		{
			int ch = op2_to_channel[op];
			s.channel[ch + (currChip?9:0)].op[op & 0x1].tremolo_enabled = !!(val & 0x80);
			s.channel[ch + (currChip?9:0)].op[op & 0x1].vibrato_enabled = !!(val & 0x40);
			s.channel[ch + (currChip?9:0)].op[op & 0x1].sustain_enabled = !!(val & 0x20);
			s.channel[ch + (currChip?9:0)].op[op & 0x1].ksr_enabled = !!(val & 0x10);
			s.channel[ch + (currChip?9:0)].op[op & 0x1].frequency_multiplication_factor = !!(val & 0x0f);
		}
	} else if ((reg >= 0x40) && (reg <= 0x55))
	{
		int op = offset_to_operator[reg & 0x1f];
		if (op >= 0)
		{
			int ch = op2_to_channel[op];
			s.channel[ch + (currChip?9:0)].op[op & 0x1].key_scale_level = val >> 6;
			s.channel[ch + (currChip?9:0)].op[op & 0x1].output_level = val & 0x3f;
		}
	} else if ((reg >= 0x60) && (reg <= 0x75))
	{
		int op = offset_to_operator[reg & 0x1f];
		if (op >= 0)
		{
			int ch = op2_to_channel[op];
			s.channel[ch + (currChip?9:0)].op[op & 0x1].attack_rate = val >> 4;
			s.channel[ch + (currChip?9:0)].op[op & 0x1].decay_rate = val & 0x0f;
		}
	} else if ((reg >= 0x80) && (reg <= 0x95))
	{
		int op = offset_to_operator[reg & 0x1f];
		if (op >= 0)
		{
			int ch = op2_to_channel[op];
			s.channel[ch + (currChip?9:0)].op[op & 0x1].sustain_level = val >> 4;
			s.channel[ch + (currChip?9:0)].op[op & 0x1].release_rate = val & 0x0f;
		}
	} else if ((reg >= 0xa0) && (reg <= 0xa8))
	{
		int ch = reg & 0x0f;
		s.channel[ch + (currChip?9:0)].frequency_number &= ~0xff;
		s.channel[ch + (currChip?9:0)].frequency_number |= val;
	} else if ((reg >= 0xb0) && (reg <= 0xb8))
	{
		int ch = reg & 0x0f;
		s.channel[ch + (currChip?9:0)].frequency_number &= ~0x300;
		s.channel[ch + (currChip?9:0)].frequency_number |= (val & 0x03) << 8;
		s.channel[ch + (currChip?9:0)].block_number = (val >> 2) & 0x07;
#if 0
		s.channel[ch + (currChip?9:0)].key_on = !! (val & 0x20);
#endif
	} else if ((reg >= 0xc0) && (reg <= 0xc8))
	{
		int ch = reg & 0x0f;
		if (!((regcache[1][5] & 0x1) && (currType == TYPE_OPL3)))
		{
			s.channel[ch + (currChip?9:0)].right = 1;
			s.channel[ch + (currChip?9:0)].left = 1;
		} else {
			s.channel[ch + (currChip?9:0)].right = !! (val & 0x20);
			s.channel[ch + (currChip?9:0)].left = !! (val & 0x10);
		}
#if 0
		s.channel[ch + (currChip?9:0)].feedback_modulation_factor = (val >> 1) & 0x07;
		s.channel[ch + (currChip?9:0)].synthtype = val & 0x01;
#endif
	} else if ((reg >= 0xe0) && (reg <= 0xf5))
	{
		int op = offset_to_operator[reg & 0x1f];
		if (op >= 0)
		{
			int ch = op2_to_channel[op];
			s.channel[ch + (currChip?9:0)].op[op & 0x1].waveform_select = val & 0x07;
		}
	}

/* handle mute, changing VAL if needed */
	if ((reg >= 0x40) && (reg <= 0x55))
	{
		int op = offset_to_operator [reg&0x1f];
		if (op >= 0)
		{
			int ch = op2_to_channel [op] + (currChip?9:0);
			if (regcache[1][0x05] & 0x01)
			{ /* OPL3-mode */
				int ch2 = op4_to_channel[op];
				/* Which channel is operator part if in op4 mode, and is that channel in op4 mode? */
				if ((ch2 >= 0) && (((currChip?8:1) << ch2) & regcache[1][0x04]))
				{
					ch = ch2 + (currChip?9:0);
				}
			}
			if (s.mute[ch])
			{
				val |= 0x3f; /* volume is inverted */
			}
		}
	}
	realopl->write (reg, val);

	/* re-evaluate mute if needed - going in/out of OPL3 mode and 4op channel mode */
	if (OPL3_enabled)
	{
		for (int i=0; i < 3; i++)
		{ /* if OP4 mode already enabled, ensure that mute for both channels matches, else reset by first channel */
			if ((regcache[1][0x04] & (0x01 << i)) && (s.mute[  i] != s.mute[3+  i])) { s.mute[  i] = !s.mute[  i]; setmute (  i, !s.mute[  i]); }
			if ((regcache[1][0x04] & (0x08 << i)) && (s.mute[9+i] != s.mute[3+9+i])) { s.mute[9+i] = !s.mute[9+i]; setmute (9+i, !s.mute[9+i]); }
		}
	} else if (OPL3_disabled)
	{
		for (int i=0; i < 3; i++)
		{ /* if OP4 was enabled, ensure that mutes no longer are connected, reset second channel if needed */
			if ((regcache[1][0x04] & (0x01 << i)) && (s.mute[  i] != s.mute[3+  i])) { s.mute[3+  i] = !s.mute[3+  i]; setmute (3+  i, !s.mute[3+  i]); }
			if ((regcache[1][0x04] & (0x08 << i)) && (s.mute[9+i] != s.mute[3+9+i])) { s.mute[3+9+i] = !s.mute[3+9+i]; setmute (3+9+i, !s.mute[3+9+i]); }
		}
	} else if (OP4_chan_enabled)
	{
		/* ensure that mute for both channels matches, else reset by first channel */
		for (int i=0; i < 3; i++)
		{ /* if OP4 mode already enabled, ensure that mute for both channels matches, else reset by first channel */
			if ((OP4_chan_enabled  & (0x01 << i)) && (s.mute[  i] != s.mute[3+  i])) { s.mute[  i] = !s.mute[  i]; setmute (  i, !s.mute[  i]); }
			if ((OP4_chan_enabled  & (0x08 << i)) && (s.mute[9+i] != s.mute[3+9+i])) { s.mute[9+i] = !s.mute[9+i]; setmute (9+i, !s.mute[9+i]); }
		}
	} else if (OP4_chan_disabled)
	{
		for (int i=0; i < 3; i++)
		{ /* if OP4 was enabled, ensure that mutes no longer are connected, reset second channel if needed */
			if ((OP4_chan_disabled & (0x01 << i)) && (s.mute[  i] != s.mute[3+  i])) { s.mute[3+  i] = !s.mute[3+  i]; setmute (3+  i, !s.mute[3+  i]); }
			if ((OP4_chan_disabled & (0x08 << i)) && (s.mute[9+i] != s.mute[3+9+i])) { s.mute[3+9+i] = !s.mute[3+9+i]; setmute (3+9+i, !s.mute[3+9+i]); }
		}
	}
}

void Cocpemu::init()
{
	memset (regcache, 0, sizeof (regcache));
	memset (&s, 0, sizeof (s));

	realopl->init();
	for (int i=0; i < 18; i++)
	{
		if (s.mute[i])
		{
			setmute (i, s.mute[i]);
		};
	}
}

void Cocpemu::setmute(int chan, int val)
{
	assert (chan >= 0);
	assert (chan < 18);
	val = !!val;
	if (s.mute[chan] == val)
	{
		return;
	}
	s.mute[chan]=val;
	realopl->setchip(chan/9);

	uint8_t reg0 = operator_to_offset[channel_to_two_operator[chan % 9][0]] | 0x40;
	uint8_t reg1 = operator_to_offset[channel_to_two_operator[chan % 9][1]] | 0x40;

	uint8_t mask = val ? 0x3f : 0; /* 0x3f == enforce mute */

	if (regcache[1][0x05] & 0x01)
	{ /* OPL3-mode */
		for (int i = 0; i < 3; i++)
		{
			if ((( chan      == i) && (regcache[1][0x04] & (1<<i))) ||
			    (((chan + 9) == i) && (regcache[1][0x04] & (8<<i)))) // this works, since we set the setchip above
			{
				uint8_t reg2 = operator_to_offset[channel_to_two_operator[(chan + 3 )% 9][0]] | 0x40;
				uint8_t reg3 = operator_to_offset[channel_to_two_operator[(chan + 3 )% 9][1]] | 0x40;

				realopl->write (reg0, regcache[chan / 9][reg0] | mask);
				realopl->write (reg1, regcache[chan / 9][reg1] | mask);
				realopl->write (reg2, regcache[chan / 9][reg2] | mask);
				realopl->write (reg3, regcache[chan / 9][reg3] | mask);
				return;
			}
			if ((((chan    ) == (i + 3)) && (regcache[1][0x04] & (1<<i))) ||
			    (((chan + 9) == (i + 3)) && (regcache[1][0x04] & (8<<i)))) // this works, since we set the setchip above
			{
				return; /* ignore for the second half */
			}
		}
	}

	realopl->write (reg0, regcache[chan / 9][reg0] | mask);
	realopl->write (reg1, regcache[chan / 9][reg1] | mask);
}
