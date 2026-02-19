/* OpenCP Module Player
 * copyright (c) 2019-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * HVLPlay glue logic between interface and actual renderer
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


#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "hvlplay.h"
#include "cpiface/cpiface.h" /* merge in from hvlpinst.c, to compensate for buffer-delay */
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "loader.h"
#include "player.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#define MAXIMUM_SLOW_DOWN 32
#define ROW_BUFFERS 25 /* half a second */

// We merged in the data-scraper for instview
uint8_t plInstUsed[256];

struct hvl_statbuffer_t
{
	uint16_t ht_SongNum;
	int16_t  ht_NoteNr;
	int16_t  ht_PosNr;
	int16_t  ht_Tempo;
	uint8_t  ht_SpeedMultiplier;
	/* TODO, we probably want more gut and stuff */

	struct hvl_chaninfo ChanInfo[MAX_CHANNELS];

	uint8_t in_use;
};

static uint16_t last_ht_SongNum;         /* These are delayed, so should be correct */
static int16_t  last_ht_NoteNr;          /* These are delayed, so should be correct */
static int16_t  last_ht_PosNr;           /* These are delayed, so should be correct */
static int16_t  last_ht_Tempo;           /* These are delayed, so should be correct */
static uint8_t  last_ht_SpeedMultiplier; /* These are delayed, so should be correct */

static struct hvl_statbuffer_t hvl_statbuffer[ROW_BUFFERS] = {{0}}; // half a second */
static int hvl_statbuffers_available = 0;

OCP_INTERNAL struct hvl_chaninfo ChanInfo[MAX_CHANNELS];

struct hvl_tune *ht = 0;
#warning current_cpifaceSession is temporary
static struct cpifaceSessionAPI_t *current_cpifaceSession = 0;
static int hvl_samples_per_row;

static int16_t *hvl_buf_stereo;
static int16_t *hvl_buf_16chan;
static uint32_t hvlRate;

static struct ringbuffer_t *hvl_buf_pos;
/*             tail              processing        head
 *  (free)      | already in devp | ready to stream |   (free)
 *
 *          As the tail catches up, we know data has been played, and we update our stats on the screen
 */

static uint32_t hvlbuffpos;
static int hvl_doloop;
static int hvl_looped;

static uint64_t samples_committed;
static uint64_t samples_lastui;

static int bal, vol;
static unsigned long voll,volr;
static int pan;
static int srnd;

static uint8_t hvl_muted[MAX_CHANNELS];

#define PANPROC \
do { \
	float _rs = rs, _ls = ls; \
	if(pan==-64) \
	{ \
		float t=_ls; \
		_ls = _rs; \
		_rs = t; \
	} else if(pan==64) \
	{ \
	} else if(pan==0) \
		_rs=_ls=(_rs+_ls) / 2.0; \
	else if(pan<0) \
	{ \
		_ls = _ls / (-pan/-64.0+2.0) + _rs*(64.0+pan)/128.0; \
		_rs = _rs / (-pan/-64.0+2.0) + _ls*(64.0+pan)/128.0; \
	} else if(pan<64) \
	{ \
		_ls = _ls / (pan/-64.0+2.0) + _rs*(64.0-pan)/128.0; \
		_rs = _rs / (pan/-64.0+2.0) + _ls*(64.0-pan)/128.0; \
	} \
	rs = _rs * volr / 256.0; \
	ls = _ls * voll / 256.0; \
	if (srnd) \
	{ \
		ls ^= 0xffff; \
	} \
} while(0)

OCP_INTERNAL void hvlGetChanInfo (int chan, struct hvl_chaninfo *ci)
{
	memcpy (ci, ChanInfo + chan, sizeof (*ci));
	ci->muted = hvl_muted[chan];
}

OCP_INTERNAL void hvlGetChanVolume (struct cpifaceSessionAPI_t *cpifaceSession, int chan, int *l, int *r)
{
	int16_t *src;
	int pos1, pos2;
	int length1, length2;
	int samples;

	*l = 0;
	*r = 0;

	cpifaceSession->ringbufferAPI->get_tail_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);

	src = hvl_buf_16chan + MAX_CHANNELS * 2 * pos1;

	for (samples = 0; samples < 256; samples++)
	{
		if (!length1)
		{
			length1 = length2;
			length2 = 0;
			src = hvl_buf_16chan + MAX_CHANNELS * 2 * pos2;
		}
		if (!length1)
		{
			return;
		}
		length1--;
		*l += abs (src[chan*2+0]);
		*r += abs (src[chan*2+1]);
		src += 2 * MAX_CHANNELS;
	}
}

static void hvl_statbuffer_callback_from_hvlbuf (void *arg, int samples_ago)
{
	struct hvl_statbuffer_t *state = arg;
	int i;

	last_ht_SongNum         = state->ht_SongNum;
	last_ht_NoteNr          = state->ht_NoteNr;
	last_ht_PosNr           = state->ht_PosNr;
	last_ht_Tempo           = state->ht_Tempo;
	last_ht_SpeedMultiplier = state->ht_SpeedMultiplier;

	/* This logic would normally be inside hvlMark(), but it would not be time-correct */
	/* START */
	for (i=0; i<ht->ht_InstrumentNr; i++)
	{
		if (plInstUsed[i])
		{
			plInstUsed[i] = 1;
		}
	}

	for (i=0; i < ht->ht_Channels; i++)
	{
		if ((state->ChanInfo[i].ins >= 0) && (state->ChanInfo[i].ins <= 255))
		{
			if (current_cpifaceSession->SelectedChannel == i)
			{
				plInstUsed[state->ChanInfo[i].ins] = 3;
			} else {
				if (plInstUsed[state->ChanInfo[i].ins] != 3)
				{
					plInstUsed[state->ChanInfo[i].ins] = 2;
				}
			}
		}
	}
	/* STOP */

	memcpy (ChanInfo, state->ChanInfo, sizeof (ChanInfo));

	state->in_use = 0;
	hvl_statbuffers_available++;
}

OCP_INTERNAL void hvlIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	while (hvl_statbuffers_available) /* we only prepare more data if hvl_statbuffers_available is non-zero. This gives about 0.5 seconds worth of sample-data */
	{
		int i, j;

		int pos1, pos2;
		int length1, length2;
		int16_t *src;
		int16_t *dst;

		/* limit the internal render buffer to 100ms */
		cpifaceSession->ringbufferAPI->get_tailandprocessing_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);
		length1 += length2;
		if (length1 >= hvlRate / 10)
		{
			break;
		}

		for (i=0; i < ROW_BUFFERS; i++)
		{
			if (hvl_statbuffer[i].in_use)
			{
				continue;
			}
			break;
		}
		assert (i != ROW_BUFFERS);

		hvl_statbuffer[i].ht_SongNum         = ht->ht_SongNum;
		hvl_statbuffer[i].ht_NoteNr          = ht->ht_NoteNr; // Row
		hvl_statbuffer[i].ht_PosNr           = ht->ht_PosNr;  // Order
		hvl_statbuffer[i].ht_Tempo           = ht->ht_Tempo;
		hvl_statbuffer[i].ht_SpeedMultiplier = ht->ht_SpeedMultiplier;

		for (j=0; j < ht->ht_Channels; j++)
		{
			struct hvl_voice *voice = ht->ht_Voices + j;
			struct hvl_step *Step = ht->ht_Tracks[ht->ht_Positions[ht->ht_PosNr].pos_Track[voice->vc_VoiceNum]] + ht->ht_NoteNr;
			if (voice->vc_Instrument)
			{
				if (voice->vc_Instrument->ins_Name[0])
				{
					hvl_statbuffer[i].ChanInfo[j].name = voice->vc_Instrument->ins_Name;
				} else {
					hvl_statbuffer[i].ChanInfo[j].name = 0;
				}
				hvl_statbuffer[i].ChanInfo[j].ins       = voice->vc_Instrument - ht->ht_Instruments;
				hvl_statbuffer[i].ChanInfo[j].pfx       = voice->vc_PerfList->pls_Entries[voice->vc_PerfCurrent].ple_FX[0];
				hvl_statbuffer[i].ChanInfo[j].pfxparam  = voice->vc_PerfList->pls_Entries[voice->vc_PerfCurrent].ple_FXParam[0];
				hvl_statbuffer[i].ChanInfo[j].pfxB      = voice->vc_PerfList->pls_Entries[voice->vc_PerfCurrent].ple_FX[1];
				hvl_statbuffer[i].ChanInfo[j].pfxBparam = voice->vc_PerfList->pls_Entries[voice->vc_PerfCurrent].ple_FXParam[1];
			} else {
				hvl_statbuffer[i].ChanInfo[j].name      = 0;
				hvl_statbuffer[i].ChanInfo[j].ins       = -1;
				hvl_statbuffer[i].ChanInfo[j].pfx       = 0;
				hvl_statbuffer[i].ChanInfo[j].pfxparam  = 0;
				hvl_statbuffer[i].ChanInfo[j].pfxB      = 0;
				hvl_statbuffer[i].ChanInfo[j].pfxBparam = 0;
			}

			hvl_statbuffer[i].ChanInfo[j].vol        = voice->vc_NoteMaxVolume;
			hvl_statbuffer[i].ChanInfo[j].notehit    = Step->stp_Note;
			hvl_statbuffer[i].ChanInfo[j].note       = 24 + voice->vc_TrackPeriod - 1;
			hvl_statbuffer[i].ChanInfo[j].noteperiod = voice->vc_AudioPeriod;
			hvl_statbuffer[i].ChanInfo[j].pan        = voice->vc_Pan;

			hvl_statbuffer[i].ChanInfo[j].pitchslide = voice->vc_PeriodSlidePeriod?3:((voice->vc_PeriodSlideSpeed>0)?1:((voice->vc_PeriodSlideSpeed<0)?2:0));
			hvl_statbuffer[i].ChanInfo[j].waveform   = voice->vc_Waveform;

			hvl_statbuffer[i].ChanInfo[j].volslide   = (voice->vc_VolumeSlideUp?1:0) | (voice->vc_VolumeSlideDown?2:0);
			hvl_statbuffer[i].ChanInfo[j].fx         = Step->stp_FX;
			hvl_statbuffer[i].ChanInfo[j].fxparam    = Step->stp_FXParam;
			hvl_statbuffer[i].ChanInfo[j].fxB        = Step->stp_FXb;
			hvl_statbuffer[i].ChanInfo[j].fxBparam   = Step->stp_FXbParam;
		}

		cpifaceSession->ringbufferAPI->get_head_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);

		/* We can fit length1+length2 samples into out devp-mirrored buffer */

		assert ((length1 + length2) >= hvl_samples_per_row);

		src = hvl_buf_16chan + MAX_CHANNELS * 2 * pos1;
		hvl_DecodeFrame (ht, src, hvl_samples_per_row/* * MAX_CHANNELS * 2 * sizeof (int16_t)*/);
		if (ht->ht_SongEndReached)
		{
			if (hvl_doloop)
			{
				ht->ht_SongEndReached = 0;
			} else {
				hvl_looped |= 1;
				return;
			}
		} else {
			hvl_looped &= ~1;
		}

		dst = hvl_buf_stereo + 2 * pos1;

		for (j=0; j < hvl_samples_per_row; j++)
		{
			int k;
			int32_t left = 0;
			int32_t right = 0;
			for (k = 0; k < MAX_CHANNELS; k++)
			{
				if (!hvl_muted[k])
				{
					left  += *(src++);
					right += *(src++);
				} else {
					src+=2;
				}
			}
			if (left  > INT16_MAX) left  = INT16_MAX;
			if (left  < INT16_MIN) left  = INT16_MIN;
			if (right > INT16_MAX) right = INT16_MAX;
			if (right < INT16_MIN) right = INT16_MIN;
			*(dst++) = left;
			*(dst++) = right;
		}
		if (length1 < hvl_samples_per_row)
		{
			memmove (hvl_buf_16chan, hvl_buf_16chan + (pos1 + length1), MAX_CHANNELS * sizeof(int16_t) * 2 * (hvl_samples_per_row - length1));
			memmove (hvl_buf_stereo, hvl_buf_stereo + (pos1 + length1),                sizeof(int16_t) * 2 * (hvl_samples_per_row - length1));
		}

		hvl_statbuffer[i].in_use = 1;
		cpifaceSession->ringbufferAPI->add_tail_callback_samples (hvl_buf_pos, 0, hvl_statbuffer_callback_from_hvlbuf, hvl_statbuffer + i);

		/* Adding hvl_samples_per_row to our devp-mirrored buffer */

		cpifaceSession->ringbufferAPI->head_add_samples (hvl_buf_pos, hvl_samples_per_row);

		hvl_statbuffers_available--;
	}
}

OCP_INTERNAL void hvlIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	static volatile int clipbusy=0;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	cpifaceSession->plrDevAPI->Idle();

	if (cpifaceSession->InPause || (hvl_looped == 3))
	{
		cpifaceSession->plrDevAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		cpifaceSession->plrDevAPI->Pause (0);

		cpifaceSession->plrDevAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			hvlIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			// warning this deviates, it uses get_processing_samples instead of tail, we need to keep a copy of the sound-buffers per channel, so makes more sense to track it here
			cpifaceSession->ringbufferAPI->get_processing_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);

			/* bufrate is always correct, since we get the correct speed always from the renderer */

			//if (hvlbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2);
					hvl_looped |= 2;
				} else {
					hvl_looped &= ~2;
				}

				// limit source to not overrun target buffer
				if (length1 > targetlength)
				{
					length1 = targetlength;
					length2 = 0;
				} else if ((length1 + length2) > targetlength)
				{
					length2 = targetlength - length1;
				}

				accumulated_source = accumulated_target = length1 + length2;

				while (length1)
				{
					while (length1)
					{
						int16_t rs, ls;

						rs = hvl_buf_stereo[(pos1<<1) + 0];
						ls = hvl_buf_stereo[(pos1<<1) + 1];

						PANPROC;

						*(t++) = rs;
						*(t++) = ls;

						pos1++;
						length1--;

						//accumulated_target++;
					}
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				}
				//accumulated_source = accumulated_target;
			} /* } else { } //if (hvlbufrate==0x10000) */
			// warning this deviates, it uses processing_consume_samples instead of tail...
			cpifaceSession->ringbufferAPI->processing_consume_samples (hvl_buf_pos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
			samples_committed += accumulated_target;
		} /* if (targetlength) */
	}

	{
		uint64_t delay = cpifaceSession->plrDevAPI->Idle();
		uint64_t new_ui = samples_committed - delay;
		if (new_ui > samples_lastui)
		{
			cpifaceSession->ringbufferAPI->tail_consume_samples (hvl_buf_pos, new_ui - samples_lastui);
			samples_lastui = new_ui;
		}
	}

	clipbusy--;
}

OCP_INTERNAL void hvlSetLoop (uint8_t s)
{
	hvl_doloop = s;
}

OCP_INTERNAL char hvlLooped (void)
{
	return hvl_looped == 3;
}

static void hvlSetSpeed (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	hvl_samples_per_row = hvlRate * 256 / (50 * sp);

	/* pause can cause slower than MAXIMUM_SLOW_DOWN, so we floor the value at that */
	if (hvl_samples_per_row > (hvlRate * MAXIMUM_SLOW_DOWN / 50))
	{
		hvl_samples_per_row = hvlRate * MAXIMUM_SLOW_DOWN / 50;
	}
}

static void hvlSetPitch (uint16_t sp)
{
	if (sp < 4)
		sp = 4;
	ht->ht_Frequency = hvlRate * 256 / sp;
	ht->ht_FreqF = (double)hvlRate * 256 / sp;
}

static void hvlSetVolume (void)
{
	volr = voll = vol * 4;
	if (bal < 0)
		voll = (voll * (64 + bal)) >> 6;
	else
		volr = (volr * (64 - bal)) >> 6;
}

static void hvlSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			hvlSetSpeed(val);
			break;
		case mcpMasterPitch:
			hvlSetPitch(val);
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			hvlSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			hvlSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			hvlSetVolume();
			break;
	}
}

static int hvlGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL void hvlRestartSong ()
{
	hvl_InitSubsong (ht, ht->ht_SongNum);
}

OCP_INTERNAL void hvlPrevSubSong ()
{
	if (ht->ht_SongNum)
	{
		ht->ht_SongNum--;
	}
	hvl_InitSubsong (ht, ht->ht_SongNum);
}

OCP_INTERNAL void hvlNextSubSong ()
{
	if (ht->ht_SongNum+1 <= ht->ht_SubsongNr)
	{
		ht->ht_SongNum++;
	}
	hvl_InitSubsong (ht, ht->ht_SongNum);
}

OCP_INTERNAL void hvlMute (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int m)
{
	cpifaceSession->MuteChannel[ch] = m;
	hvl_muted[ch] = m;
}

OCP_INTERNAL int hvlGetChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	int stereo = (opt&mcpGetSampleStereo)?1:0;
	uint32_t step = imuldiv(0x00010000, hvlRate, (signed)rate);
	int16_t *src;
	int pos1, pos2;
	int length1, length2;
	uint32_t posf = 0;

	cpifaceSession->ringbufferAPI->get_tail_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);

	src = hvl_buf_16chan + MAX_CHANNELS * 2 * pos1;

	while (len)
	{
		if (stereo)
		{
			*(s++) = src[ch*2+0];
			*(s++) = src[ch*2+1];
		} else {
			*(s++) = src[ch*2+0] + src[ch*2+1];
		}
		len--;

		posf += step;

		while (posf >= 0x00010000)
		{
			posf -= 0x00010000;

			src += 2 * MAX_CHANNELS;
			length1--;

			if (!length1)
			{
				length1 = length2;
				length2 = 0;
				src = hvl_buf_16chan + MAX_CHANNELS * 2 * pos2;
			}
			if (!length1)
			{
				memset (s, 0, (len<<stereo)<<2);
				return !!hvl_muted[ch];
			}
		}
	}
	return !!hvl_muted[ch];
}

OCP_INTERNAL void hvlGetStats (int *row, int *rows, int *order, int *orders, int *subsong, int *subsongs, int *tempo, int *speedmult)
{
	*row       = last_ht_NoteNr;
	*order     = last_ht_PosNr;
	*subsong   = last_ht_SongNum;
	*rows      = ht->ht_TrackLength;
	*orders    = ht->ht_PositionNr;
	*subsongs  = ht->ht_SubsongNr;
	*tempo     = last_ht_Tempo;
	*speedmult = last_ht_SpeedMultiplier;
}

OCP_INTERNAL int hvlOpenPlayer (const uint8_t *mem, size_t memlen, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	int retval;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	memset (ChanInfo, 0, sizeof (ChanInfo));
	hvl_InitReplayer ();

	hvlRate=0;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&hvlRate, &format, file, cpifaceSession))
	{
		return errPlay;
	}

	current_cpifaceSession = cpifaceSession;

	ht = hvl_LoadTune_memory (cpifaceSession, mem, memlen, 4, hvlRate);
	if (!ht)
	{
		retval = errFormStruc;
		goto error_out_plrDevAPI_Play;
	}

	if( !hvl_InitSubsong( ht, 0 ) )
	{
		retval = errFormStruc;
		goto error_out_tune;
	}

	last_ht_SongNum = 0;
	last_ht_NoteNr = 0;
	last_ht_PosNr = 0;
	last_ht_Tempo = 1;
	last_ht_SpeedMultiplier = 1;

	hvlbuffpos = 0x00000000;
	hvl_doloop = 0;
	hvl_looped = 0;
	samples_committed=0;
	samples_lastui=0;

	hvl_samples_per_row = hvlRate / 50;

	hvl_buf_stereo = malloc (sizeof (int16_t) * (ROW_BUFFERS + 2) * MAXIMUM_SLOW_DOWN * 2 * hvl_samples_per_row); /* The + 2 is on purpose, so we do not have to wrap when calling hvl_DecodeFrame(), and another to have enough space when buffer utilization is close to maximum */
	hvl_buf_16chan = malloc (sizeof (int16_t) * (ROW_BUFFERS + 2) * MAXIMUM_SLOW_DOWN * 2 * MAX_CHANNELS * hvl_samples_per_row);

	if ((!hvl_buf_stereo) && (!hvl_buf_16chan))
	{
		retval = errAllocMem;
		goto error_out_mem;
	}

	hvl_buf_pos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, (ROW_BUFFERS + 1) * MAXIMUM_SLOW_DOWN * hvl_samples_per_row);
	if (!hvl_buf_pos)
	{
		retval = errAllocMem;
		goto error_out_mem;
	}

	memset (hvl_muted, 0, sizeof (hvl_muted));

	memset (hvl_statbuffer, 0, sizeof (hvl_statbuffer));
	hvl_statbuffers_available = ROW_BUFFERS;

	memset (plInstUsed, 0, sizeof (plInstUsed));

	cpifaceSession->mcpSet = hvlSet;
	cpifaceSession->mcpGet = hvlGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP | mcpNormalizeCanSpeedPitchUnlock);

	return errOk;

	//if (hvl_buf_pos)
	//{
	//	cpifaceSession->ringbufferAPI->free (hvl_buf_pos);
	//	hvl_buf_pos = 0;
	//}
error_out_mem:
	free (hvl_buf_stereo);
	hvl_buf_stereo = 0;
	free (hvl_buf_16chan);
	hvl_buf_16chan = 0;
error_out_tune:
	if (ht)
	{
		hvl_FreeTune (ht);
		ht = 0;
	}
error_out_plrDevAPI_Play:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

	current_cpifaceSession = 0;

	return retval;
}

OCP_INTERNAL void hvlClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}

	if (hvl_buf_pos)
	{
		cpifaceSession->ringbufferAPI->free (hvl_buf_pos);
		hvl_buf_pos = 0;
	}

	free (hvl_buf_stereo);
	hvl_buf_stereo = 0;

	free (hvl_buf_16chan);
	hvl_buf_16chan = 0;

	if (ht)
	{
		hvl_FreeTune (ht);
		ht = 0;
	}

	current_cpifaceSession = 0;
}
