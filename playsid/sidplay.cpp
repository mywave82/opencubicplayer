/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '11-'20 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
 *
 * SIDPlay - SID file player based on Michael Schwendt's SIDPlay routines
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
 *
 * revision history: (please note changes here)
 *  -kb980717  Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 *  -ss04????  Stian Skjelstad <stian@nixia.no>
 *    -ported the assembler to gcc
 *  -ss040908  Stian Skjelstad <stian@nixia.no>
 *    -made assembler optimize safe
 */

extern "C"
{
#include "sidplayfp-config/config.h"

/* sidplayfp compilation some defines set, that we need to remove again */
#ifdef DPACKAGE_NAME
#undef DPACKAGE_NAME
#endif

#ifdef VERSION
#undef VERSION
#endif

#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#include "../config.h"
}

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include "../types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/mixclip.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
}

#include "sidplay.h"
#include "libsidplayfp-api.h"

#define MAXIMUM_SLOW_DOWN 32
#define ROW_BUFFERS 25 /* half a second */

static libsidplayfp::ConsolePlayer *mySidPlayer;
static SidTuneInfo const *mySidTuneInfo;
static int sid_samples_per_row;

typedef struct
{
	uint8_t gatestoggle[3]; /* bit 0, 1 and 2 describes the toggles performed, 3 SID chips */
	uint8_t syncstoggle[3]; /* bit 0, 1 and 2 describes the toggles performed, 3 SID chips */
	uint8_t teststoggle[3];	/* bit 0, 1 and 2 describes the toggles performed, 3 SID chips */
	uint8_t registers[3][0x20]; /* current register values, 3 SID chips */
	uint8_t volumes[3][3];

	uint8_t in_use;
} SidStatBuffer_t;

static SidStatBuffer_t last; /* current register values, 3 SID chips */

static SidStatBuffer_t SidStatBuffers[ROW_BUFFERS] = {0}; // half a second */
static int SidStatBuffers_available = 0;

static int16_t *sid_buf_stereo; /* stereo interleaved */
static int16_t *sid_buf_4x3[3]; /* 4-chan interleaved, 3 SID chips */

static struct ringbuffer_t *sid_buf_pos;
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
static volatile int PauseSamples;

static uint32_t sidbuffpos;
static uint32_t sidPauseRate;

/*static unsigned long amplify;  TODO */
static unsigned long voll,volr;
static int pan;
static int srnd;

static char sid_inpause;

static int SidCount;

static volatile int clipbusy=0;

static uint8_t sidMuted[3*3];

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

static void SidStatBuffers_callback_from_sidbuf (void *arg, int samples_ago)
{
	SidStatBuffer_t *state = (SidStatBuffer_t *)arg;
	int i;

	last = *state;


	state->in_use = 0;
	SidStatBuffers_available++;
}

extern void __attribute__ ((visibility ("internal"))) sidIdler (void)
{
	while (SidStatBuffers_available) /* we only prepare more data if SidStatBuffers_available is non-zero. This gives about 0.5 seconds worth of sample-data */
	{
		int i, j;

		int pos1, pos2;
		int length1, length2;
		int16_t *dst;

		for (i=0; i < ROW_BUFFERS; i++)
		{
			if (SidStatBuffers[i].in_use)
			{
				continue;
			}
			break;
		}
		assert (i != ROW_BUFFERS);

		ringbuffer_get_head_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

		/* We can fit length1+length2 samples into out devp-mirrored buffer */

		assert ((length1 + length2) >= sid_samples_per_row);

		if (length1 >= sid_samples_per_row)
		{
			std::vector<int16_t *> raw {sid_buf_4x3[0] + (pos1<<2),
			                            sid_buf_4x3[1] + (pos1<<2),
			                            sid_buf_4x3[2] + (pos1<<2)};
			mySidPlayer->iterateaudio (sid_buf_stereo + (pos1<<1), sid_samples_per_row, &raw);
		} else {
			std::vector<int16_t *> raw1 {sid_buf_4x3[0] + (pos1<<2),
			                             sid_buf_4x3[1] + (pos1<<2),
			                             sid_buf_4x3[2] + (pos1<<2)};
			mySidPlayer->iterateaudio (sid_buf_stereo + (pos1<<1), length1, &raw1);

			std::vector<int16_t *> raw2 {sid_buf_4x3[0] + (pos2<<2),
			                             sid_buf_4x3[1] + (pos2<<2),
			                             sid_buf_4x3[2] + (pos2<<2)};
			mySidPlayer->iterateaudio (sid_buf_stereo + (pos2<<1), sid_samples_per_row - length1, &raw2);
		}
		for (j=0; j < SidCount; j++)
		{
			uint8_t *registers = NULL;
			mySidPlayer->getSidStatus (j,
			                           SidStatBuffers[i].gatestoggle[j],
			                           SidStatBuffers[i].syncstoggle[j],
			                           SidStatBuffers[i].teststoggle[j],
			                           &registers,
			                           SidStatBuffers[i].volumes[j][0],
			                           SidStatBuffers[i].volumes[j][1],
			                           SidStatBuffers[i].volumes[j][2]);
			memcpy (SidStatBuffers[i].registers[j], registers, 0x20);
		}

		SidStatBuffers[i].in_use = 1;
		ringbuffer_add_tail_callback_samples (sid_buf_pos, 0, SidStatBuffers_callback_from_sidbuf, SidStatBuffers + i);

		/* Adding sid_samples_per_row to our devp-mirrored buffer */

		ringbuffer_head_add_samples (sid_buf_pos, sid_samples_per_row);

		SidStatBuffers_available--;
	}
}

static void sidUpdateKernPos (void)
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
		ringbuffer_tail_consume_samples (sid_buf_pos, delta);
	}

	kernpos = newpos;
}

void __attribute__ ((visibility ("internal"))) sidIdle(void)
{
	uint32_t bufdelta, newpos;
	uint32_t pass2;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	/* "fill" up our buffers */
	sidIdler();

	sidUpdateKernPos ();

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

	if (sid_inpause)
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

			/* Put "data" into PauseSamples, instead of sid_buf_pos please */
			PauseSamples += bufdelta;
			//ringbuffer_processing_consume_samples (sid_buf_pos, bufdelta); /* add this rate buf16_filled == tail_used */
		}
	} else {
		int pos1, length1, pos2, length2;
		int i;
		int buf16_filled = 0;

		/* how much data is available to transfer into devp.. we are using a ringbuffer, so we might receive two fragments */
		ringbuffer_get_processing_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

		if (sidPauseRate == 0x00010000)
		{
			int16_t *t = buf16;

			if (bufdelta > (length1+length2))
			{
				bufdelta=(length1+length2);
				//sid_looped |= 2;

			} else {
				//sid_looped &= ~2;
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

				rs = sid_buf_stereo[(pos1<<1)    ];
				ls = sid_buf_stereo[(pos1<<1) + 1];

				PANPROC;

				*(t++) = rs;
				*(t++) = ls;

				pos1++;
				length1--;
			}

			ringbuffer_processing_consume_samples (sid_buf_pos, buf16_filled); /* add this rate buf16_filled == tail_used */
		} else {
			/* We are going to perform cubic interpolation of rate conversion, used for cool pause-slow-down... this bit is tricky */
			unsigned int accumulated_progress = 0;

			// sid_looped &= ~2;

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
					//sid_looped |= 2;
					break;
				}
				/* will we overflow the wavebuf if we advance? */
				if ((length1+length2) < ((sidPauseRate+sidbuffpos)>>16))
				{
					//sid_looped |= 2;
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


				rvm1 = (uint16_t)sid_buf_stereo[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
				lvm1 = (uint16_t)sid_buf_stereo[(wpm1<<1)+1]^0x8000;
				 rc0 = (uint16_t)sid_buf_stereo[(wp0<<1)+0]^0x8000;
				 lc0 = (uint16_t)sid_buf_stereo[(wp0<<1)+1]^0x8000;
				 rv1 = (uint16_t)sid_buf_stereo[(wp1<<1)+0]^0x8000;
				 lv1 = (uint16_t)sid_buf_stereo[(wp1<<1)+1]^0x8000;
				 rv2 = (uint16_t)sid_buf_stereo[(wp2<<1)+0]^0x8000;
				 lv2 = (uint16_t)sid_buf_stereo[(wp2<<1)+1]^0x8000;

				rc1 = rv1-rvm1;
				rc2 = 2*rvm1-2*rc0+rv1-rv2;
				rc3 = rc0-rvm1-rv1+rv2;
				rc3 =  imulshr16(rc3,sidbuffpos);
				rc3 += rc2;
				rc3 =  imulshr16(rc3,sidbuffpos);
				rc3 += rc1;
				rc3 =  imulshr16(rc3,sidbuffpos);
				rc3 += rc0;
				if (rc3<0)
					rc3=0;
				if (rc3>65535)
					rc3=65535;

				lc1 = lv1-lvm1;
				lc2 = 2*lvm1-2*lc0+lv1-lv2;
				lc3 = lc0-lvm1-lv1+lv2;
				lc3 =  imulshr16(lc3,sidbuffpos);
				lc3 += lc2;
				lc3 =  imulshr16(lc3,sidbuffpos);
				lc3 += lc1;
				lc3 =  imulshr16(lc3,sidbuffpos);
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

				sidbuffpos+=sidPauseRate;
				progress = sidbuffpos>>16;
				sidbuffpos &= 0xffff;

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
			ringbuffer_processing_consume_samples (sid_buf_pos, accumulated_progress);
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

#warning We can probably REMOVE mixClipAlt2 soon globally

#if 0
	int quietlen=0;

	bufdelta-=quietlen;

	if (bufdelta)
	{
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;

		plrClearBuf(buf16, bufdelta*2, 1);

    //sidplay nach buf16

		sidEmuFillBuffer(*myEmuEngine,*mySidTune,buf16,bufdelta<<(stereo+1));

		if (stereo && srnd)
			for (uint32_t i=0; i<bufdelta; i++)
				buf16[2*i]^=0xFFFF;

		if (bit16)
		{
			if (stereo)
			{
				mixClipAlt2((uint16_t *)plrbuf+bufpos*2, buf16, bufdelta-pass2, cliptabl);
				mixClipAlt2((uint16_t *)plrbuf+bufpos*2+1, buf16+1, bufdelta-pass2, cliptabr);
				if (pass2)
				{
					mixClipAlt2((uint16_t *)plrbuf, buf16+2*(bufdelta-pass2), pass2, cliptabl);
					mixClipAlt2((uint16_t *)plrbuf+1, buf16+2*(bufdelta-pass2)+1, pass2, cliptabr);
				}
			} else {
				mixClipAlt((uint16_t *)plrbuf+bufpos, buf16, bufdelta-pass2, cliptabl);
				if (pass2)
					mixClipAlt((uint16_t *)plrbuf, buf16+bufdelta-pass2, pass2, cliptabl);
			}
		} else {
			if (stereo)
			{
				mixClipAlt2(buf16, buf16, bufdelta, cliptabl);
				mixClipAlt2(buf16+1, buf16+1, bufdelta, cliptabr);
			} else
				mixClipAlt(buf16, buf16, bufdelta, cliptabl);
			plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t *)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	bufdelta=quietlen;
	if (bufdelta)
	{
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		if (bit16)
		{
			plrClearBuf((unsigned short*)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, !signedout);
			if (pass2)
				plrClearBuf((unsigned short*)plrbuf, pass2<<stereo, !signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, !signedout);
			plr16to8((unsigned char*)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((unsigned char*)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

	if (plrIdle)
		plrIdle();
#endif

	clipbusy--;
}

#if 0
static void updateconf()
{
	clipbusy++;
	myEmuEngine->setConfig(*myEmuConfig);
	clipbusy--;
}
#endif


int __attribute__ ((visibility ("internal"))) sidNumberOfChips(void)
{
	return SidCount;
}

int __attribute__ ((visibility ("internal"))) sidNumberOfComments(void)
{
	return mySidTuneInfo->numberOfCommentStrings();
}

int __attribute__ ((visibility ("internal"))) sidNumberOfInfos(void)
{
	return mySidTuneInfo->numberOfInfoStrings();
}

const char __attribute__ ((visibility ("internal"))) *sidInfoString(int i)
{
	return mySidTuneInfo->infoString(i);
}

const char __attribute__ ((visibility ("internal"))) *sidCommentString(int i)
{
	return mySidTuneInfo->commentString(i);
}

const char __attribute__ ((visibility ("internal"))) *sidFormatString(void)
{
	return mySidTuneInfo->formatString();
}

const char __attribute__ ((visibility ("internal"))) *sidROMDescKernal(void)
{
	return mySidPlayer->kernalDesc();
}

const char __attribute__ ((visibility ("internal"))) *sidROMDescBasic(void)
{
	return mySidPlayer->basicDesc();
}

const char __attribute__ ((visibility ("internal"))) *sidROMDescChargen(void)
{
	return mySidPlayer->chargenDesc();
}

const float __attribute__ ((visibility ("internal"))) sidGetCPUSpeed(void)
{
	return mySidPlayer->getMainCpuSpeed();
}

const char __attribute__ ((visibility ("internal"))) *sidGetVICIIModelString(void)
{
	return libsidplayfp::VICIImodel_ToString(mySidPlayer->getVICIImodel());
}

const char __attribute__ ((visibility ("internal"))) *sidGetCIAModelString(void)
{
	return mySidPlayer->getCIAmodel();
}

const char __attribute__ ((visibility ("internal"))) *sidChipModel(int i)
{
	return libsidplayfp::sidModel_ToString(mySidPlayer->getSIDmodel(i));
}

uint16_t __attribute__ ((visibility ("internal"))) sidChipAddr(int i)
{
	return mySidPlayer->getSIDaddr(i);
}

const char __attribute__ ((visibility ("internal"))) *sidTuneStatusString(void)
{
	return mySidPlayer->getTuneStatusString();
}

const char __attribute__ ((visibility ("internal"))) *sidTuneInfoClockSpeedString(void)
{
	return libsidplayfp::tuneInfo_clockSpeed_toString(mySidPlayer->getTuneInfoClockSpeed());
}

unsigned char __attribute__ ((visibility ("internal"))) sidOpenPlayer(FILE *f)
{
	if (!plrPlay)
	{
		return 0;
	}

	int playrate=cfGetProfileInt("commandline_s", "r", cfGetProfileInt2(cfSoundSec, "sound", "mixrate", 44100, 10), 10);
	if (playrate<66)
	{
		if (playrate%11)
		{
			playrate*=1000;
		} else {
			playrate=playrate*11025/11;
		}
	}
	plrSetOptions(playrate, PLR_STEREO|PLR_16BIT);

	fseek(f, 0, SEEK_END);
	const int length=ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char *buf=new unsigned char[length];
	if (fread(buf, length, 1, f)!=1)
	{
		fprintf(stderr, __FILE__": fread failed #1\n");
		return 0;
	}

	mySidPlayer = new libsidplayfp::ConsolePlayer(plrRate);
	if (!mySidPlayer->load (buf, length))
	{
		fprintf (stderr, "[playsid]: loading file failed\n");
		delete mySidPlayer; mySidPlayer = NULL;
		delete [] buf;
		return 0;
	}
	delete [] buf;
	mySidTuneInfo = mySidPlayer->getInfo();

	SidCount = mySidPlayer->getSidCount();

	if (!mySidTuneInfo)
	{
		fprintf (stderr, "[playsid]: retrieve info from file failed\n");
		delete mySidPlayer; mySidPlayer = NULL;
		return 0;
	}

	int BufSize = plrBufSize;
	if (BufSize > 40)
	{
		BufSize = 40;
	}
	if (!plrOpenPlayer(&plrbuf, &buflen, BufSize * plrRate / 1000))
	{
		delete mySidPlayer; mySidPlayer = NULL;
		                    mySidTuneInfo = NULL;
		return 0;
	}

	sid_samples_per_row = plrRate / 50;

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);
	srnd=0;

#if 0
	myEmuEngine->getConfig(*myEmuConfig);

	myEmuConfig->bitsPerSample=SIDEMU_16BIT;
	myEmuConfig->sampleFormat=SIDEMU_UNSIGNED_PCM;
	myEmuConfig->channels=stereo?SIDEMU_STEREO:SIDEMU_MONO;
	myEmuConfig->sidChips=1;

	myEmuConfig->volumeControl=SIDEMU_FULLPANNING;
	myEmuConfig->autoPanning=SIDEMU_CENTEREDAUTOPANNING;

	myEmuConfig->mos8580=0;
	myEmuConfig->measuredVolume=0;
	myEmuConfig->emulateFilter=1;
	myEmuConfig->filterFs=SIDEMU_DEFAULTFILTERFS;
	myEmuConfig->filterFm=SIDEMU_DEFAULTFILTERFM;
	myEmuConfig->filterFt=SIDEMU_DEFAULTFILTERFT;
	myEmuConfig->memoryMode=MPU_BANK_SWITCHING;
	myEmuConfig->clockSpeed=SIDTUNE_CLOCK_PAL;
	myEmuConfig->forceSongSpeed=0;
	myEmuConfig->digiPlayerScans=10;

	myEmuEngine->setConfig(*myEmuConfig);
#endif

	memset(sidMuted, 0, sizeof (sidMuted));
	sid_inpause=0;

	buf16=new int16_t [buflen*2];
	sid_buf_stereo = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 2 * sid_samples_per_row];
	sid_buf_4x3[0] = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 4 * sid_samples_per_row];
	sid_buf_4x3[1] = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 4 * sid_samples_per_row];
	sid_buf_4x3[2] = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 4 * sid_samples_per_row];
	if ((!buf16) || (!sid_buf_4x3[0]) || (!sid_buf_4x3[1]) || (!sid_buf_4x3[2]))
	{
		plrClosePlayer();
		return 0;
	}

	sid_buf_pos = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, ROW_BUFFERS * MAXIMUM_SLOW_DOWN * sid_samples_per_row);
	if (!sid_buf_pos)
	{
		plrClosePlayer();
		return 0;
	}

	bzero (SidStatBuffers, sizeof (SidStatBuffers));
	SidStatBuffers_available = ROW_BUFFERS;

	bufpos=0;
	kernpos=0;
	sidbuffpos = 0x00000000;
	PauseSamples = 0;
	sid_inpause = 0;
	sidPauseRate = 0x00010000;

	// construct song message
	{
		int i,j;
		const int msgLen=50;
		static const char* msg[msgLen];
		for(i=0; i<msgLen; i++)
			msg[i]=0;
		i=0;
		for(j=0; j<mySidTuneInfo->numberOfInfoStrings() && i<msgLen; j++)
			msg[i++]=mySidTuneInfo->infoString(j);
		for(j=0; j<mySidTuneInfo->numberOfCommentStrings() && i<msgLen; j++)
			msg[i++]=mySidTuneInfo->commentString(j);
		if(i<msgLen)
			msg[i++]=mySidTuneInfo->formatString();
		plUseMessage((char **)msg);
	}

	if (!pollInit(sidIdle))
	{
		plrClosePlayer();
		return 0;
	}

	return 1;
}

void __attribute__ ((visibility ("internal"))) sidClosePlayer(void)
{
	pollClose();

	plrClosePlayer();

	if (sid_buf_pos)
	{
		ringbuffer_free (sid_buf_pos);
		sid_buf_pos = 0;
	}

	delete[] buf16;          buf16 = NULL;
	delete mySidPlayer;      mySidPlayer = NULL;
	                         mySidTuneInfo = NULL;
	delete[] sid_buf_stereo; sid_buf_stereo = NULL;
	delete[] sid_buf_4x3[0]; sid_buf_4x3[0] = NULL;
	delete[] sid_buf_4x3[1]; sid_buf_4x3[1] = NULL;
	delete[] sid_buf_4x3[2]; sid_buf_4x3[2] = NULL;
}


void __attribute__ ((visibility ("internal"))) sidPause(unsigned char p)
{
	sid_inpause=p;
}

void __attribute__ ((visibility ("internal"))) sidSetVolume(unsigned char vol, signed char bal, signed char _pan, unsigned char opt)
{
	pan=_pan;
	voll=vol*4;
	volr=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
	srnd=opt;
}

void __attribute__ ((visibility ("internal"))) sidStartSong(uint8_t sng)
{
	if (!mySidPlayer)
	{
		return;
	}
	if (sng<1)
		sng=1;
	if (sng>mySidTuneInfo->songs())
		sng=mySidTuneInfo->songs();
	clipbusy++;
	mySidPlayer->selecttrack (sng);
	clipbusy--;
}

extern uint8_t __attribute__ ((visibility ("internal"))) sidGetSong()
{
	if (!mySidPlayer)
	{
		return 0;
	}
	return mySidTuneInfo->currentSong();
}

uint8_t __attribute__ ((visibility ("internal"))) sidGetSongs(void)
{
	if (!mySidPlayer)
	{
		return 0;
	}
	return mySidTuneInfo->songs();
}

char __attribute__ ((visibility ("internal"))) sidGetVideo(void)
{
	if (!mySidPlayer)
	{
		return 0;
	}
	switch (mySidPlayer->c64Model())
	{
		default:
		case libsidplayfp::c64::model_t::PAL_B:      ///< PAL C64
		case libsidplayfp::c64::model_t::PAL_N:      ///< C64 Drean
		case libsidplayfp::c64::model_t::PAL_M:      ///< C64 Brasil
			return 1; /* PAL */
		case libsidplayfp::c64::model_t::NTSC_M:     ///< NTSC C64
		case libsidplayfp::c64::model_t::OLD_NTSC_M: ///< Old NTSC C64
			return 0; /* NTSC */
	}
}

#if 0
char __attribute__ ((visibility ("internal"))) sidGetFilter(void)
{
	return myEmuConfig->emulateFilter;
}


void __attribute__ ((visibility ("internal"))) sidToggleFilter(void)
{
	myEmuConfig->emulateFilter^=1;
	updateconf();
}
#endif

void __attribute__ ((visibility ("internal"))) sidMute(int i, int m)
{
	fprintf (stderr, "sidMute(%d, %d)::", i, m);
	sidMuted[i] = m;
	mySidPlayer->mute(i, m);
	fprintf (stderr, "done\n");
}

/*extern ubyte filterType;*/
void __attribute__ ((visibility ("internal"))) sidGetChanInfo(int i, sidChanInfo &ci)
{
	int sid = i / 3;
	int ch = i % 3;
	ci.freq=         last.registers[sid][ch*0x07+0x00] |
                        (last.registers[sid][ch*0x07+0x01]<<8);
	ci.pulse=        last.registers[sid][ch*0x07+0x02] |
                       ((last.registers[sid][ch*0x07+0x03] & 0x0f)<<8);
	ci.wave=         last.registers[sid][ch*0x07+0x04];
	ci.ad=           last.registers[sid][ch*0x07+0x05];
	ci.sr=           last.registers[sid][ch*0x07+0x06];
	ci.filtenabled = last.registers[sid][0x17] & (1<<ch);
	ci.filttype    = last.registers[sid][0x18];

	unsigned int leftvol, rightvol;
	leftvol = rightvol = last.volumes[sid][ch];

	switch (SidCount)
	{ /* mirror sidplayfp-git/libsidplayfp/src/mixer.h layout */
		default:
		case 1:
			break;
		case 2:
			if (sid) { leftvol = 0; } else { rightvol = 0; }
			break;
		case 3:
			switch (sid)
			{
				case 0:
					 leftvol = (leftvol * 150) >> 8;
					rightvol = 0;
					break;
				case 1:
					 leftvol = ( leftvol * 106) >> 8;
					rightvol = (rightvol * 106) >> 8;
					break;
				case 2:
					 leftvol = 0;
					rightvol = (rightvol * 150) >> 8;
					break;
			}
			break;
	}

	long pulsemul;
	switch (ci.wave & 0xf0)
	{
		case 0x10:
			leftvol*=192;
			rightvol*=192;
			break;
		case 0x20:
			leftvol*=224;
			rightvol*=224;
			break;
		case 0x30:
			leftvol*=208;
			rightvol*=208;
			break;
		case 0x40:
			pulsemul=2*(ci.pulse>>4);
			if (ci.pulse & 0x800)
				pulsemul=511-pulsemul;
			leftvol*=pulsemul;
			rightvol*=pulsemul;
			break;
		case 0x50:
			pulsemul=255-(ci.pulse>>4);
			leftvol*=pulsemul;
			rightvol*=pulsemul;
			break;
		case 0x60:
			pulsemul=255-(ci.pulse>>4);
			leftvol*=pulsemul;
			rightvol*=pulsemul;
			break;
		case 0x70:
			leftvol*=224;
			rightvol*=224;
			break;
		case 0x80:
			leftvol*=240;
			rightvol*=240;
			break;
		default:
			leftvol=ci.rightvol=0;
	}
	ci.leftvol=leftvol>>8;
	ci.rightvol=rightvol>>8;
}

int __attribute__ ((visibility ("internal"))) sidGetLChanSample(unsigned int i, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	int sid = i / 3;
	int ch = (i % 3) + 1;

	int stereo = (opt&cpiGetSampleStereo)?1:0;
	uint32_t step = imuldiv(0x00010000, plrRate, (signed)rate);
	int16_t *src;
	int pos1, pos2;
	int length1, length2;
	uint32_t posf = 0;

	ringbuffer_get_tail_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

	src = sid_buf_4x3[sid] + pos1 * 4 + ch;

	while (len)
	{
		if (stereo)
		{
			*(s++) = *(s++) = *src;
		} else {
			*(s++) = *src;
		}
		len--;

		posf += step;

		while (posf >= 0x00010000)
		{
			posf -= 0x00010000;

			src += 4;
			length1--;

			if (!length1)
			{
				length1 = length2;
				length2 = 0;
				src = sid_buf_4x3[sid] + pos2 * 4 + ch;
			}
			if (!length1)
			{
				memsetd(s, 0, len<<stereo);
				return !!sidMuted[ch];
			}
		}
	}
	return !!sidMuted[ch];
}

int __attribute__ ((visibility ("internal"))) sidGetPChanSample(unsigned int i, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	int sid = i / 4;
	int ch = i % 4;

	int stereo = (opt&cpiGetSampleStereo)?1:0;
	uint32_t step = imuldiv(0x00010000, plrRate, (signed)rate);
	int16_t *src;
	int pos1, pos2;
	int length1, length2;
	uint32_t posf = 0;

	ringbuffer_get_tail_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

	src = sid_buf_4x3[sid] + pos1 * 4 + ch;

	while (len)
	{
		if (stereo)
		{
			*(s++) = *(s++) = *src;
		} else {
			*(s++) = *src;
		}
		len--;

		posf += step;

		while (posf >= 0x00010000)
		{
			posf -= 0x00010000;

			src += 4;
			length1--;

			if (!length1)
			{
				length1 = length2;
				length2 = 0;
				src = sid_buf_4x3[sid] + pos2 * 4 + ch;
			}
			if (!length1)
			{
				memsetd(s, 0, len<<stereo);
				return !!sidMuted[ch];
			}
		}
	}
	return !!sidMuted[ch];

}
