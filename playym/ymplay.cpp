/* OpenCP Module Player
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OPLPlay - Player for AdPlug - Replayer for many OPL2/OPL3 audio file formats.
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
#include <cstdlib>
#include <string.h>
#include "types.h"
extern "C"
{
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
}
#include "ymplay.h"
#include "stsoundlib/YmMusic.h"

static int ym_inpause;
static int ym_looped;

static uint32_t ymRate;

static int vol, bal;
static unsigned long voll,volr;
static int pan;
static int srnd;
/* Are resourses in-use (needs to be freed at Close) ?*/
static int active=0;

/* devp buffer zone */
static int donotloop=1;

/* ymIdler dumping locations */

#define TIMESLOTS 128
#define REGISTERS 10
static struct timeslot
{
	int inymbuf; /* ymbuf */
	int indevp;  /* devp */
	uint8_t registers[REGISTERS];
	const struct plrDevAPI_t *plrDevAPI;
} timeslots[TIMESLOTS];
static struct timeslot register_current_state;

#define YMBUFLEN 16386
static ymsample ymbuf[YMBUFLEN]; /* the buffer, mono */
static struct ringbuffer_t *ymbufpos = 0;
static uint32_t ymbuffpos; /* read fine-pos.. when ymbufrate has a fraction */
__attribute__ ((visibility ("internal"))) uint32_t ymbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */

__attribute__ ((visibility ("internal"))) CYmMusic *pMusic;

/* clipper threadlock since we use a timer-signal */
static volatile int clipbusy=0;

static struct timeslot *register_slot_get (void)
{
	int i;
	for (i=0; i < TIMESLOTS; i++)
	{
		if (timeslots[i].inymbuf) continue;
		if (timeslots[i].indevp) continue;
		return timeslots + i;
	}
	return 0;
}

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
	rs = (ymsample)(_rs * volr / 256.0); \
	ls = (ymsample)(_ls * voll / 256.0); \
	if (srnd) \
	{ \
		ls ^= 0xffff; \
	} \
} while(0)

void __attribute__ ((visibility ("internal"))) ymClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (active)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);

		ymMusicStop(pMusic);
		ymMusicDestroy(pMusic);

		if (ymbufpos)
		{
			cpifaceSession->ringbufferAPI->free (ymbufpos);
			ymbufpos = 0;
		}

		active=0;
	}
}

void __attribute__ ((visibility ("internal"))) ymMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m)
{
	cpifaceSession->MuteChannel[i] = m;
	fprintf(stderr, "[YM] TODO, ymMute(i, m)\n");
}

static void ymSetSpeed(uint16_t sp)
{
	if (sp < 4)
		sp  = 4;
	ymbufrate=256*sp;
}

static void ymSetVolume(void)
{
	volr=voll=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
}

static void ymSet (int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			ymSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			break;
		case mcpMasterVolume:
			vol=val;
			ymSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			ymSetVolume();
			break;
	}
}

static int ymGet (int ch, int opt)
{
	return 0;
}

uint32_t __attribute__ ((visibility ("internal"))) ymGetPos(void)
{
	return ymMusicGetPos(pMusic);
}
void __attribute__ ((visibility ("internal"))) ymSetPos(uint32_t pos)
{
	if (pos>=0x80000000)
		pos=0;
	ymMusicSeek(pMusic, pos);
}

int __attribute__ ((visibility ("internal"))) ymOpenPlayer(struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	void *buffer = 0;
	uint64_t length = file->filesize (file);
	int retval;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}
	if (length <= 0)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[YM] Unable to determine file length\n");
		return errFormStruc;
	}
	if (length > (1024*1024))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[YM] File too big\n");
		return errFormStruc;
	}
	buffer = malloc(length);
	if (!buffer)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[YM] Unable to malloc()\n");
		return errAllocMem;
	}
	if (file->read (file, buffer, length) != (int)length)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[YM] Unable to read file\n");
		goto error_out_buffer;
	}

	ymRate=0;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&ymRate, &format, file, cpifaceSession))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[YM] plrDevAPI->Play() failed\n");
		retval = errPlay;
		goto error_out_buffer;
	}

	cpifaceSession->mcpSet = ymSet;
	cpifaceSession->mcpGet = ymGet;
	cpifaceSession->mcpAPI->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	ym_looped = 0;

	bzero (timeslots, sizeof (timeslots));

	pMusic = new CYmMusic(ymRate);
	if (!pMusic)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[YM] Unable to create stymulator object\n");
		retval = errAllocMem;
		goto error_out_plrDevAPI_Play;
	}
	if (!pMusic->loadMemory(buffer, length))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[YM] Unable to load file: %s\n", pMusic->getLastError());
		retval = errFormStruc;
		goto error_out_plrDevAPI_Play;
	}

	free(buffer); buffer = 0;

	ymbufrate=0x10000; /* 1.0 */
	ymbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_MONO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, YMBUFLEN);
	if (!ymbufpos)
	{
		retval = errAllocMem;
		goto error_out_plrDevAPI_Play;
	}
	ymbuffpos=0;

	active=1;
	return errOk;

error_out_plrDevAPI_Play:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);

error_out_buffer:
	free (buffer); buffer = 0;

	if (ymbufpos)
	{
		cpifaceSession->ringbufferAPI->free (ymbufpos);
		ymbufpos = 0;
	}

	if (pMusic)
	{
		delete(pMusic);
		pMusic = 0;
	}
	return retval;
}

void __attribute__ ((visibility ("internal"))) ymSetLoop(int loop)
{
	pMusic->setLoopMode(loop);
	donotloop=!loop;
}

int __attribute__ ((visibility ("internal"))) ymIsLooped(void)
{
	return ym_looped==3;
}

void __attribute__ ((visibility ("internal"))) ymPause(uint8_t p)
{
	ym_inpause=p;
}

static struct channel_info_t Registers;

static void register_delay_callback_from_devp (void *arg, int samples_ago)
{
	struct timeslot *state = (struct timeslot *)arg;
	state->indevp = 0;

	register_current_state = *state;

	if (register_current_state.registers[0]==0)
		Registers.frequency_a = 0;
	else
		Registers.frequency_a = pMusic->readYmClock() / (register_current_state.registers[0] * 16);

	if (register_current_state.registers[1]==0)
		Registers.frequency_b = 0;
	else
		Registers.frequency_b = pMusic->readYmClock() / (register_current_state.registers[1] * 16);

	if (register_current_state.registers[2]==0)
		Registers.frequency_c = 0;
	else
		Registers.frequency_c = pMusic->readYmClock() / (register_current_state.registers[2] * 16);

	if (register_current_state.registers[3] == 0)
		Registers.frequency_noise = 0;
	else
		Registers.frequency_noise = pMusic->readYmClock() / (register_current_state.registers[3] * 16);

	Registers.mixer_control = register_current_state.registers[4];
	Registers.level_a = register_current_state.registers[5];
	Registers.level_b = register_current_state.registers[6];
	Registers.level_c = register_current_state.registers[7];

	if (register_current_state.registers[8] == 0)
		Registers.frequency_envelope = 0;
	else
		Registers.frequency_envelope = pMusic->readYmClock() / (register_current_state.registers[8] * 256);

	Registers.envelope_shape = register_current_state.registers[9];
}

static void register_delay_callback_from_ymbuf (void *arg, int samples_ago)
{
	struct timeslot *state = (struct timeslot *)arg;

	int samples_until = samples_ago * ymbufrate / 65536;

	state->inymbuf = 0;
	state->indevp = 1;
	state->plrDevAPI->OnBufferCallback (samples_until, register_delay_callback_from_devp, state);
}

static void ymIdler(struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos1, pos2;
	int length1, length2;

	if (!active)
		return;

	cpifaceSession->ringbufferAPI->get_head_samples (ymbufpos, &pos1, &length1, &pos2, &length2);

	while (length1)
	{
		struct timeslot *slot;

		if ((ym_looped & 1) && donotloop)
			break;

		if ((unsigned int)length1>(ymRate/50))
			length1=ymRate/50;

		if (!pMusic->update(ymbuf + pos1, length1))
			ym_looped|=1;
	
		slot = register_slot_get ();
		if (slot)
		{
			slot->registers[0] = pMusic->readYmRegister(0)|(pMusic->readYmRegister(1)<<8); /* frequency A */
			slot->registers[1] = pMusic->readYmRegister(2)|(pMusic->readYmRegister(3)<<8); /* frequency B */
			slot->registers[2] = pMusic->readYmRegister(4)|(pMusic->readYmRegister(5)<<8); /* frequency C */
			slot->registers[3] = pMusic->readYmRegister(6)&0x1f; /* frequency noise */
			slot->registers[4] = pMusic->readYmRegister(7); /* mixer control */
			slot->registers[5] = pMusic->readYmRegister(8); /* volume A */
			slot->registers[6] = pMusic->readYmRegister(9); /* volume B */
			slot->registers[7] = pMusic->readYmRegister(10); /* volume C */
			slot->registers[8] = pMusic->readYmRegister(11)|(pMusic->readYmRegister(12)<<8); /* frequency envelope */
			slot->registers[9] = pMusic->readYmRegister(13) & 0x0f;  /* envelope shape */
			slot->inymbuf = 1;
			slot->plrDevAPI = cpifaceSession->plrDevAPI;
			cpifaceSession->ringbufferAPI->add_tail_callback_samples (ymbufpos, 0, register_delay_callback_from_ymbuf, slot);
		}

		cpifaceSession->ringbufferAPI->head_add_samples (ymbufpos, length1);
		cpifaceSession->ringbufferAPI->get_head_samples (ymbufpos, &pos1, &length1, &pos2, &length2);
	}
}

__attribute__ ((visibility ("internal"))) struct channel_info_t *ymRegisters()
{
	return &Registers;
}

void __attribute__ ((visibility ("internal"))) ymIdle(struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (ym_inpause || (ym_looped == 3))
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

			ymIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (ymbufpos, &pos1, &length1, &pos2, &length2);

			if (ymbufrate==0x10000)
			{
				if (targetlength>((unsigned int)length1+length2))
				{
					targetlength=(length1+length2);
					ym_looped |= 2;
				} else {
					ym_looped &= ~2;
				}

				// limit source to not overrun target buffer
				if ((unsigned int)length1 > targetlength)
				{
					length1 = targetlength;
					length2 = 0;
				} else if ((unsigned int)(length1 + length2) > targetlength)
				{
					length2 = targetlength - length1;
				}

				accumulated_source = accumulated_target = length1 + length2;

				while (length1)
				{
					while (length1)
					{
						int16_t rs, ls;

						ls = rs = ymbuf[pos1]; /* mono source... */

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
				ym_looped &= ~2;

				while (targetlength && length1)
				{
					while (targetlength && length1)
					{
						uint32_t wpm1, wp0, wp1, wp2;
						int32_t lc0, lc1, lc2, lc3, lvm1, lv1, lv2;
						unsigned int progress;
						int16_t rs, ls;

						/* will the interpolation overflow? */
						if ((length1+length2) <= 3)
						{
							ym_looped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if (((unsigned int)length1+length2) < ((ymbufrate+ymbuffpos)>>16))
						{
							ym_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						lvm1 = (uint16_t)ymbuf[wpm1]^0x8000;
						 lc0 = (uint16_t)ymbuf[wp0 ]^0x8000;
						 lv1 = (uint16_t)ymbuf[wp1 ]^0x8000;
						 lv2 = (uint16_t)ymbuf[wp2 ]^0x8000;


						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,ymbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,ymbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,ymbuffpos);
						lc3 += lc0;
						if (lc3<0)
							lc3=0;
						if (lc3>65535)
							lc3=65535;

						ls = lc3 ^ 0x8000;
						rs = ls;

						PANPROC;

						*(t++) = rs;
						*(t++) = ls;

						ymbuffpos+=ymbufrate;
						progress = ymbuffpos>>16;
						ymbuffpos &= 0xffff;
						accumulated_source+=progress;
						pos1+=progress;
						length1-=progress;
						targetlength--;

						accumulated_target++;
					} /* while (targetlength && length1) */
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				} /* while (targetlength && length1) */
			} /* if (ymbufrate==0x10000) */

			cpifaceSession->ringbufferAPI->tail_consume_samples (ymbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();

	clipbusy--;
}
