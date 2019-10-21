/* OpenCP Module Player
 * copyright (c) 2019 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "hvlplay.h"
#include "cpiface/cpiface.h" /* merge in from hvlpinst.c, to compensate for buffer-delay */
#include "dev/deviplay.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "loader.h"
#include "player.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"

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

static struct hvl_statbuffer_t hvl_statbuffer[ROW_BUFFERS] = {0}; // half a second */
static int hvl_statbuffers_available = 0;

struct hvl_chaninfo ChanInfo[MAX_CHANNELS];

struct hvl_tune *ht = 0;
static int hvl_samples_per_row;

static int16_t *hvl_buf_stereo;
static int16_t *hvl_buf_16chan;

static struct ringbuffer_t *hvl_buf_pos;
/*             tail              processing        head
 *  (free)      | already in devp | ready to stream |   (free)
 *
 *          As the tail catches up, we know data has been played, and we update our stats on the screen
 */

/* devp pre-buffer zone */
static int16_t *buf16 = 0; /* here we dump out data before it goes live */

/* devp buffer zone */
static volatile uint32_t bufpos; /* devp write head location */
static uint32_t buflen; /* devp buffer-size in samples */
static volatile uint32_t kernpos; /* devp read/tail location - used to track when to show buf8_states */
static void *plrbuf; /* the devp buffer */
static int stereo; /* boolean */
static int bit16; /* boolean */
static int signedout; /* boolean */
static int reversestereo; /* boolean */
static int active=0;
static volatile int PauseSamples;

static uint32_t hvlbuffpos;
static uint32_t hvlPauseRate;
static int hvl_doloop;
static int hvl_looped;
static int hvl_inpause;

/*static unsigned long amplify;  TODO */
static unsigned long voll,volr;
static int pan;
static int srnd;

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

void hvlGetChanInfo (int chan, struct hvl_chaninfo *ci)
{
	memcpy (ci, ChanInfo + chan, sizeof (*ci));
}

void hvlGetChanVolume (int chan, int *l, int *r)
{
	int16_t *src;
	int pos1, pos2;
	int length1, length2;
	int samples;

	*l = 0;
	*r = 0;

	ringbuffer_get_tail_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);

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
			if (plSelCh == i)
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

extern void __attribute__ ((visibility ("internal"))) hvlIdler (void)
{
	while (hvl_statbuffers_available) /* we only prepare more data if hvl_statbuffers_available is non-zero. This gives about 0.5 seconds worth of sample-data */
	{
		int i, j;

		int pos1, pos2;
		int length1, length2;
		int16_t *src;
		int16_t *dst;

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
			if (ht->ht_Voices[j].vc_Instrument)
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
			hvl_statbuffer[i].ChanInfo[j].pan        = voice->vc_Pan;

			hvl_statbuffer[i].ChanInfo[j].pitchslide = voice->vc_PeriodSlidePeriod?3:((voice->vc_PeriodSlideSpeed>0)?1:((voice->vc_PeriodSlideSpeed<0)?2:0));
			hvl_statbuffer[i].ChanInfo[j].waveform   = voice->vc_Waveform;

			hvl_statbuffer[i].ChanInfo[j].volslide   = (voice->vc_VolumeSlideUp?1:0) | (voice->vc_VolumeSlideDown?2:0);
			hvl_statbuffer[i].ChanInfo[j].fx         = Step->stp_FX;
			hvl_statbuffer[i].ChanInfo[j].fxparam    = Step->stp_FXParam;
			hvl_statbuffer[i].ChanInfo[j].fxB        = Step->stp_FXb;
			hvl_statbuffer[i].ChanInfo[j].fxBparam   = Step->stp_FXbParam;
		}

		ringbuffer_get_head_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);

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
				left  += *(src++);
				right += *(src++);
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
		ringbuffer_add_tail_callback_samples (hvl_buf_pos, 0, hvl_statbuffer_callback_from_hvlbuf, hvl_statbuffer + i);

		/* Adding hvl_samples_per_row to our devp-mirrored buffer */

		ringbuffer_head_add_samples (hvl_buf_pos, hvl_samples_per_row);

		hvl_statbuffers_available--;
	}
}

static void hvlUpdateKernPos (void)
{
	uint32_t delta, newpos;
	newpos = plrGetPlayPos() >> (stereo+bit16);
	delta = (buflen + newpos - kernpos) % buflen;

	if (PauseSamples)
	{
		if (delta >= PauseSamples)
		{
			delta -= PauseSamples;
			PauseSamples = 0;
		} else {
			PauseSamples -= delta;
			delta = 0;
		}
	}

	if (delta)
	{
		/* devp-buffer used completed playing DELTA amounts of samples */
		ringbuffer_tail_consume_samples (hvl_buf_pos, delta);
	}

	kernpos = newpos;
}

extern void __attribute__ ((visibility ("internal"))) hvlIdle (void)
{
	uint32_t bufdelta;
	uint32_t pass2;
	static volatile int clipbusy=0;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	/* "fill" up our buffers */
	hvlIdler();

	hvlUpdateKernPos ();

	{
		uint32_t buf_read_pos; /* bufpos is last given buf_write_pos */

		buf_read_pos = plrGetBufPos() >> (stereo+bit16);

		bufdelta = ( buflen + buf_read_pos - bufpos )%buflen;
	}

	if (!bufdelta)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}

	if (hvl_inpause)
	{
		/* If we are in pause, we fill buffer with the correct type of zeroes */
		/* But we also make sure that the buffer-fill is never more than 0.1s, making it reponsive */
		uint32_t min_bufdelta;

		if (buflen > plrRate/10)
		{
			min_bufdelta = buflen - (plrRate/10);
		} else {
			min_bufdelta = 0;
		}

		if (bufdelta > min_bufdelta)
		{
			bufdelta = bufdelta - min_bufdelta;

			if ((bufpos+bufdelta)>buflen)
				pass2=bufpos+bufdelta-buflen;
			else
				pass2=0;

			if (bit16)
			{
				plrClearBuf((uint16_t *)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, signedout);
				if (pass2)
					plrClearBuf((uint16_t *)plrbuf, pass2<<stereo, signedout);
			} else {
				plrClearBuf(buf16, bufdelta<<stereo, signedout);
				plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), (uint16_t *)buf16, (bufdelta-pass2)<<stereo);
				if (pass2)
					plr16to8((uint8_t *)plrbuf, (uint16_t *)buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
			}
			bufpos+=bufdelta;
			if (bufpos>=buflen)
				bufpos-=buflen;

			/* Put "data" into PauseSamples, instead of hvl_buf_pos please */
			PauseSamples += bufdelta;
			//ringbuffer_processing_consume_samples (hvl_buf_pos, bufdelta); /* add this rate buf16_filled == tail_used */
		}
	} else {
		int pos1, length1, pos2, length2;
		int i;
		int buf16_filled = 0;

		/* how much data is available to transfer into devp.. we are using a ringbuffer, so we might receive two fragments */
		ringbuffer_get_processing_samples (hvl_buf_pos, &pos1, &length1, &pos2, &length2);

		if (hvlPauseRate == 0x00010000)
		{
			int16_t *t = buf16;

			if (bufdelta > (length1+length2))
			{
				bufdelta=(length1+length2);
				hvl_looped |= 2;

			} else {
				hvl_looped &= ~2;
			}

			for (buf16_filled=0; buf16_filled<bufdelta; buf16_filled++)
			{
				int16_t rs, ls;

				if (!length1)
				{
					pos1 = pos2;
					length1 = length2;
					pos2 = 0;
					length2 = 0;
				}

				assert (length1);

				rs = hvl_buf_stereo[(pos1<<1)    ];
				ls = hvl_buf_stereo[(pos1<<1) + 1];

				PANPROC;

				*(t++) = rs;
				*(t++) = ls;

				pos1++;
				length1--;
			}

			ringbuffer_processing_consume_samples (hvl_buf_pos, buf16_filled); /* add this rate buf16_filled == tail_used */
		} else {
			/* We are going to perform cubic interpolation of rate conversion, used for cool pause-slow-down... this bit is tricky */
			unsigned int accumulated_progress = 0;

			hvl_looped &= ~2;

			for (buf16_filled=0; buf16_filled<bufdelta; buf16_filled++)
			{
				uint32_t wpm1, wp0, wp1, wp2;
				int32_t rc0, rc1, rc2, rc3, rvm1,rv1,rv2;
				int32_t lc0, lc1, lc2, lc3, lvm1,lv1,lv2;
				unsigned int progress;
				int16_t rs, ls;

				/* will the interpolation overflow? */
				if ((length1+length2) <= 3)
				{
					hvl_looped |= 2;
					break;
				}
				/* will we overflow the wavebuf if we advance? */
				if ((length1+length2) < ((hvlPauseRate+hvlbuffpos)>>16))
				{
					hvl_looped |= 2;
					break;
				}

				switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
				{
					case 1:
						wpm1 = pos1;
						wp0  = pos2;
						wp1  = pos2+1;
						wp2  = pos2+2;
						break;
					case 2:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos2;
						wp2  = pos2+1;
						break;
					case 3:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos1+2;
						wp2  = pos2;
						break;
					default:
						wpm1 = pos1;
						wp0  = pos1+1;
						wp1  = pos1+2;
						wp2  = pos1+3;
						break;
				}


				rvm1 = (uint16_t)hvl_buf_stereo[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
				lvm1 = (uint16_t)hvl_buf_stereo[(wpm1<<1)+1]^0x8000;
				 rc0 = (uint16_t)hvl_buf_stereo[(wp0<<1)+0]^0x8000;
				 lc0 = (uint16_t)hvl_buf_stereo[(wp0<<1)+1]^0x8000;
				 rv1 = (uint16_t)hvl_buf_stereo[(wp1<<1)+0]^0x8000;
				 lv1 = (uint16_t)hvl_buf_stereo[(wp1<<1)+1]^0x8000;
				 rv2 = (uint16_t)hvl_buf_stereo[(wp2<<1)+0]^0x8000;
				 lv2 = (uint16_t)hvl_buf_stereo[(wp2<<1)+1]^0x8000;

				rc1 = rv1-rvm1;
				rc2 = 2*rvm1-2*rc0+rv1-rv2;
				rc3 = rc0-rvm1-rv1+rv2;
				rc3 =  imulshr16(rc3,hvlbuffpos);
				rc3 += rc2;
				rc3 =  imulshr16(rc3,hvlbuffpos);
				rc3 += rc1;
				rc3 =  imulshr16(rc3,hvlbuffpos);
				rc3 += rc0;
				if (rc3<0)
					rc3=0;
				if (rc3>65535)
					rc3=65535;

				lc1 = lv1-lvm1;
				lc2 = 2*lvm1-2*lc0+lv1-lv2;
				lc3 = lc0-lvm1-lv1+lv2;
				lc3 =  imulshr16(lc3,hvlbuffpos);
				lc3 += lc2;
				lc3 =  imulshr16(lc3,hvlbuffpos);
				lc3 += lc1;
				lc3 =  imulshr16(lc3,hvlbuffpos);
				lc3 += lc0;
				if (lc3<0)
					lc3=0;
				if (lc3>65535)
					lc3=65535;

				rs = rc3 ^ 0x8000;
				ls = lc3 ^ 0x8000;

				PANPROC;

				buf16[(buf16_filled<<1)+0] = rs;
				buf16[(buf16_filled<<1)+1] = ls;

				hvlbuffpos+=hvlPauseRate;
				progress = hvlbuffpos>>16;
				hvlbuffpos &= 0xffff;

				accumulated_progress += progress;

				/* did we wrap? if so, progress up to the wrapping point */
				if (progress >= length1)
				{
					progress -= length1;
					pos1 = pos2;
					length1 = length2;
					pos2 = 0;
					length2 = 0;
				}
				if (progress)
				{
					pos1 += progress;
					length1 -= progress;
				}
			}

			/* We are slowing down, so accumulated_progress is ALWAYS bigger than buf16_filled */
			PauseSamples += buf16_filled - accumulated_progress;
			ringbuffer_processing_consume_samples (hvl_buf_pos, accumulated_progress);
		}

		bufdelta=buf16_filled;

		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		bufdelta-=pass2;

		if (bit16)
		{
			if (stereo)
			{
				int16_t *p=(int16_t *)plrbuf+2*bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0];
						p[1]=b[1];
						p+=2;
						b+=2;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0];
						p[1]=b[1];
						p+=2;
						b+=2;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0]^0x8000;
						p[1]=b[1]^0x8000;
						p+=2;
						b+=2;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0]^0x8000;
						p[1]=b[1]^0x8000;
						p+=2;
						b+=2;
					}
				}
			} else {
				int16_t *p=(int16_t *)plrbuf+bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0];
						p++;
						b++;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0];
						p++;
						b++;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0]^0x8000;
						p++;
						b++;
					}
					p=(int16_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0]^0x8000;
						p++;
						b++;
					}
				}
			}
		} else {
			if (stereo)
			{
				uint8_t *p=(uint8_t *)plrbuf+2*bufpos;
				uint8_t *b=(uint8_t *)buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1];
						p[1]=b[3];
						p+=2;
						b+=4;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1];
						p[1]=b[3];
						p+=2;
						b+=4;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1]^0x80;
						p[1]=b[3]^0x80;
						p+=2;
						b+=4;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1]^0x80;
						p[1]=b[3]^0x80;
						p+=2;
						b+=4;
					}
				}
			} else {
				uint8_t *p=(uint8_t *)plrbuf+bufpos;
				int16_t *b=buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=(b[0]+b[1])>>9;
						p++;
						b+=2;
					}

					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=(b[0]+b[1])>>9;
						p++;
						b+=2;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=((b[0]+b[1])>>9)^0x80;
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=((b[0]+b[1])>>9)^0x80;
						p++;
						b+=2;
					}
				}
			}
		}
		bufpos+=buf16_filled;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

	if (plrIdle)
		plrIdle();

	clipbusy--;
}

struct hvl_tune __attribute__ ((visibility ("internal"))) *hvlOpenPlayer (const uint8_t *mem, size_t memlen)
{
	hvl_InitReplayer ();

	plrSetOptions(44100, (PLR_SIGNEDOUT|PLR_16BIT)|PLR_STEREO);

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

	ht = hvl_LoadTune_memory (mem, memlen, 4, plrRate);
	if (!ht)
	{
		goto error_out;
	}

	if( !hvl_InitSubsong( ht, 0 ) )
	{
		goto error_out;
	}

	last_ht_SongNum = 0;
	last_ht_NoteNr = 0;
	last_ht_PosNr = 0;
   	last_ht_Tempo = 1;
	last_ht_SpeedMultiplier = 1;

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize))
	{
		goto error_out;
	}
	bufpos=0;
	kernpos=0;
	hvlbuffpos = 0x00000000;
	PauseSamples = 0;
	hvl_inpause = 0;
	active = 1;
	hvl_doloop = 0;
	hvlPauseRate = 0x00010000;

	hvl_samples_per_row = plrRate / 50;

	buf16 = malloc (sizeof (int16_t) * buflen * 2);
	hvl_buf_stereo = malloc (sizeof (int16_t) * (ROW_BUFFERS + 2) * MAXIMUM_SLOW_DOWN * 2 * hvl_samples_per_row); /* The + 2 is on purpose, so we do not have to wrap when calling hvl_DecodeFrame(), and another to have enough space when buffer utilization is close to maximum */
	hvl_buf_16chan = malloc (sizeof (int16_t) * (ROW_BUFFERS + 2) * MAXIMUM_SLOW_DOWN * 2 * MAX_CHANNELS * hvl_samples_per_row);

	if ((!buf16) && (!hvl_buf_stereo) && (!hvl_buf_16chan))
	{
		goto error_out;
	}

	hvl_buf_pos = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, (ROW_BUFFERS + 1) * MAXIMUM_SLOW_DOWN * hvl_samples_per_row);
	if (!hvl_buf_pos)
	{
		goto error_out;
	}

	bzero (hvl_statbuffer, sizeof (hvl_statbuffer));
	hvl_statbuffers_available = ROW_BUFFERS;

	bzero (plInstUsed, sizeof (plInstUsed));

	if (!pollInit(hvlIdle))
	{
		goto error_out;
	}
	active = 3;

	return ht;

error_out:
	hvlClosePlayer();

	return 0;
}

void __attribute__ ((visibility ("internal"))) hvlClosePlayer (void)
{
	if (active & 2)
	{
		pollClose();
	}
	if (active & 1)
	{
		plrClosePlayer();
		plrbuf = 0;
	}
	active = 0;

	if (hvl_buf_pos)
	{
		ringbuffer_free (hvl_buf_pos);
		hvl_buf_pos = 0;
	}

	if (buf16)
	{
		free (buf16);
		buf16 = 0;
	}

	if (hvl_buf_stereo)
	{
		free (hvl_buf_stereo);
		hvl_buf_stereo = 0;
	}

	if (hvl_buf_16chan)
	{
		free (hvl_buf_16chan);
		hvl_buf_16chan = 0;
	}

	if (ht)
	{
		hvl_FreeTune (ht);
		ht = 0;
	}
}

void __attribute__ ((visibility ("internal"))) hvlSetLoop (uint8_t s)
{
	hvl_doloop = s;
}

char __attribute__ ((visibility ("internal"))) hvlLooped (void)
{
	return hvl_looped == 3;
}

void __attribute__ ((visibility ("internal"))) hvlPause (uint8_t p)
{
	hvl_inpause = p;
}

void __attribute__ ((visibility ("internal"))) hvlSetAmplify (uint32_t amp)
{
	//fprintf (stderr, "TODO hvlSetAmplify(0x%08x)\n", (int)amp);
}

void __attribute__ ((visibility ("internal"))) hvlSetSpeed (uint16_t sp)
{
	hvl_samples_per_row = plrRate * 256 / (50 * sp);

	/* pause can cause slower than MAXIMUM_SLOW_DOWN, so we floor the value at that */
	if (hvl_samples_per_row > (plrRate * MAXIMUM_SLOW_DOWN / 50))
	{
		hvl_samples_per_row = plrRate * MAXIMUM_SLOW_DOWN / 50;
	}
}

void __attribute__ ((visibility ("internal"))) hvlSetPitch (uint16_t sp)
{
	ht->ht_Frequency = plrRate * 256 / sp;
	ht->ht_FreqF = (double)plrRate * 256 / sp;
}

void __attribute__ ((visibility ("internal"))) hvlSetPausePitch (uint32_t sp)
{
	assert (sp);
	assert (sp <= 0x00010000);
	hvlPauseRate = sp;
}

void __attribute__ ((visibility ("internal"))) hvlSetVolume (uint8_t vol_, int8_t bal_, int8_t pan_, uint8_t opt)
{
	pan=pan_;
	if (reversestereo)
	{
		pan = -pan;
	}
	volr=voll=vol_*4;
	if (bal_<0)
		voll=(voll*(64+bal_))>>6;
	else
		volr=(volr*(64-bal_))>>6;
	srnd=opt;
}

void __attribute__ ((visibility ("internal"))) hvlPrevSubSong ()
{
	if (ht->ht_SongNum)
	{
		ht->ht_SongNum--;
	}
	hvl_InitSubsong (ht, ht->ht_SongNum);
}

void __attribute__ ((visibility ("internal"))) hvlNextSubSong ()
{
	if (ht->ht_SongNum+1 <= ht->ht_SubsongNr)
	{
		ht->ht_SongNum++;
	}
	hvl_InitSubsong (ht, ht->ht_SongNum);
}

void __attribute__ ((visibility ("internal"))) hvlGetStats (int *row, int *rows, int *order, int *orders, int *subsong, int *subsongs, int *tempo, int *speedmult)
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

