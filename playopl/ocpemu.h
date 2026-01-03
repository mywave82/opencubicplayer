/* OpenCP Module Player
 * copyright (c) 2005-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * This file is based on AdPlug project, released under GPLv2
 * with permission from Simon Peter.
 *
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2004 Simon Peter, <dn.tlp@gmx.net>, et al.
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

#ifndef H_ADPLUG_OCPOPL
#define H_ADPLUG_OCPOPL

#include "adplug-git/src/opl.h"
#include <stdio.h>
#include <stdint.h>

struct op_status
{
	uint8_t tremolo_enabled;
	uint8_t vibrato_enabled;
	uint8_t sustain_enabled;
	uint8_t ksr_enabled;
	uint8_t frequency_multiplication_factor;
	uint8_t key_scale_level;
	uint8_t output_level;
	uint8_t attack_rate;
	uint8_t decay_rate;
	uint8_t sustain_level;
	uint8_t release_rate;
	uint8_t waveform_select;

#define STATE_OFF     0
#define STATE_ATTACK  1
#define STATE_DELAY   2
#define STATE_SUSTAIN 3
#define STATE_RELEASE 4
	uint8_t EnvelopeState; // 0=off, 1=attack, 2=delay, 3=sustain, 4=release
	uint32_t EnvelopePosition;
};

enum channel_mode
{
	CM_disabled,
	CM_2OP_FM,
	CM_2OP_AM,
	CM_4OP_FM_FM,
	CM_4OP_AM_FM,
	CM_4OP_FM_AM,
	CM_4OP_AM_AM,
	CM_PERCUSSION
};

struct channel_status
{
	enum channel_mode CM;

	uint16_t frequency_number;
	uint8_t block_number;
	uint8_t right;
	uint8_t left;
#if 0
	uint8_t feedback_modulation_factor;
	uint8_t synthtype;
#endif
	struct op_status op[2]; /* ADSR for op3+4 should be taken from channel+3 if 4OP mode is enabled */

#if 0
	uint8_t Feedback[2];     // 0=none, 1=n/16, 2=n/8, 3=n/4, 4=n/2, 5=n, 6=2.n, 7=4.n
#endif
};

struct oplStatus
{
	struct channel_status channel[18];
	char mute[18];
	uint8_t Vibrato[2]; // 0=7cents, 1=14cents
	uint8_t Tremolo[2]; // 0=1dB,    1=4.8dBm
};

class Cocpemu : public Copl
{
	Copl *realopl;
	int isRetroWave;
public:
	Cocpemu(Copl *realopl, int rate, int isRetroWave);    // rate = sample rate
	virtual ~Cocpemu();

	void update(short *buf, int samples)
	{
		fprintf (stderr, "Warning, OCP should use update with 3 parameters\n");
	}
	virtual void update(short *buf, int samples, uint32_t ratescale);   // fill buffer
	void setchip(int n);

	// template methods
	void write(int reg, int val);
	void init();

	void setmute(int chan, int val);

	struct channel_status channel[18];

	struct oplStatus s;
private:
	unsigned char regcache[2][256];
	uint32_t steprate[16]; // 16:16 fixed point

	void update_op (const int ch, const int o, unsigned int samples); // used by update() to update the volume envelope of each op

	void register_channel_2_op_drum (const int chan, const int chip);
	void register_channel_1_op_drum (const int chan, const int op, const int chip);
	void register_channel_4_op (const int chan, const int chip);
	void register_channel_2_op (const int chan, const int chip);

	void unregister_channel_2_op_drum (const int chan, const int chip);
	void unregister_channel_1_op_drum (const int chan, const int op, const int chip);
	void unregister_channel_4_op (const int chan, const int chip);
	void unregister_channel_2_op (const int chan, const int chip);
};

#endif
