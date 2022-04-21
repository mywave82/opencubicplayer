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
#include <binio.h>
#include <binstr.h>
#include <cstdlib>
#include <string.h>
#include <adplug/adplug.h>
#include <adplug/fprovide.h>
#include "types.h"
extern "C"
{
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "dev/mcp.h"
#include "dev/deviplay.h"
#include "dev/ringbuffer.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
}
#include "oplplay.h"
#include "ocpemu.h"

/* options */
static int opl_inpause;
//static int opl_looped;

static unsigned long voll,volr;
static int pan;
static int srnd;
/* Are resourses in-use (needs to be freed at Close) ?*/
static int active=0;

/* mcp stuff */
static uint16_t _speed, vol;
static int16_t bal;

static int donotloop=1;

/* oplIdler dumping locations */
static int16_t oplbuf[8*1024]; /* the buffer */
static struct ringbuffer_t *oplbufpos = 0;
static uint32_t oplbuffpos; /* read fine-pos.. when oplbufrate has a fraction */
static uint32_t oplbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */
static uint32_t oplRate; /* devp rate */

static int opltowrite; /* this is adplug interface */

static Cocpopl *opl;
static CPlayer *p;

static int (*_GET)(int ch, int opt);
static void (*_SET)(int ch, int opt, int val);

/* clipper threadlock since we use a timer-signal */
static volatile int clipbusy=0;

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
	rs = (int16_t)(_rs * volr / 256.0); \
	ls = (int16_t)(_ls * voll / 256.0); \
	if (srnd) \
	{ \
		ls ^= 0xffff; \
	} \
} while(0)

static void oplSetVolume(void);
static void oplSetSpeed(uint16_t sp);

void __attribute__ ((visibility ("internal"))) oplClosePlayer(void)
{
	if (active)
	{
		ringbuffer_free (oplbufpos);
		oplbufpos = 0;

		pollClose();

		plrAPI->Stop();

		mcpSet=_SET;
		mcpGet=_GET;

		delete(p);
		delete(opl);

		active=0;
	}
}

void __attribute__ ((visibility ("internal"))) oplSetSong (int song)
{
	int songs = p->getsubsongs();
	if (song < 1)
	{
		song = 1;
	} else if (song > songs)
	{
		song = songs;
	}
	song--;
	p->rewind (song);
}

void __attribute__ ((visibility ("internal"))) oplMute(int i, int m)
{
	opl->setmute(i, m);
}
static void SET(int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			oplSetSpeed(val);
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
			oplSetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			oplSetVolume();
			break;
	}
}
static int GET(int ch, int opt)
{
	return 0;
}

class CProvider_Mem: public CFileProvider
{
	private:
		const uint8_t *file_data;
		int file_size;

	public:
		CProvider_Mem(const uint8_t *file_data, int file_size) :
			file_data(file_data),
			file_size(file_size)
		{
		}

		virtual binistream *open(std::string filename) const;
		virtual void close(binistream *f) const;
};

binistream *CProvider_Mem::open(std::string filename) const
{
	binisstream *f = new binisstream((uint8_t *)this->file_data, this->file_size);

	if (!f) return 0;
	if (f->error()) { delete f; return 0; }

	// Open all files as little endian with IEEE floats by default
	f->setFlag(binio::BigEndian, false);
	f->setFlag(binio::FloatIEEE);

	return f;
}

void CProvider_Mem::close(binistream *f) const
{
	delete f;
}

int __attribute__ ((visibility ("internal"))) oplOpenPlayer (const char *filename /* needed for detection */, const uint8_t *content, const size_t len, struct ocpfilehandle_t *file)
{
	enum plrRequestFormat format;

	oplRate = 0;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!plrAPI->Play (&oplRate, &format, file))
	{
		return 0;
	}

	opl = new Cocpopl(oplRate);

	CProvider_Mem prMem (content, len);
	if (!(p = CAdPlug::factory(filename, opl, CAdPlug::players, prMem)))
	{
		delete (opl);
		return 0;
	}

	oplbufrate=0x10000; /* 1.0 */
	oplbuffpos = 0;
	//opl_looped = 0;
	oplbufpos = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, sizeof (oplbuf) >> 2 /* stereo + bit16 */);
	if (!oplbufpos)
	{
		goto error_out;
	}
	opltowrite=0;

	if (!pollInit(oplIdle))
	{
		goto error_out;
	}

	_SET=mcpSet;
	_GET=mcpGet;
	mcpSet=SET;
	mcpGet=GET;
	mcpNormalize (mcpNormalizeDefaultPlayP);

	active=1;
	return 1;

error_out:
	plrAPI->Stop();
	if (oplbufpos)
	{
		ringbuffer_free (oplbufpos);
		oplbufpos = 0;
	}
	delete(p);
	delete(opl);
	return 0;
}

void __attribute__ ((visibility ("internal"))) oplSetLoop(int loop)
{
	donotloop=!loop;
}

int __attribute__ ((visibility ("internal"))) oplIsLooped(void)
{
	return 0; // opl_looped == 3;
}

void __attribute__ ((visibility ("internal"))) oplPause(uint8_t p)
{
	opl_inpause=p;
}

static void oplSetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	oplbufrate=256*sp;
	_speed=sp;
}

static void oplSetVolume(void)
{
	volr=voll=vol*4;
	if (bal<0)
		volr=(volr*(64+bal))>>6;
	else
		voll=(voll*(64-bal))>>6;
}

static void oplIdler(void)
{
	int pos1, pos2;
	int length1, length2;

	if (!active)
	{
		return;
	}

	ringbuffer_get_head_samples (oplbufpos, &pos1, &length1, &pos2, &length2);

	while (length1)
	{
		if (!opltowrite)
		{
			p->update(); /* TODO, rewind... */
			opltowrite = (int)((float)(oplRate)*256.0 / (p->getrefresh()*((float)_speed)));
		}
		if (length1>opltowrite)
		{
			length1=opltowrite;
		}
		opl->update(oplbuf+(pos1<<1) /* stereo */, length1); /* given in samples */

		opltowrite-=length1;

		ringbuffer_head_add_samples (oplbufpos, length1);
		ringbuffer_get_head_samples (oplbufpos, &pos1, &length1, &pos2, &length2);
	}
}

void __attribute__ ((visibility ("internal"))) oplIdle(void)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (opl_inpause /*|| (opl_looped == 3)*/)
	{
		plrAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		plrAPI->Pause (0);

		plrAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = (int16_t *)targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			oplIdler();

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			ringbuffer_get_tail_samples (oplbufpos, &pos1, &length1, &pos2, &length2);

			if (oplbufrate==0x10000)
			{
				if (targetlength>(unsigned int)(length1+length2))
				{
					targetlength=(length1+length2);
					//opl_looped |= 2;
				} else {
					//opl_looped &= ~2;
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

						rs = oplbuf[(pos1<<1) + 0];
						ls = oplbuf[(pos1<<1) + 1];

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
				//opl_looped &= ~2;

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
							//opl_looped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if ((unsigned int)(length1+length2) < ((oplbufrate+oplbuffpos)>>16))
						{
							//opl_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}

						rvm1 = (uint16_t)oplbuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)oplbuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)oplbuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)oplbuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)oplbuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)oplbuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)oplbuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)oplbuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,oplbuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,oplbuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,oplbuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,oplbuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,oplbuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,oplbuffpos);
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

						oplbuffpos+=oplbufrate;
						progress = oplbuffpos>>16;
						oplbuffpos&=0xFFFF;
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
			} /* if (oplbufrate==0x10000) */
			ringbuffer_tail_consume_samples (oplbufpos, accumulated_source);
			plrAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	plrAPI->Idle();

	clipbusy--;
}

void __attribute__ ((visibility ("internal"))) oplpGetChanInfo(int i, oplChanInfo &ci)
{
	OPL_CH *ch = &opl->opl->P_CH[i/2];
	OPL_SLOT *slot = &ch->SLOT[i&1];

	if (slot->Incr)
		ci.freq = (slot->Incr) / 0x100 ;
	else
		ci.freq = 0;
	ci.wave=opl->wavesel[i];
	if (!slot->Incr)
		ci.vol=0;
	else
		if ((ci.vol=opl->vol(i)>>7)>63)
			ci.vol=63;
}

void __attribute__ ((visibility ("internal"))) oplpGetGlobInfo(oplTuneInfo &si)
{
	std::string author = p->getauthor(); /* we need to keep a reference, else c_str() data will die before we get to use it */
	std::string title = p->gettitle();  /* same here */

	si.songs=p->getsubsongs();
	si.currentSong=p->getsubsong() + 1;

	snprintf (si.author, sizeof (si.author), "%s", author.c_str());
	snprintf (si.title, sizeof (si.title), "%s", title.c_str());
}
