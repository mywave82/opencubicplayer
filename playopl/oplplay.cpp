/* OpenCP Module Player
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "libbinio-git/src/binio.h"
#include "libbinio-git/src/binstr.h"
#include <cstdlib>
#include <string.h>
#include "adplug-git/src/adplug.h"
#include "adplug-git/src/fprovide.h"
#include "types.h"
extern "C"
{
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/player.h"
#include "dev/mcp.h"
#include "dev/ringbuffer.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
}
#include "oplplay.h"
#include "oplptrak.h"
#include "ocpemu.h"
#include "oplKen.h"
#include "oplNuked.h"
#include "oplSatoh.h"
#include "oplWoody.h"

#define ROW_BUFFERS 100

struct oplStatusBuffer_t
{
	const struct plrDevAPI_t *plrDevAPI;
	int in_use;
	struct oplStatus data;
	int pos;
};
static struct oplStatusBuffer_t oplStatusBuffers[ROW_BUFFERS] = {{0}};

struct oplStatus oplLastStatus; /* current register status */
int oplLastPos;

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

static Cocpemu *opl;
static CPlayer *p;

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

OCP_INTERNAL void oplClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (active)
	{
		cpifaceSession->ringbufferAPI->free (oplbufpos);
		oplbufpos = 0;

		cpifaceSession->plrDevAPI->Stop (cpifaceSession);

		delete(p);
		delete(opl);

		active=0;

		oplTrkDone();
	}
}

OCP_INTERNAL void oplSetSong (struct cpifaceSessionAPI_t *cpifaceSession, int song)
{
	int pos1, length1, pos2, length2;
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

	cpifaceSession->ringbufferAPI->get_tail_bytes (oplbufpos, &pos1, &length1, &pos2, &length2);
	cpifaceSession->ringbufferAPI->tail_consume_bytes (oplbufpos, length1 + length2);
	oplbuffpos = 0;
}

OCP_INTERNAL void oplMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m)
{
	cpifaceSession->MuteChannel[i] = m;
	opl->setmute(i, m);
}
static void oplSet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
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

static int oplGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

class binisstreamfree: public binisstream
{
public:
	binisstreamfree(void *str, unsigned long len) : binsbase(str, len), binisstream (str, len)
	{
	}
	~binisstreamfree()
	{
		free (data);
	}
};

class CProvider_Mem: public CFileProvider
{
	private:
		char *filename;
		struct ocpfilehandle_t *file;
		struct cpifaceSessionAPI_t *cpifaceSession;
		uint8_t *file_data;
		int file_size;

	public:
		CProvider_Mem(const char *filename, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *file_data, int file_size) :
			cpifaceSession(cpifaceSession),
			file_data(file_data),
			file_size(file_size)
		{
			this->filename = strdup (filename);
			this->file = file;
			this->file->ref (this->file);
		}

		~CProvider_Mem(void)
		{
			free (filename);
			free (file_data);
			file->unref (file);
		}

		virtual binistream *open(std::string filename) const override;
		virtual void close(binistream *f) const override;
};

binistream *CProvider_Mem::open(std::string filename) const
{
	binisstream *retval = NULL;
	if (!strcmp (filename.c_str(), this->filename))
	{
		retval = new binisstream(this->file_data, this->file_size);
	} else {
		uint32_t d = cpifaceSession->dirdb->FindAndRef (file->origin->parent->dirdb_ref, filename.c_str(), dirdb_use_children);

		cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] Also need file \"%s\"\n", filename.c_str());

		if (d != UINT32_MAX)
		{
			struct ocpfile_t *f = file->origin->parent->readdir_file (file->origin->parent, d);
			cpifaceSession->dirdb->Unref (d, dirdb_use_children);
			if (f)
			{
				struct ocpfilehandle_t *file = f->open (f);
				f->unref (f);

				if (file)
				{
					size_t buffersize = 16*1024;
					uint8_t *buffer = (uint8_t *)malloc (buffersize);
					size_t bufferfill = 0;

					int res;
					while (!file->eof(file))
					{
						if (buffersize == bufferfill)
						{
							if (buffersize >= 16*1024*1024)
							{
								cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] \"%s\" is bigger than 16 Mb - further loading blocked\n", filename.c_str());
								break;
							}
							buffersize += 16*1024;
							buffer = (uint8_t *)realloc (buffer, buffersize);
						}
						res = file->read (file, buffer + bufferfill, buffersize - bufferfill);
						if (res<=0)
							break;
						bufferfill += res;
					}
					if (bufferfill)
					{
						retval = new binisstreamfree(buffer, bufferfill);
					} else {
						free (buffer);
					}
					file->unref (file);
				} else {
					cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] Unable to open %s\n", filename.c_str());
				}
			} else {
				cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] Unable to find %s\n", filename.c_str());
			}
		}
	}

	if (!retval) return 0;
	if (retval->error()) { delete retval; return 0; }

	// Open all files as little endian with IEEE floats by default
	retval->setFlag(binio::BigEndian, false);
	retval->setFlag(binio::FloatIEEE);

	return retval;
}

void CProvider_Mem::close(binistream *f) const
{
	delete f;
}

OCP_INTERNAL int oplOpenPlayer (const char *filename /* needed for detection */, uint8_t *content, const size_t len, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	enum plrRequestFormat format;
	int retval;
	const char *str;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	memset (oplStatusBuffers, 0, sizeof (oplStatusBuffers));
	memset (&oplLastStatus, 0, sizeof (oplLastStatus));
	oplLastPos = 0;

	oplRate = 0;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&oplRate, &format, file, cpifaceSession))
	{
		free (content);
		return errPlay;
	}

#warning do we need surround support?
	str = cpifaceSession->configAPI->GetProfileString ("adplug", "emulator", "nuked");
	if (!strcasecmp (str, "ken"))
	{
		opl = new Cocpemu(new oplKen(oplRate), oplRate);
	} else if (!strcasecmp (str, "satoh"))
	{
		opl = new Cocpemu(new oplSatoh(oplRate), oplRate);
	} else if (!strcasecmp (str, "woody"))
	{
		opl = new Cocpemu(new oplWoody(oplRate), oplRate);
	} else /* nuked */
	{
		opl = new Cocpemu(new oplNuked(oplRate), oplRate);
	}

	CProvider_Mem prMem (filename, file, cpifaceSession, content, len);
	if (!(p = CAdPlug::factory(filename, opl, CAdPlug::players, prMem)))
	{
		delete (opl);
		cpifaceSession->cpiDebug (cpifaceSession, "[Adplug OPL] Failed to load file\n");
		return errFormStruc;
	}

	oplbufrate=0x10000; /* 1.0 */
	oplbuffpos = 0;
	//opl_looped = 0;
	oplbufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, sizeof (oplbuf) >> 2 /* stereo + bit16 */);
	if (!oplbufpos)
	{
		retval = errAllocMem;
		goto error_out;
	}
	opltowrite=0;

	cpifaceSession->mcpSet = oplSet;
	cpifaceSession->mcpGet = oplGet;
	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayP);

	opl_inpause = 0;

	active=1;

	oplTrkSetup (cpifaceSession, p);

	return errOk;

error_out:
	cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	if (oplbufpos)
	{
		cpifaceSession->ringbufferAPI->free (oplbufpos);
		oplbufpos = 0;
	}
	delete(p);
	delete(opl);
	free (content);
	return retval;
}

OCP_INTERNAL void oplSetLoop (int loop)
{
	donotloop=!loop;
}

OCP_INTERNAL int oplIsLooped (void)
{
	return 0; // opl_looped == 3;
}

OCP_INTERNAL void oplPause (uint8_t p)
{
	opl_inpause=p;
}

static void oplSetSpeed(uint16_t sp)
{
	if (sp < 4)
		sp = 4;
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

static void delay_callback_from_devp (void *arg, int samples_ago)
{
	oplStatusBuffer_t *state = (oplStatusBuffer_t *)arg;
	oplLastStatus = state->data;
	oplLastPos = state->pos;
	state->plrDevAPI = 0;
	state->in_use = 0;
}

static void oplStatBuffers_callback_from_oplbuf (void *arg, int samples_ago)
{
	oplStatusBuffer_t *state = (oplStatusBuffer_t *)arg;
	int samples_until = samples_ago * oplbufrate / 65536;
	state->plrDevAPI->OnBufferCallback (-samples_until, delay_callback_from_devp, arg);
}

static void oplIdler (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int pos1, pos2;
	int length1, length2;

	if (!active)
	{
		return;
	}

	cpifaceSession->ringbufferAPI->get_head_samples (oplbufpos, &pos1, &length1, &pos2, &length2);

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

		for (int i=0; i < ROW_BUFFERS; i++)
		{
			if (oplStatusBuffers[i].in_use)
			{
				continue;
			}

			oplStatusBuffers[i].plrDevAPI = cpifaceSession->plrDevAPI;
			oplStatusBuffers[i].in_use = 1;
			oplStatusBuffers[i].data = opl->s;
			oplStatusBuffers[i].pos = p->getorder() << 8 | p->getrow();
			cpifaceSession->ringbufferAPI->add_tail_callback_samples (oplbufpos, 0, oplStatBuffers_callback_from_oplbuf, oplStatusBuffers + i);
			break;
		}

		cpifaceSession->ringbufferAPI->head_add_samples (oplbufpos, length1);
		cpifaceSession->ringbufferAPI->get_head_samples (oplbufpos, &pos1, &length1, &pos2, &length2);
	}
}

OCP_INTERNAL void oplIdle (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (opl_inpause /*|| (opl_looped == 3)*/)
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

			oplIdler (cpifaceSession);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (oplbufpos, &pos1, &length1, &pos2, &length2);

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
			cpifaceSession->ringbufferAPI->tail_consume_samples (oplbufpos, accumulated_source);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
		} /* if (targetlength) */
	}

	cpifaceSession->plrDevAPI->Idle();

	clipbusy--;
}

OCP_INTERNAL void oplpGetGlobInfo (oplTuneInfo &si)
{
	std::string author = p->getauthor(); /* we need to keep a reference, else c_str() data will die before we get to use it */
	std::string title = p->gettitle();  /* same here */

	si.songs=p->getsubsongs();
	si.currentSong=p->getsubsong() + 1;

	snprintf (si.author, sizeof (si.author), "%s", author.c_str());
	snprintf (si.title, sizeof (si.title), "%s", title.c_str());
}
