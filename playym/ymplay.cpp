/* OpenCP Module Player
 * copyright (c) '05-'10 Stian Skjelstad <stian@nixia.no>
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
#include "dev/player.h"
#include "dev/mcp.h"
#include "dev/deviplay.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
#include "dev/plrasm.h"
}
#include "ymplay.h"
#include "stsoundlib/YmMusic.h"

/* options */
static int inpause;
static int looped;

static unsigned long amplify; /* TODO */
static unsigned long voll,volr;
__attribute__ ((visibility ("internal"))) int pan;
__attribute__ ((visibility ("internal"))) int srnd;
/* Are resourses in-use (needs to be freed at Close) ?*/
static int active=0;

/* mcp stuff */
__attribute__ ((visibility ("internal"))) uint16_t vol;
__attribute__ ((visibility ("internal"))) int16_t bal;

/* devp pre-buffer zone */
static uint16_t *buf16; /* here we dump out data before it goes live */
/* devp buffer zone */
static uint32_t devp_bufpos; /* devp write head location */
static uint32_t devp_buflen; /* devp buffer-size in samples */
static void *devp_plrbuf; /* the devp buffer */
static int devp_stereo; /* boolean */
static int devp_bit16; /* boolean */
static int devp_signedout; /* boolean */
static int devp_reversestereo; /* boolean */
static int donotloop=1;

/* ymIdler dumping locations */

#define TIMESLOTS 128
#define REGISTERS 10
struct timeslot
{
	int buffer; /* 0: ymbuf_pre/post */
                    /* 1: buf16 */
                    /* 2: devp */
	unsigned int buffer_offset;
	uint8_t registers[REGISTERS];
} timeslots[TIMESLOTS];
static int timeslot_head_ym;
static int timeslot_head_buf16;
static int timeslot_head_devp;
static int timeslot_tail_devp;

#define YMBUFLEN 16386
static ymsample ymbuf_pre[YMBUFLEN]; /* the buffer */
static int16_t ymbuf_post[YMBUFLEN*2];
static uint32_t ymbufread; /* actually this is the write head */
static uint32_t ymbufpos;  /* read pos */
static uint32_t ymbuffpos; /* read fine-pos.. when ymbufrate has a fraction */
__attribute__ ((visibility ("internal"))) uint32_t ymbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */

__attribute__ ((visibility ("internal"))) CYmMusic *pMusic;

static int (*_GET)(int ch, int opt);
static void (*_SET)(int ch, int opt, int val);

/* clipper threadlock since we use a timer-signal */
static volatile int clipbusy=0;



#define timeslot_debug()
#if 0
static void timeslot_debug(void)
{
	int i;
	for (i=0;i<TIMESLOTS;i++)
	{
		printf("%d", timeslots[i].buffer);
	}
	printf(" %d %d %d %d\n", timeslot_tail_devp, timeslot_head_devp, timeslot_head_buf16, timeslot_head_ym);
	printf(" %d %d\n", ymbufpos, ymbufread);
}
/*
   - - - - - -|2 2 2 2 2 2 2|1 1 1 1 1 1 1 1|0 0 0 0 0 0 0 0|- - - - - - -
              tail_devp     head_devp       head_buf16      head_ym
                conained in DEVP/kernel space
                              contained in buf16
                                             contained in ymbuf_pre/post... can be removed
*/
#endif



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
} while(0)

static void ymSetVolume(void);

void __attribute__ ((visibility ("internal"))) ymClosePlayer(void)
{
	if (active)
	{
		pollClose();
		free(buf16);

		plrClosePlayer();

		mcpSet=_SET;
		mcpGet=_GET;

		ymMusicStop(pMusic);
		ymMusicDestroy(pMusic);

		active=0;
	}
}

void __attribute__ ((visibility ("internal"))) ymMute(int i, int m)
{
	fprintf(stderr, "TODO, ymMute(i, m)\n");
}
static void SET(int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			ymSetSpeed(val);
			break;
		case mcpMasterPitch:
			break;
		case mcpMasterAmplify:
			amplify=val;
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
static int GET(int ch, int opt)
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

int __attribute__ ((visibility ("internal"))) ymOpenPlayer(FILE *file)
{
	void *buffer;
	long length;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (length < 0)
	{
		fprintf(stderr, "[ymplay.cpp]: Unable to determine file length\n");
		return 0;
	}
	buffer = malloc(length);
	if (!buffer)
	{
		fprintf(stderr, "[ymplay.cpp]: Unable to malloc()\n");
		return 0;
	}
	if (fread(buffer, length, 1, file) != 1)
	{
		fprintf(stderr, "[ymplay.cpp]: Unable to read file\n");
		free(buffer);
		return 0;
	}

	plrSetOptions(44100, (PLR_SIGNEDOUT|PLR_16BIT)|PLR_STEREO);

	_SET=mcpSet;
	_GET=mcpGet;
	mcpSet=SET;
	mcpGet=GET;
	mcpNormalize(0);

	devp_stereo=!!(plrOpt&PLR_STEREO);
	devp_bit16=!!(plrOpt&PLR_16BIT);
	devp_signedout=!!(plrOpt&PLR_SIGNEDOUT);
	devp_reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

	looped = 0;

	pMusic = new CYmMusic(plrRate);
	if (!pMusic)
	{
		fprintf(stderr, "[ymplay.cpp]: Unable to create stymulator object\n");
		free(buffer);
		mcpSet=_SET;
		mcpGet=_GET;
		return 0;
	}
	if (!pMusic->loadMemory(buffer, length))
	{
		fprintf(stderr, "[ymplay.cpp]: Unable to load file: %s\n", pMusic->getLastError());
		free(buffer);
		mcpSet=_SET;
		mcpGet=_GET;
		return 0;
	}

	free(buffer);

	ymbufrate=0x10000; /* 1.0 */
	ymbufpos=0;
	ymbuffpos=0;
	ymbufread=sizeof(ymsample); /* 1 << (stereo + bit16) */

	if (!plrOpenPlayer(&devp_plrbuf, &devp_buflen, plrBufSize * plrRate / 1000))
	{
		fprintf(stderr, "[ymplay.cpp]: plrOpenPlayer() failed\n");
		goto error_out;
	}

	if (!(buf16=(uint16_t *)malloc(sizeof(uint16_t)*devp_buflen*2)))
	{
		fprintf(stderr, "[ymplay.cpp]: malloc buf16 failed\n");
		plrClosePlayer();
		goto error_out;
	}
	devp_bufpos=0;

	if (!pollInit(ymIdle))
	{
		fprintf(stderr, "[ymplay.cpp]: pollInit() failed\n");
		free(buf16);
		plrClosePlayer();
		goto error_out;
	}

	active=1;
	return 1;

error_out:
	mcpSet=_SET;
	mcpGet=_GET;

	delete(pMusic);
	return 0;
}

void __attribute__ ((visibility ("internal"))) ymSetLoop(int loop)
{
	pMusic->setLoopMode(loop);
	donotloop=!loop;
}

int __attribute__ ((visibility ("internal"))) ymIsLooped(void)
{
	return looped==2;
}

void __attribute__ ((visibility ("internal"))) ymPause(uint8_t p)
{
	inpause=p;
}

void __attribute__ ((visibility ("internal"))) ymSetAmplify(uint32_t amp)
{
	amplify=amp;
}

void __attribute__ ((visibility ("internal"))) ymSetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
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

static void ymIdler(void)
{
	size_t clean;

	if (!active)
		return;

	clean=(ymbufpos+YMBUFLEN-ymbufread)%YMBUFLEN;
	if (clean<=1)
		return;
	clean-=1;

	while (clean>0)
	{
		size_t read=clean;

		if (looped)
			break;

		/* check for buffer wrapping */
		if ((ymbufread+read)>YMBUFLEN)
			read=YMBUFLEN-ymbufread;

		if (read>(plrRate/50))
			read=plrRate/50;

		if (!pMusic->update(ymbuf_pre+ymbufread, read))
			looped=1;

		if (((timeslot_head_ym+1)%TIMESLOTS)!=timeslot_tail_devp)
		{
			timeslots[timeslot_head_ym].buffer=1;
			timeslots[timeslot_head_ym].buffer_offset = ymbufread+read;
			timeslots[timeslot_head_ym].registers[0] = pMusic->readYmRegister(0)|(pMusic->readYmRegister(1)<<8); /* frequency A */
			timeslots[timeslot_head_ym].registers[1] = pMusic->readYmRegister(2)|(pMusic->readYmRegister(3)<<8); /* frequency B */
			timeslots[timeslot_head_ym].registers[2] = pMusic->readYmRegister(4)|(pMusic->readYmRegister(5)<<8); /* frequency C */
			timeslots[timeslot_head_ym].registers[3] = pMusic->readYmRegister(6)&0x1f; /* frequency noise */
			timeslots[timeslot_head_ym].registers[4] = pMusic->readYmRegister(7); /* mixer control */
			timeslots[timeslot_head_ym].registers[5] = pMusic->readYmRegister(8); /* volume A */
			timeslots[timeslot_head_ym].registers[6] = pMusic->readYmRegister(9); /* volume B */
			timeslots[timeslot_head_ym].registers[7] = pMusic->readYmRegister(10); /* volume C */
			timeslots[timeslot_head_ym].registers[8] = pMusic->readYmRegister(11)|(pMusic->readYmRegister(12)<<8); /* frequency envelope */
			timeslots[timeslot_head_ym].registers[9] = pMusic->readYmRegister(13) & 0x0f;  /* envelope shape */

			timeslot_head_ym++;
			if (timeslot_head_ym == TIMESLOTS)
				timeslot_head_ym = 0;
#if 0
		} else {
			printf("Buffer full... %d would be equal with %d if added one\n", timeslot_head_ym, timeslot_tail_devp);
#endif
		}

		{
			/* TODO, asm optimize*/
			ymsample *base_pre = ymbuf_pre+ymbufread;
			int16_t *base_post = ymbuf_post+(ymbufread<<1); /* stereo */
			ymsample rs, ls;
			int len=read;
			while (len)
			{
				rs=base_pre[0];
				ls=base_pre[0];
				PANPROC;
				if (srnd)
					rs^=~0;
				base_post[0]=(int16_t)rs;
				base_post[1]=(int16_t)ls;
				base_pre+=1;
				base_post+=2;
				len--;
			}
		}
		ymbufread=(ymbufread+read)%YMBUFLEN;
		clean-=read;
	}
	timeslot_debug();
}

static struct channel_info_t Registers;
static void ymUpdateRegisters(void)
{
#if 0
/*	int bufplayed;

	bufplayed=plrGetBufPos()>>(devp_stereo+devp_bit16);*/
#else
	plrGetBufPos();
#endif
	while (1)
	{
		if (timeslot_tail_devp == timeslot_head_devp)
			break;
		/* if mark was infront, we can't reach it yet */
		if (ymbufread < ymbufpos)
		{
/*
0123456789
--|     |-
X    X   X
*/
			if (timeslots[timeslot_tail_devp].buffer_offset > ymbufpos)
				break;
		} else {
/*
0123456789
  |-----|
X    X   X
*/
		/* this logic might be faulty */
			if ((timeslots[timeslot_tail_devp].buffer_offset > ymbufpos) && (timeslots[timeslot_tail_devp].buffer_offset < ymbufread))
				break;

		}
		if (timeslots[timeslot_tail_devp].registers[0]==0)
			Registers.frequency_a = 0;
		else
			Registers.frequency_a = pMusic->readYmClock() / (timeslots[timeslot_tail_devp].registers[0] * 16);
		if (timeslots[timeslot_tail_devp].registers[1]==0)
			Registers.frequency_b = 0;
		else
			Registers.frequency_b = pMusic->readYmClock() / (timeslots[timeslot_tail_devp].registers[1] * 16);
		if (timeslots[timeslot_tail_devp].registers[2]==0)
			Registers.frequency_c = 0;
		else
			Registers.frequency_c = pMusic->readYmClock() / (timeslots[timeslot_tail_devp].registers[2] * 16);
		if (timeslots[timeslot_tail_devp].registers[3] == 0)
			Registers.frequency_noise = 0;
		else
			Registers.frequency_noise = pMusic->readYmClock() / (timeslots[timeslot_tail_devp].registers[3] * 16);
		Registers.mixer_control = timeslots[timeslot_tail_devp].registers[4];
		Registers.level_a = timeslots[timeslot_tail_devp].registers[5];
		Registers.level_b = timeslots[timeslot_tail_devp].registers[6];
		Registers.level_c = timeslots[timeslot_tail_devp].registers[7];

		if (timeslots[timeslot_tail_devp].registers[8] == 0)
			Registers.frequency_envelope = 0;
		else
			Registers.frequency_envelope = pMusic->readYmClock() / (timeslots[timeslot_tail_devp].registers[8] * 256);

		Registers.envelope_shape = timeslots[timeslot_tail_devp].registers[9];

		timeslots[timeslot_tail_devp].buffer=4;
		timeslots[timeslot_tail_devp].buffer_offset = 0;
		timeslot_tail_devp++;
		if (timeslot_tail_devp == TIMESLOTS)
			timeslot_tail_devp = 0;
	}
	timeslot_debug();
}

__attribute__ ((visibility ("internal"))) struct channel_info_t *ymRegisters()
{
	ymUpdateRegisters();
	return &Registers;
}

void __attribute__ ((visibility ("internal"))) ymIdle(void)
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

	ymUpdateRegisters();

	quietlen=0;
	/* Where is our devp reading head? */
	bufplayed=plrGetBufPos()>>(devp_stereo+devp_bit16);
	bufdelta=(devp_buflen+bufplayed-devp_bufpos)%devp_buflen;

	/* No delta on the devp? */
	if (!bufdelta)
	{
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}

	/* fill up our buffers */
	ymIdler();

	if (inpause)
		quietlen=bufdelta;
	else
	{
		uint32_t towrap=imuldiv(((YMBUFLEN+ymbufread-ymbufpos-1)%YMBUFLEN), 65536, ymbufrate);
		if (bufdelta>towrap)
		{
			/* will the eof hit inside the delta? */
			/*quietlen=bufdelta-towrap;*/
			bufdelta=towrap;
			if ((looped==1)&&(towrap==0))
				looped=2;
		}
	}

	bufdelta-=quietlen;

	if (bufdelta)
	{
		uint32_t i;
		if (ymbufrate==0x10000) /* 1.0... just copy into buf16 direct until we run out of target buffer or source buffer */
		{
			uint32_t o=0;
			while (o<bufdelta)
			{
				uint32_t w=bufdelta-o;
				if ((YMBUFLEN-ymbufpos)<w)
					w=YMBUFLEN-ymbufpos;
				while (1)
				{
					if (timeslot_head_buf16 == timeslot_head_ym)
						break;
					/* if mark was infront, we can't reach it yet */
					if (timeslots[timeslot_head_buf16].buffer_offset < ymbufpos)
						break;
					/* if mark is after (after the add), we haven't used it yet */
					if (timeslots[timeslot_head_buf16].buffer_offset > (ymbufpos + w))
						break;
					timeslots[timeslot_head_buf16].buffer=2;
					timeslots[timeslot_head_buf16].buffer_offset += o - ymbufpos;
					timeslot_head_buf16++;
					if (timeslot_head_buf16 == TIMESLOTS)
						timeslot_head_buf16 = 0;
				}
				timeslot_debug();

				memcpy(buf16+(o<<1)/*stereo*/, ymbuf_post+(ymbufpos<<1)/*stereo)*/, w<<2/*stereo+16bit*/);
				o+=w;
				ymbufpos+=w;
				if (ymbufpos>=YMBUFLEN)
					ymbufpos-=YMBUFLEN;
			}
		} else { /* re-sample intil we don't have more target-buffer or source-buffer */
			unsigned int pre_pos;
			int32_t c0, c1, c2, c3, ls, rs, vm1, v1, v2, wpm1;
			uint32_t wp1, wp2;
/*
			if ((bufdelta-=2)<0) bufdelta=0;  by my meening, this should be in place   TODO stian */
			for (i=0; i<bufdelta; i++)
			{
				wpm1=ymbufpos-1; if (wpm1<0) wpm1+=YMBUFLEN;
				wp1=ymbufpos+1; if (wp1>=YMBUFLEN) wp1-=YMBUFLEN;
				wp2=ymbufpos+2; if (wp2>=YMBUFLEN) wp2-=YMBUFLEN;

				c0 = *(uint16_t *)(ymbuf_post+(ymbufpos<<1))^0x8000;
				vm1= *(uint16_t *)(ymbuf_post+(wpm1<<1))^0x8000;
				v1 = *(uint16_t *)(ymbuf_post+(wp1<<1))^0x8000;
				v2 = *(uint16_t *)(ymbuf_post+(wp2<<1))^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3, ymbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3, ymbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3, ymbuffpos);
				ls = c3+c0;
				if (ls>65535)
					ls=65535;
				else if (ls<0)
					ls=0;

				c0 = *(uint16_t *)(ymbuf_post+(ymbufpos<<1)+1)^0x8000;
				vm1= *(uint16_t *)(ymbuf_post+(wpm1<<1)+1)^0x8000;
				v1 = *(uint16_t *)(ymbuf_post+(wp1<<1)+1)^0x8000;
				v2 = *(uint16_t *)(ymbuf_post+(wp2<<1)+1)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3, ymbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3, ymbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3, ymbuffpos);
				rs = c3+c0;
				if (rs>65535)
					rs=65535;
				else if(rs<0)
					rs=0;
				buf16[2*i]=(uint16_t)ls^0x8000;
				buf16[2*i+1]=(uint16_t)rs^0x8000;

				pre_pos=ymbufpos;
				ymbuffpos+=ymbufrate;
				ymbufpos+=(ymbuffpos>>16);
				ymbuffpos&=0xFFFF;
				while (1)
				{
					if (timeslot_head_buf16 == timeslot_head_ym)
						break;

					if (ymbufpos>=YMBUFLEN)
					{

						/* if we wrap, we only need to check post_add is bigger */
						if (timeslots[timeslot_head_buf16].buffer_offset > ymbufpos)
							break;
					} else {
						/* if mark was infront, we can't reach it yet */
						if (timeslots[timeslot_head_buf16].buffer_offset < pre_pos)
							break;
						/* if mark is after (after the add), we haven't used it yet */
						if (timeslots[timeslot_head_buf16].buffer_offset > ymbufpos)
							break;
					}
					timeslots[timeslot_head_buf16].buffer=2;
					timeslots[timeslot_head_buf16].buffer_offset = i;
					timeslot_head_buf16++;
					if (timeslot_head_buf16 == TIMESLOTS)
						timeslot_head_buf16 = 0;
				}
				timeslot_debug();
				if (ymbufpos>=YMBUFLEN)
					ymbufpos-=YMBUFLEN;
			}
		}
		/* when we copy out from buf16, pass the buffer-len that wraps around end-of-buffer till pass2 */
		if ((devp_bufpos+bufdelta)>devp_buflen)
			pass2=devp_bufpos+bufdelta-devp_buflen;
		else
			pass2=0;
		bufdelta-=pass2;

		while (1)
		{
			if (timeslot_head_devp == timeslot_head_buf16)
				break;
			timeslots[timeslot_head_devp].buffer=3;
			if (timeslots[timeslot_head_devp].buffer_offset < bufdelta)
			{
				timeslots[timeslot_head_devp].buffer_offset += devp_bufpos;
			} else {
				timeslots[timeslot_head_devp].buffer_offset -= bufdelta;
			}
			timeslot_head_devp++;
			if (timeslot_head_devp == TIMESLOTS)
				timeslot_head_devp = 0;
		}
		timeslot_debug();
		if (devp_bit16)
		{
			if (devp_stereo)
			{
				if (devp_reversestereo)
				{
					int16_t *p=(int16_t *)devp_plrbuf+2*devp_bufpos;
					int16_t *b=(int16_t *)buf16;
					if (devp_signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1];
							p[1]=b[0];
							p+=2;
							b+=2;
						}
						p=(int16_t *)devp_plrbuf;
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
						p=(int16_t *)devp_plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[1]^0x8000;
							p[1]=b[0]^0x8000;
							p+=2;
							b+=2;
						}
					}
				} else {
					int16_t *p=(int16_t *)devp_plrbuf+2*devp_bufpos;
					int16_t *b=(int16_t *)buf16;
					if (devp_signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[0];
							p[1]=b[1];
							p+=2;
							b+=2;
						}
						p=(int16_t *)devp_plrbuf;
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
						p=(int16_t *)devp_plrbuf;
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
				int16_t *p=(int16_t *)devp_plrbuf+devp_bufpos;
				int16_t *b=(int16_t *)buf16;
				if (devp_signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[0];
						p++;
						b++;
					}
					p=(int16_t *)devp_plrbuf;
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
					p=(int16_t *)devp_plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[0]^0x8000;
						p++;
						b++;
					}
				}
			}
		} else {
			if (devp_stereo)
			{
				if (devp_reversestereo)
				{
					uint8_t *p=(uint8_t *)devp_plrbuf+2*devp_bufpos;
					uint8_t *b=(uint8_t *)buf16;
					if (devp_signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[3];
							p[1]=b[1];
							p+=2;
							b+=4;
						}
						p=(uint8_t *)devp_plrbuf;
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
						p=(uint8_t *)devp_plrbuf;
						for (i=0; i<pass2; i++)
						{
							p[0]=b[3]^0x80;
							p[1]=b[1]^0x80;
							p+=2;
							b+=4;
						}
					}
				} else {
					uint8_t *p=(uint8_t *)devp_plrbuf+2*devp_bufpos;
					uint8_t *b=(uint8_t *)buf16;
					if (devp_signedout)
					{
						for (i=0; i<bufdelta; i++)
						{
							p[0]=b[1];
							p[1]=b[3];
							p+=2;
							b+=4;
						}
						p=(uint8_t *)devp_plrbuf;
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
						p=(uint8_t *)devp_plrbuf;
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
				uint8_t *p=(uint8_t *)devp_plrbuf+devp_bufpos;
				uint8_t *b=(uint8_t *)buf16;
				if (devp_signedout)
				{
					for (i=0; i<bufdelta; i++)
					{
						p[0]=b[1];
						p++;
						b+=2;
					}
					p=(uint8_t *)devp_plrbuf;
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
					p=(uint8_t *)devp_plrbuf;
					for (i=0; i<pass2; i++)
					{
						p[0]=b[1]^0x80;
						p++;
						b+=2;
					}
				}
			}
		}
		devp_bufpos+=bufdelta+pass2;
		if (devp_bufpos>=devp_buflen)
			devp_bufpos-=devp_buflen;
	}

	bufdelta=quietlen;
	if (bufdelta)
	{
		if ((devp_bufpos+bufdelta)>devp_buflen)
			pass2=devp_bufpos+bufdelta-devp_buflen;
		else
			pass2=0;
		if (devp_bit16)
		{
			plrClearBuf((uint16_t *)devp_plrbuf+(devp_bufpos<<devp_stereo), (bufdelta-pass2)<<devp_stereo, !devp_signedout);
			if (pass2)
				plrClearBuf((uint16_t *)devp_plrbuf, pass2<<devp_stereo, !devp_signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<devp_stereo, !devp_signedout);
			plr16to8((uint8_t *)devp_plrbuf+(devp_bufpos<<devp_stereo), buf16, (bufdelta-pass2)<<devp_stereo);
			if (pass2)
				plr16to8((uint8_t *)devp_plrbuf, buf16+((bufdelta-pass2)<<devp_stereo), pass2<<devp_stereo);
		}
		devp_bufpos+=bufdelta;
		if (devp_bufpos>=devp_buflen)
			devp_bufpos-=devp_buflen;
	}

	plrAdvanceTo(devp_bufpos<<(devp_stereo+devp_bit16));

	if (plrIdle)
		plrIdle();

	clipbusy--;
}
