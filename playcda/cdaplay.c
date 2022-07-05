/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Audio CD player
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -ss040907   Stian Skjelstad <stian@nixia.no>
 *    -complete rewrite for linux
 */

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "boot/psetting.h"
#include "cdaudio.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/cdrom.h"
#include "filesel/filesystem.h"
#include "stuff/imsrtns.h"

static int cda_inpause;

static int lba_start;   // start of track
static int lba_stop;    // end of track
static int lba_next;    // next sector to fetch
static int lba_current; // last sector sent to devp
static struct ocpfilehandle_t *fh;

#ifdef CD_FRAMESIZE_RAW
#undef CD_FRAMESIZE_RAW
#endif
#define CD_FRAMESIZE_RAW 2352

#define BUFFER_SLOTS 75*4
#define REQUEST_SLOTS 16
/* cdIdler dumping locations */
static unsigned char        cdbufdata[CD_FRAMESIZE_RAW*BUFFER_SLOTS];
static struct ringbuffer_t *cdbufpos;
static uint32_t             cdbuffpos; /* fractional part */
static uint32_t             cdbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */
static uint32_t rip_sectors[BUFFER_SLOTS]; /* replace me */
static uint32_t rip_sectors2[BUFFER_SLOTS]; /* devp space */
static uint32_t cdRate; /* the devp output rate */
static volatile int clipbusy;
static int speed;
static int cda_looped;
static int donotloop;

static unsigned int voll = 256, volr = 256;
static int vol;
static int bal = 0;
static int pan = 64;
static int srnd;

static int (*_GET)(int ch, int opt);
static void (*_SET)(int ch, int opt, int val);

static struct ioctl_cdrom_readaudio_request_t req;
static int req_active = 0;
static int req_pos1;

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

static void delay_callback_from_devp (void *arg, int samples_ago)
{
	uint32_t *rip_sector_last = arg;
	lba_current = *rip_sector_last;
}

static void delay_callback_from_cdbufdata (void *arg, int samples_ago)
{
	int samples_until = samples_ago * cdbufrate / 65536;

	int index = ((uint32_t *)arg - rip_sectors) / sizeof (rip_sectors[0]);

	rip_sectors2[index] = rip_sectors[index];

	plrAPI->OnBufferCallback (-samples_until, delay_callback_from_devp, &rip_sectors[index]);
}

static void cdIdlerAddBuffer(void)
{
	int temp;

	for (temp = 0; temp < req.lba_count; temp++)
	{
		int index = (req_pos1/CD_FRAMESIZE_RAW)+temp;
		rip_sectors[index] = lba_next + temp;
		ringbuffer_add_tail_callback_samples (cdbufpos, - temp * (CD_FRAMESIZE_RAW >> 2) /* stereo + bit16 */, delay_callback_from_cdbufdata, &rip_sectors[index]);
	}

#ifdef WORDS_BIGENDIAN
	for (temp = 0; temp < req.lba_count * CD_FRAMESIZE_RAW / 2; temp++)
	{
		((uint16_t *)req.ptr)[temp] = uint16_little(((uint16_t *)req.ptr)[temp]);
	}
#endif
	ringbuffer_head_add_bytes (cdbufpos, req.lba_count * CD_FRAMESIZE_RAW);
	lba_next += req.lba_count;
}

static void cdIdler(void)
{
	int pos2;
	int length1, length2;
	int emptyframes;
	int temp;

	if (req_active)
	{
		if (fh->ioctl (fh, IOCTL_CDROM_READAUDIO_ASYNC_PULL, &req))
		{
			return;
		}
		cdIdlerAddBuffer();
		req_active = 0;
	}

	/* first check for EOF */
	if (lba_next == lba_stop)
	{
		if (donotloop)
		{
			cda_looped |= 1;
			return;
		} else {
			cda_looped &= ~1;
			lba_next = lba_start;
		}
	} else {
		cda_looped &= ~1;
	}

	ringbuffer_get_head_bytes (cdbufpos, &req_pos1, &length1, &pos2, &length2);

	emptyframes = length1 / CD_FRAMESIZE_RAW;
	assert (emptyframes >= 0);
	if ((!emptyframes) || ((emptyframes < REQUEST_SLOTS) && (!length2)))
	{
		return;
	}
	emptyframes = (emptyframes > REQUEST_SLOTS) ? REQUEST_SLOTS : emptyframes;
	assert (emptyframes > 0);

	/* check against track end */
	assert (lba_stop > lba_next);
	temp = lba_stop - lba_next;
	if (emptyframes > temp)
	{
		emptyframes = temp;
	}

	assert (emptyframes);

	req.lba_addr = lba_next;
	req.lba_count = emptyframes;
	req.ptr = cdbufdata + req_pos1;
	assert ((req_pos1 + emptyframes * CD_FRAMESIZE_RAW) <= sizeof (cdbufdata));
	switch (fh->ioctl (fh, IOCTL_CDROM_READAUDIO_ASYNC_REQUEST, &req))
	{
		case -1:
			return;
		case 0:
			cdIdlerAddBuffer();
			return;
		case 1:
			req_active = 1;
			return;
	}
}

void __attribute__ ((visibility ("internal"))) cdIdle(void)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (cda_inpause || (cda_looped == 3))
	{
		plrAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		plrAPI->Pause (0);

		plrAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			cdIdler();

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			ringbuffer_get_tail_samples (cdbufpos, &pos1, &length1, &pos2, &length2);

			if (cdbufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2);
					cda_looped |= 2;
				} else {
					cda_looped &= ~2;
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

						rs = ((int16_t *)cdbufdata)[(pos1<<1) + 0];
						ls = ((int16_t *)cdbufdata)[(pos1<<1) + 1];

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
				cda_looped &= ~2;

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
							cda_looped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if ((length1+length2) < ((cdbufrate+cdbuffpos)>>16))
						{
							cda_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = ((uint16_t *)cdbufdata)[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = ((uint16_t *)cdbufdata)[(wpm1<<1)+1]^0x8000;
						 rc0 = ((uint16_t *)cdbufdata)[(wp0 <<1)+0]^0x8000;
						 lc0 = ((uint16_t *)cdbufdata)[(wp0 <<1)+1]^0x8000;
						 rv1 = ((uint16_t *)cdbufdata)[(wp1 <<1)+0]^0x8000;
						 lv1 = ((uint16_t *)cdbufdata)[(wp1 <<1)+1]^0x8000;
						 rv2 = ((uint16_t *)cdbufdata)[(wp2 <<1)+0]^0x8000;
						 lv2 = ((uint16_t *)cdbufdata)[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,cdbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,cdbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,cdbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,cdbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,cdbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,cdbuffpos);
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

						cdbuffpos+=cdbufrate;
						progress = cdbuffpos>>16;
						cdbuffpos &= 0xffff;
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
			} /* if (cdbufrate==0x10000) */

			ringbuffer_tail_consume_samples (cdbufpos, accumulated_source);
			plrAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	plrAPI->Idle();

	clipbusy--;
}

static void cdSetSpeed (unsigned short sp)
{

	if (sp<32)
		sp=32;

	speed=sp;

	cdbufrate=imuldiv(256*sp, 44100, cdRate);
}

static void cdSetVolume()
{
	volr=voll=vol*4;
	if (bal<0)
		voll=(voll*(64+bal))>>6;
	else
		volr=(volr*(64-bal))>>6;
}

static void SET(int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			cdSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			cdSetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			cdSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			cdSetVolume();
			break;
	}
}
static int GET(int ch, int opt)
{
	return 0;
}

void __attribute__ ((visibility ("internal"))) cdPause (void)
{
	cda_inpause=1;
}

void __attribute__ ((visibility ("internal"))) cdClose (void)
{
	cda_inpause=1;

	plrAPI->Stop();

	if (cdbufpos)
	{
		ringbuffer_free (cdbufpos);
		cdbufpos = 0;
	}

	if (req_active)
	{
		while (fh->ioctl (fh, IOCTL_CDROM_READAUDIO_ASYNC_PULL, &req) > 1)
		{
			usleep (1000);
		}
	}

	if (fh)
	{
		fh->unref (fh);
		fh = 0;
	}

	if (_SET)
	{
		mcpSet = _SET;
		_SET = 0;
	}
	if (_GET)
	{
		mcpGet = _GET;
		_GET = 0;
	}
}

void __attribute__ ((visibility ("internal"))) cdUnpause (void)
{
	cda_inpause=0;
}

void __attribute__ ((visibility ("internal"))) cdJump (unsigned long start)
{
	int pos1, length1, pos2, length2;
	if (start < lba_start) start = lba_start;
	if (start > lba_stop) start = lba_stop - 1;

	lba_next = start;

	ringbuffer_get_tail_bytes (cdbufpos, &pos1, &length1, &pos2, &length2);
	ringbuffer_tail_consume_bytes (cdbufpos, length1 + length2);
	cdbuffpos = 0;
}

int __attribute__ ((visibility ("internal"))) cdOpen (unsigned long start, unsigned long len, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;

	lba_next = lba_start = lba_current = start;
	lba_stop = start + len;

	if (fh)
	{
		cdClose ();
	}
	fh = file;
	fh->ref (fh);

	clipbusy = 0;

	cdRate=44100;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!plrAPI->Play (&cdRate, &format, file, cpifaceSession))
	{
		return -1;
	}

	cda_inpause=0;
	cda_looped=0;
	donotloop = 1;

	cdbufpos = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, sizeof (cdbufdata) / 4);
	if (!cdbufpos)
	{
		plrAPI->Stop();
		return 0;
	}
	cdbuffpos = 0;

	cdSetSpeed(256);

	_SET=mcpSet;
	_GET=mcpGet;
	mcpSet=SET;
	mcpGet=GET;

	mcpNormalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	return 0;
}

void __attribute__ ((visibility ("internal"))) cdGetStatus (struct cdStat *stat)
{
	stat->error=0;
	stat->paused=cda_inpause;
	stat->position=lba_current;
	stat->speed=(cda_inpause?0:speed);
	stat->looped=(lba_next==lba_stop)&&(cda_looped==3);
}

void __attribute__ ((visibility ("internal"))) cdSetLoop (int loop)
{
	donotloop=!loop;
}
