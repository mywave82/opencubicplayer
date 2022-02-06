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
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
#include "dev/plrasm.h"
}
#include "oplplay.h"
#include "ocpemu.h"

/* options */
static int inpause;
static int looped;

static unsigned long voll,volr;
static int pan;
static int srnd;
/* Are resourses in-use (needs to be freed at Close) ?*/
static int active=0;

/* mcp stuff */
static uint16_t _speed, vol;
static int16_t bal;

/* devp pre-buffer zone */
static uint16_t *buf16; /* here we dump out data before it goes live */
/* devp buffer zone */
static uint32_t bufpos; /* devp write head location */
static uint32_t buflen; /* devp buffer-size in samples */
static void *plrbuf; /* the devp buffer */
static int stereo; /* boolean */
static int bit16; /* boolean */
static int signedout; /* boolean */
static int reversestereo; /* boolean */
static int donotloop=1;

/* oplIdler dumping locations */
static uint8_t oplbuf[16*1024]; /* the buffer */
static uint32_t oplbufread; /* actually this is the write head */
static uint32_t oplbufpos;  /* read pos */
static uint32_t oplbuffpos; /* read fine-pos.. when oplbufrate has a fraction */
static uint32_t oplbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */

static size_t opltowrite; /* this is adplug interface */

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
} while(0)

static void oplSetVolume(void);
static void oplSetSpeed(uint16_t sp);

void __attribute__ ((visibility ("internal"))) oplClosePlayer(void)
{
	if (active)
	{
		pollClose();
		free(buf16);

		plrClosePlayer();

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
	plrSetOptions(44100, (PLR_SIGNEDOUT|PLR_16BIT)|PLR_STEREO);

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

	opl = new Cocpopl(plrRate);

	CProvider_Mem prMem (content, len);
	if (!(p = CAdPlug::factory(filename, opl, CAdPlug::players, prMem)))
	{
		delete (opl);
		return 0;
	}

	oplbufrate=0x10000; /* 1.0 */
	oplbufpos=0;
	oplbuffpos=0;
	oplbufread=4; /* 1 << (stereo + bit16) */
	opltowrite=0;

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize * plrRate / 1000, file))
		goto error_out;

	if (!(buf16=(uint16_t *)malloc(sizeof(uint16_t)*buflen*2)))
	{
		plrClosePlayer();
		goto error_out;
	}
	bufpos=0;

	if (!pollInit(oplIdle))
	{
		free(buf16);
		plrClosePlayer();
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
	return looped;
}

void __attribute__ ((visibility ("internal"))) oplPause(uint8_t p)
{
	inpause=p;
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

/*
void mpegGetInfo(struct mpeginfo *info)
{
	info->pos=datapos;
	info->len=fl;
	info->rate=mpegrate;
	info->stereo=mpegstereo;
	info->bit16=1;
}
int32_t mpegGetPos(void)
{
	return datapos;
}
void mpegSetPos(int32_t pos)
*/

static void oplIdler(void)
{
	size_t clean;

	if (!active)
		return;

	clean=(oplbufpos+sizeof(oplbuf)-oplbufread)%sizeof(oplbuf);
	if (clean<8)
		return;
	clean-=8;

	while (clean>0)
	{
		size_t read=clean;
		if (!opltowrite)
		{
			p->update(); /* TODO, rewind... */
			opltowrite = (size_t)((float)(plrRate)*256.0 / (p->getrefresh()*((float)_speed)))<<2; /* stereo + 16bit */
		}
		if ((oplbufread+read)>sizeof(oplbuf))
			read=sizeof(oplbuf)-oplbufread;
		if (read>opltowrite)
			read=opltowrite;
		opl->update((int16_t *)(oplbuf+oplbufread), read>>2); /* given in samples */
		{
			/* TODO, asm optimize*/
			int16_t *base=(int16_t *)(oplbuf+oplbufread);
			int16_t rs, ls;
			int len=read>>2;
			while (len)
			{
				rs=/*int16_little(*/base[0]/*)*/;
				ls=/*int16_little(*/base[1]/*)*/;
				PANPROC;
				if (srnd)
					rs^=~0;
				base[0]=rs;
				base[1]=ls;
				base+=2;
				len--;
			}
		}
		oplbufread=(oplbufread+read)%sizeof(oplbuf);
		clean-=read;
		opltowrite-=read;
	}
}

void __attribute__ ((visibility ("internal"))) oplIdle(void)
{
	uint32_t bufplayed;
	uint32_t bufdelta;
	uint32_t pass2;
	int quietlen;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	quietlen=0;
	/* Where is our devp reading head? */
	bufplayed=plrGetBufPos()>>(stereo+bit16);
	bufdelta=(buflen+bufplayed-bufpos)%buflen;

	/* No delta on the devp? */
	if (!bufdelta)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}

	/* fill up our buffers */
	oplIdler();

	if (inpause)
		quietlen=bufdelta;
	else /*if (sizeof(oplbuf)!=opllen)*/ /* EOF of the oplstream? */
	{           /* should not the crap below match up easy with imuldiv(opllen>>2, 65536, plrRate) ??? TODO */
		uint32_t towrap=imuldiv((((sizeof(oplbuf)+oplbufread-oplbufpos-1)%sizeof(oplbuf))>>(1 /* we are always given stereo */ + 1 /* we are always given 16bit */)), 65536, oplbufrate);
		if (bufdelta>towrap)
		{
			/* will the eof hit inside the delta? */
			// quietlen=bufdelta-towrap;
			bufdelta=towrap;
			// looped=1;
		}
	}

	bufdelta-=quietlen;

	if (bufdelta)
	{
		uint32_t i;
		if (oplbufrate==0x10000) /* 1.0... just copy into buf16 direct until we run out of target buffer or source buffer */
		{
			uint32_t o=0;
			while (o<bufdelta)
			{
				uint32_t w=(bufdelta-o)*4;
				if ((sizeof(oplbuf)-oplbufpos)<w)
					w=sizeof(oplbuf)-oplbufpos;
				memcpy(buf16+2*o, oplbuf+oplbufpos, w);
				o+=w>>2;

				oplbufpos+=w;
				if (oplbufpos>=sizeof(oplbuf))
					oplbufpos-=sizeof(oplbuf);
			}
		} else { /* re-sample intil we don't have more target-buffer or source-buffer */
			int32_t c0, c1, c2, c3, ls, rs, vm1, v1, v2, wpm1;
			uint32_t wp1, wp2;
/*
			if ((bufdelta-=2)<0) bufdelta=0;  by my meening, this should be in place   TODO stian
*/
			for (i=0; i<bufdelta; i++)
			{
				wpm1=oplbufpos-4; if (wpm1<0) wpm1+=sizeof(oplbuf);
				wp1=oplbufpos+4; if (wp1>=sizeof(oplbuf)) wp1-=sizeof(oplbuf);
				wp2=oplbufpos+8; if (wp2>=sizeof(oplbuf)) wp2-=sizeof(oplbuf);

				c0 = *(uint16_t *)(oplbuf+oplbufpos)^0x8000;
				vm1= *(uint16_t *)(oplbuf+wpm1)^0x8000;
				v1 = *(uint16_t *)(oplbuf+wp1)^0x8000;
				v2 = *(uint16_t *)(oplbuf+wp2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3, oplbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3, oplbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3, oplbuffpos);
				ls = c3+c0;
				if (ls>65535)
					ls=65535;
				else if (ls<0)
					ls=0;

				c0 = *(uint16_t *)(oplbuf+oplbufpos+2)^0x8000;
				vm1= *(uint16_t *)(oplbuf+wpm1+2)^0x8000;
				v1 = *(uint16_t *)(oplbuf+wp1+2)^0x8000;
				v2 = *(uint16_t *)(oplbuf+wp2+2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3, oplbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3, oplbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3, oplbuffpos);
				rs = c3+c0;
				if (rs>65535)
					rs=65535;
				else if(rs<0)
					rs=0;
				buf16[2*i]=(uint16_t)ls^0x8000;
				buf16[2*i+1]=(uint16_t)rs^0x8000;

				oplbuffpos+=oplbufrate;
				oplbufpos+=(oplbuffpos>>16)*4;
				oplbuffpos&=0xFFFF;
				if (oplbufpos>=sizeof(oplbuf))
					oplbufpos-=sizeof(oplbuf);
			}
		}
		/* when we copy out from buf16, pass the buffer-len that wraps around end-of-buffer till pass2 */
		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;
		bufdelta-=pass2;

		if (bit16)
		{
			if (stereo)
			{
				if (reversestereo)
				{
					int16_t *p=(int16_t *)plrbuf+2*bufpos;
					int16_t *b=(int16_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1];
							p[1]=b[0];
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1];
							p[1]=b[0];
							p+=2;
							b+=2;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1]^0x8000;
							p[1]=b[0]^0x8000;
							p+=2;
							b+=2;
						}
						p=(int16_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1]^0x8000;
							p[1]=b[0]^0x8000;
							p+=2;
							b+=2;
						}
					}
				} else {
					int16_t *p=(int16_t *)plrbuf+2*bufpos;
					int16_t *b=(int16_t *)buf16;
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
				}
			} else {
				int16_t *p=(int16_t *)plrbuf+bufpos;
				int16_t *b=(int16_t *)buf16;
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
				if (reversestereo)
				{
					uint8_t *p=(uint8_t *)plrbuf+2*bufpos;
					uint8_t *b=(uint8_t *)buf16;
					if (signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[3];
							p[1]=b[1];
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[3];
							p[1]=b[1];
							p+=2;
							b+=4;
						}
					} else {
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[3]^0x80;
							p[1]=b[1]^0x80;
							p+=2;
							b+=4;
						}
						p=(uint8_t *)plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[3]^0x80;
							p[1]=b[1]^0x80;
							p+=2;
							b+=4;
						}
					}
				} else {
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
				}
			} else {
				uint8_t *p=(uint8_t *)plrbuf+bufpos;
				uint8_t *b=(uint8_t *)buf16;
				if (signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1];
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1];
						p++;
						b+=2;
					}
				} else {
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1]^0x80;
						p++;
						b+=2;
					}
					p=(uint8_t *)plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1]^0x80;
						p++;
						b+=2;
					}
				}
			}
		}
		bufpos+=bufdelta+pass2;
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
			plrClearBuf((uint16_t *)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, signedout);
			if (pass2)
				plrClearBuf((uint16_t *)plrbuf, pass2<<stereo, signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, signedout);
			plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t *)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

/*
	fprintf(stderr, "max_ch=%d\n", opl->opl->max_ch);
	for (int i=0;i<opl->opl->max_ch;i++)
	{
		fprintf(stderr, "kcode=%02x fc=%08x ksl_base=%08x keyon=%02x\n", opl->opl->P_CH[i].kcode, opl->opl->P_CH[i].fc, opl->opl->P_CH[i].ksl_base, opl->opl->P_CH[i].keyon);
	}
*/

	if (plrIdle)
		plrIdle();

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
