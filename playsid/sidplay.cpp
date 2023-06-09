/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2011-'23 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/mixclip.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
}

#include "sidplay.h"
#include "libsidplayfp-api.h"

#define MAXIMUM_SLOW_DOWN 32
#define ROW_BUFFERS 30 /* half a second */

static libsidplayfp::ConsolePlayer *mySidPlayer;
static SidTuneInfo const *mySidTuneInfo;
static int sid_samples_per_row;

typedef struct
{
	uint8_t registers[3][0x20]; /* current register values, 3 SID chips */
	uint8_t volumes[3][3];

	uint8_t in_use;
} SidStatBuffer_t;

static SidStatBuffer_t last; /* current register values, 3 SID chips */

static SidStatBuffer_t SidStatBuffers[ROW_BUFFERS] = {{0}}; // half a second
static int SidStatBuffers_available = 0;

static int16_t *sid_buf_stereo; /* stereo interleaved */
static int16_t *sid_buf_4x3[3]; /* 4-chan interleaved, 3 SID chips */

static struct ringbuffer_t *sid_buf_pos;
/*             tail              processing        head
 *  (free)      | already in devp | ready to stream |   (free)
 *
 *          As the tail catches up, we know data has been played, and we update our stats on the screen
 */

static uint32_t sidbuffpos;
static uint32_t sidbufrate;
static signed int sidbufrate_compensate;
static uint32_t sidRate; /* devp rate */

static uint64_t samples_committed;
static uint64_t samples_lastui;

static unsigned long voll,volr;
static int vol, bal;
static int pan;
static int srnd;

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

	last = *state;

	state->in_use = 0;
	SidStatBuffers_available++;
}

OCP_INTERNAL void sidIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	while (SidStatBuffers_available) /* we only prepare more data if SidStatBuffers_available is non-zero. This gives about 0.5 seconds worth of sample-data */
	{
		int i, j;

		int pos1, pos2;
		int length1, length2;

		for (i=0; i < ROW_BUFFERS; i++)
		{
			if (SidStatBuffers[i].in_use)
			{
				continue;
			}
			break;
		}
		assert (i != ROW_BUFFERS);

		cpifaceSession->ringbufferAPI->get_head_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

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
			uint8_t registers[32];
			mySidPlayer->getSidStatus (j,
			                           registers,
			                           SidStatBuffers[i].volumes[j][0],
			                           SidStatBuffers[i].volumes[j][1],
			                           SidStatBuffers[i].volumes[j][2]);
			memcpy (SidStatBuffers[i].registers[j], registers, 0x20);
		}

		SidStatBuffers[i].in_use = 1;
		cpifaceSession->ringbufferAPI->add_tail_callback_samples (sid_buf_pos, 0, SidStatBuffers_callback_from_sidbuf, SidStatBuffers + i);

		/* Adding sid_samples_per_row to our devp-mirrored buffer */

		cpifaceSession->ringbufferAPI->head_add_samples (sid_buf_pos, sid_samples_per_row);

		SidStatBuffers_available--;
	}
}

OCP_INTERNAL void sidIdle(struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (cpifaceSession->InPause /*|| (sid_looped == 3)*/)
	{
		cpifaceSession->plrDevAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		cpifaceSession->plrDevAPI->Pause (0);

		cpifaceSession->plrDevAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = (int16_t *)targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			sidIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			/* We are using processing, not tail */
			cpifaceSession->ringbufferAPI->get_processing_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

			if (sidbufrate == 0x00010000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2);
					//sid_looped |= 2;
				} else {
					//sid_looped &= ~2;
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

						rs = sid_buf_stereo[(pos1<<1) + 0];
						ls = sid_buf_stereo[(pos1<<1) + 1];

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
			} else {
				/* We are going to perform cubic interpolation of rate conversion... this bit is tricky */
				// sid_looped &= ~2;

				while (targetlength && length1)
				{
					while (targetlength && length1)
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
						if ((length1+length2) < ((sidbufrate+sidbuffpos)>>16))
						{
							//sid_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)sid_buf_stereo[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)sid_buf_stereo[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)sid_buf_stereo[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)sid_buf_stereo[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)sid_buf_stereo[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)sid_buf_stereo[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)sid_buf_stereo[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)sid_buf_stereo[(wp2 <<1)+1]^0x8000;

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

						*(t++) = rs;
						*(t++) = ls;
						sidbuffpos+=sidbufrate;
						progress = sidbuffpos>>16;
						sidbuffpos &= 0xffff;
						accumulated_source+=progress;
						pos1+=progress;
						length1-=progress;
						targetlength--;

						if (length1 < 0)
						{
							length2 += length1;
							length1 = 0;
						}

						accumulated_target++;
					} /* while (targetlength && length1) */
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				} /* while (targetlength && length1) */
			} /* if (sidbufrate==0x10000) */
			/* We are using processing instead of tail here */
			cpifaceSession->ringbufferAPI->processing_consume_samples (sid_buf_pos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
			samples_committed += accumulated_target;
			sidbufrate_compensate += accumulated_target - accumulated_source;
		} /* if (targetlength) */
	}

	{
		uint64_t delay = cpifaceSession->plrDevAPI->Idle();
		uint64_t new_ui = samples_committed - delay;
		if (new_ui > samples_lastui)
		{
			int delta = new_ui - samples_lastui;

#warning use the new API instead of this wierd hack ???
			if (sidbufrate_compensate > 0) /* we have been slowing down */
			{
				if (delta >= sidbufrate_compensate)
				{
					delta -= sidbufrate_compensate;
					sidbufrate_compensate = 0;
				} else {
					sidbufrate_compensate -= delta;
					delta = 0;
				}
			} else if ((sidbufrate_compensate < 0) && delta) /* we have been speeding up... */
			{
				delta -= sidbufrate_compensate; /* double negative, makes delta grow */
				sidbufrate_compensate = 0;
			}

			cpifaceSession->ringbufferAPI->tail_consume_samples (sid_buf_pos, delta);
			samples_lastui = new_ui;
		}
	}

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


OCP_INTERNAL int sidNumberOfChips (void)
{
	return SidCount;
}

OCP_INTERNAL int sidNumberOfComments (void)
{
	return mySidTuneInfo->numberOfCommentStrings();
}

OCP_INTERNAL int sidNumberOfInfos(void)
{
	return mySidTuneInfo->numberOfInfoStrings();
}

OCP_INTERNAL const char *sidInfoString (int i)
{
	return mySidTuneInfo->infoString(i);
}

OCP_INTERNAL const char *sidCommentString (int i)
{
	return mySidTuneInfo->commentString(i);
}

OCP_INTERNAL const char *sidFormatString (void)
{
	return mySidTuneInfo->formatString();
}

OCP_INTERNAL const char *sidROMDescKernal (void)
{
	return mySidPlayer->kernalDesc();
}

OCP_INTERNAL const char *sidROMDescBasic (void)
{
	return mySidPlayer->basicDesc();
}

OCP_INTERNAL const char *sidROMDescChargen (void)
{
	return mySidPlayer->chargenDesc();
}

OCP_INTERNAL const float sidGetCPUSpeed (void)
{
	return mySidPlayer->getMainCpuSpeed();
}

OCP_INTERNAL const char *sidGetVICIIModelString (void)
{
	return libsidplayfp::VICIImodel_ToString(mySidPlayer->getVICIImodel());
}

OCP_INTERNAL const char *sidGetCIAModelString (void)
{
	return mySidPlayer->getCIAmodel();
}

OCP_INTERNAL const char *sidChipModel (int i)
{
	return libsidplayfp::sidModel_ToString(mySidPlayer->getSIDmodel(i));
}

OCP_INTERNAL uint16_t sidChipAddr (int i)
{
	return mySidPlayer->getSIDaddr(i);
}

OCP_INTERNAL const char *sidTuneStatusString (void)
{
	return mySidPlayer->getTuneStatusString();
}

OCP_INTERNAL const char *sidTuneInfoClockSpeedString (void)
{
	return libsidplayfp::tuneInfo_clockSpeed_toString(mySidPlayer->getTuneInfoClockSpeed());
}

static void sidSetPitch (uint32_t sp)
{
	if (sp > 0x00080000) sp = 0x00080000;
	if (!sp) sp = 0x1;
	sidbufrate = sp;
}

static void sidSetVolume (void)
{
	voll=vol*4;
	volr=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
}

static void sidSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
#warning TODO SetSpeed...
			//sidSetSpeed(val);
			break;
		case mcpMasterPitch:
			sidSetPitch(val<<8);
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			sidSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			sidSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			sidSetVolume();
			break;
#warning FILTER TODO
	}
}

static int sidGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

OCP_INTERNAL void sidStartSong (uint8_t sng)
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

OCP_INTERNAL uint8_t sidGetSong ()
{
	if (!mySidPlayer)
	{
		return 0;
	}
	return mySidTuneInfo->currentSong();
}

OCP_INTERNAL uint8_t sidGetSongs (void)
{
	if (!mySidPlayer)
	{
		return 0;
	}
	return mySidTuneInfo->songs();
}

OCP_INTERNAL char sidGetVideo (void)
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
OCP_INTERNAL char sidGetFilter (void)
{
	return myEmuConfig->emulateFilter;
}

OCP_INTERNAL void sidToggleFilter (void)
{
	myEmuConfig->emulateFilter^=1;
	updateconf();
}
#endif

OCP_INTERNAL void sidMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m)
{
	cpifaceSession->MuteChannel[i] = m;
	sidMuted[i] = m;
	mySidPlayer->mute(i, m);
}

/*extern ubyte filterType;*/
OCP_INTERNAL void sidGetChanInfo (int i, sidChanInfo &ci)
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

OCP_INTERNAL int sidGetLChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int i, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	int sid = i / 3;
	int ch = (i % 3) + 1;

	int stereo = (opt&mcpGetSampleStereo)?1:0;
	uint32_t step = imuldiv(0x00010000, sidRate, (signed)rate);
	int16_t *src;
	int pos1, pos2;
	int length1, length2;
	uint32_t posf = 0;

	cpifaceSession->ringbufferAPI->get_tail_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

	src = sid_buf_4x3[sid] + pos1 * 4 + ch;

	while (len)
	{
		if (stereo)
		{
			*(s++) = *src;
			*(s++) = *src;
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
				memset (s, 0, (len<<stereo)<<2);
				return !!sidMuted[ch];
			}
		}
	}
	return !!sidMuted[ch];
}

OCP_INTERNAL int sidGetPChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int i, int16_t *s, unsigned int len, uint32_t rate, int opt)
{
	int sid = i / 4;
	int ch = i % 4;

	int stereo = (opt&mcpGetSampleStereo)?1:0;
	uint32_t step = imuldiv(0x00010000, sidRate, (signed)rate);
	int16_t *src;
	int pos1, pos2;
	int length1, length2;
	uint32_t posf = 0;

	cpifaceSession->ringbufferAPI->get_tail_samples (sid_buf_pos, &pos1, &length1, &pos2, &length2);

	src = sid_buf_4x3[sid] + pos1 * 4 + ch;

	while (len)
	{
		if (stereo)
		{
			*(s++) = *src;
			*(s++) = *src;
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
				memset (s, 0, (len<<stereo)<<2);
				return !!sidMuted[ch];
			}
		}
	}
	return !!sidMuted[ch];
}

OCP_INTERNAL int sidOpenPlayer (struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int retval;
	enum plrRequestFormat format=PLR_STEREO_16BIT_SIGNED;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	samples_committed = 0;
	samples_lastui = 0;

	const int length = file->filesize (file);
	if (!length)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SID] File is way too small\n");
		return errFormStruc;
	}
	if (length > 1024*1024)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SID] File is way too big\n");
		return errFormStruc;
	}

	unsigned char *buf=new unsigned char[length];
	if (!buf)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SID] new() #1 failed\n");
		return errAllocMem;
	}

	if (file->read (file, buf, length) != length)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SID] read failed #1\n");
		retval = errFileRead;
		goto error_out_buf;
	}

	sidRate=0;
	if (!cpifaceSession->plrDevAPI->Play (&sidRate, &format, file, cpifaceSession))
	{
		retval = errPlay;
		goto error_out_buf;
	}

	mySidPlayer = new libsidplayfp::ConsolePlayer(sidRate, cpifaceSession->configAPI, cpifaceSession->dirdb, cpifaceSession->dmFile);
	if (!mySidPlayer->load (buf, length))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SID] loading file failed\n");
		retval = errFormStruc;
		goto error_out_mySidPlay;
	}
	delete [] buf; buf = 0;
	mySidTuneInfo = mySidPlayer->getInfo();

	SidCount = mySidPlayer->getSidCount();
	if (!mySidTuneInfo)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[SID] retrieve info from file failed\n");
		retval = errFormStruc;
		goto error_out_mySidPlay;
	}

	memset(sidMuted, 0, sizeof (sidMuted));

#warning FIX ME, rate is fixed to 50 at this line!!!
	sid_samples_per_row = sidRate / 50;

	sid_buf_stereo = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 2 * sid_samples_per_row];
	sid_buf_4x3[0] = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 4 * sid_samples_per_row];
	sid_buf_4x3[1] = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 4 * sid_samples_per_row];
	sid_buf_4x3[2] = new int16_t [ROW_BUFFERS * MAXIMUM_SLOW_DOWN * 4 * sid_samples_per_row];
	if ((!sid_buf_4x3[0]) || (!sid_buf_4x3[1]) || (!sid_buf_4x3[2]))
	{
		retval = errAllocMem;
		goto error_out_sid_buffers;
	}

	sid_buf_pos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, ROW_BUFFERS * MAXIMUM_SLOW_DOWN * sid_samples_per_row);
	if (!sid_buf_pos)
	{
		retval = errAllocMem;
		goto error_out_sid_buffers;
	}

	memset (SidStatBuffers, 0, sizeof (SidStatBuffers));
	SidStatBuffers_available = ROW_BUFFERS;

	sidbuffpos = 0x00000000;
	sidbufrate_compensate = 0;
	sidbufrate = 0x00010000;

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
		cpifaceSession->UseMessage((char **)msg);
	}

	cpifaceSession->mcpSet = sidSet;
	cpifaceSession->mcpGet = sidGet;
	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	return errOk;

	//cpifaceSession->ringbufferAPI->free (sid_buf_pos); sid_buf_pos = 0;
error_out_sid_buffers:
	delete[] sid_buf_stereo; sid_buf_stereo = NULL;
	delete[] sid_buf_4x3[0]; sid_buf_4x3[0] = NULL;
	delete[] sid_buf_4x3[1]; sid_buf_4x3[1] = NULL;
	delete[] sid_buf_4x3[2]; sid_buf_4x3[2] = NULL;
error_out_mySidPlay:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	delete mySidPlayer; mySidPlayer = NULL;
error_out_buf:
	if (buf) delete [] buf;
	return retval;
}

OCP_INTERNAL void sidClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}

	if (sid_buf_pos)
	{
		cpifaceSession->ringbufferAPI->free (sid_buf_pos);
		sid_buf_pos = 0;
	}

	delete mySidPlayer;      mySidPlayer = NULL;
	                         mySidTuneInfo = NULL;
	delete[] sid_buf_stereo; sid_buf_stereo = NULL;
	delete[] sid_buf_4x3[0]; sid_buf_4x3[0] = NULL;
	delete[] sid_buf_4x3[1]; sid_buf_4x3[1] = NULL;
	delete[] sid_buf_4x3[2]; sid_buf_4x3[2] = NULL;
}
