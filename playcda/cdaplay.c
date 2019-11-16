/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/devisamp.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/sampler.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"
#include "cdaudio.h"

static int device;
/* 0 = sampler (with no avaible device)
 * 1 = sampler (analog)
 * 2 = player (digital)
 */
static int cfCDAtLineIn;
static int cfCDAdigital;
static int doPause; /* digital playback */

/* devp buffer zone */
static uint32_t bufpos; /* devp write head location */
static uint32_t buflen; /* devp buffer-size in samples */
static void *plrbuf; /* the devp buffer */
static int stereo; /* boolean */
static int bit16; /* boolean */
static int signedout; /* boolean */
static int reversestereo; /* boolean */

/* cdIdler dumping locations */
static uint8_t *cdbuf=NULL; /* the buffer */
static uint32_t cdbuflen;  /* total buffer size */
static uint32_t cdbufread; /* actually this is the write head */
static uint32_t cdbufpos;  /* read pos */
static uint32_t cdbuffpos; /* read fine-pos.. when cdbufrate has a fraction */
static uint32_t cdbufrate; /* re-sampling rate.. fixed point 0x10000 => 1.0 */
static volatile int clipbusy;
static int speed;
static int doLoop;
static int cdflushed; /* all is sent to devp */

/* cdIdle dumping location */
static uint16_t *buf16=NULL;


static int lba_start, lba_stop, lba_next;
static int cd_fd;

#define BUFFER_SLOTS 4
/* TODO, add circular rip buffer */
static struct cdrom_read_audio rip_ioctl;
static unsigned char rip_ioctl_buf[CD_FRAMESIZE_RAW*BUFFER_SLOTS];
static unsigned int rip_pcm_left;

static void cdIdler(void)
{
	size_t clean;

	clean=(cdbufpos+cdbuflen-cdbufread)%cdbuflen;
	if (clean<8)
		return;
	clean-=8;

	while (clean)
	{
		size_t read=clean;

		if (!rip_pcm_left)
		{
			if (lba_next==lba_stop)
			{
				if (doLoop)
					lba_next=lba_start;
				else
					return;
			}
			rip_ioctl.addr.lba=lba_next;
			rip_ioctl.addr_format=CDROM_LBA;
			rip_ioctl.nframes=lba_stop-lba_next;
			if (rip_ioctl.nframes>BUFFER_SLOTS)
				rip_ioctl.nframes=BUFFER_SLOTS;
			rip_ioctl.buf=rip_ioctl_buf;
			if (ioctl(cd_fd, CDROMREADAUDIO, &rip_ioctl)<0)
			{
				perror("ioctl(cd_fd, CDROMREADAUDIO, &rip_ioctl)");
				return;
			}

			rip_pcm_left=CD_FRAMESIZE_RAW*rip_ioctl.nframes;
			lba_next+=rip_ioctl.nframes;
		}

		if (read>rip_pcm_left)
			read=rip_pcm_left;
		if ((cdbufread+read)>cdbuflen)
			read=cdbuflen-cdbufread;
		memcpy(cdbuf+cdbufread, rip_ioctl_buf+(CD_FRAMESIZE_RAW*BUFFER_SLOTS)-rip_pcm_left, read);
		cdbufread=(cdbufread+read)%cdbuflen;
		clean-=read;
		rip_pcm_left-=read;
	}
}

void __attribute__ ((visibility ("internal"))) cdIdle(void)
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
	cdIdler();

	if (doPause)
	{
		cdflushed=0;
		quietlen=bufdelta;
	} else {
		uint32_t towrap=imuldiv((((cdbuflen+cdbufread-cdbufpos-4)%cdbuflen)>>(1 /* we are always given stereo */ + 1 /* we are always given 16bit */)), 65536, cdbufrate);
		cdflushed=!towrap; /* dirty hack to tell if all pcm-data has been sent.. */

		if (bufdelta>towrap)
		{
			/* will the eof hit inside the delta? */
			/*quietlen=bufdelta-towrap;*/
			bufdelta=towrap;
#if 0
			/* TODO */
			if (eof) /* make sure we did hit eof, and just not out of data situasion due to streaming latency */
				looped=1;
#endif
		}
	}

	bufdelta-=quietlen;

	if (bufdelta)
	{
		uint32_t i;
		if (cdbufrate==0x10000) /* 1.0... just copy into buf16 direct until we run out of target buffer or source buffer */
		{
			uint32_t o=0;
			while (o<bufdelta)
			{
				uint32_t w=(bufdelta-o)*4;
				if ((cdbuflen-cdbufpos)<w)
					w=cdbuflen-cdbufpos;
				memcpy(buf16+2*o, cdbuf+cdbufpos, w);
				o+=w>>2;
				cdbufpos+=w;
				if (cdbufpos>=cdbuflen)
					cdbufpos-=cdbuflen;
			}
		} else { /* re-sample intil we don't have more target-buffer or source-buffer */
			int32_t c0, c1, c2, c3, ls, rs, vm1,v1,v2, wpm1;
			uint32_t wp1, wp2;
/*
			if ((bufdelta-=2)<0) bufdelta=0;  by my meening, this should be in place   TODO stian */
			for (i=0; i<bufdelta; i++)
			{
				wpm1=cdbufpos-4; if (wpm1<0) wpm1+=cdbuflen;
				wp1=cdbufpos+4; if (wp1>=cdbuflen) wp1-=cdbuflen;
				wp2=cdbufpos+8; if (wp2>=cdbuflen) wp2-=cdbuflen;

				c0 = *(uint16_t *)(cdbuf+cdbufpos)^0x8000;
				vm1= *(uint16_t *)(cdbuf+wpm1)^0x8000;
				v1 = *(uint16_t *)(cdbuf+wp1)^0x8000;
				v2 = *(uint16_t *)(cdbuf+wp2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,cdbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,cdbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,cdbuffpos);
				ls = c3+c0;
				if (ls>65535)
					ls=65535;
				else if (ls<0)
					ls=0;

				c0 = *(uint16_t *)(cdbuf+cdbufpos+2)^0x8000;
				vm1= *(uint16_t *)(cdbuf+wpm1+2)^0x8000;
				v1 = *(uint16_t *)(cdbuf+wp1+2)^0x8000;
				v2 = *(uint16_t *)(cdbuf+wp2+2)^0x8000;
				c1 = v1-vm1;
				c2 = 2*vm1-2*c0+v1-v2;
				c3 = c0-vm1-v1+v2;
				c3 =  imulshr16(c3,cdbuffpos);
				c3 += c2;
				c3 =  imulshr16(c3,cdbuffpos);
				c3 += c1;
				c3 =  imulshr16(c3,cdbuffpos);
				rs = c3+c0;
				if (rs>65535)
					rs=65535;
				else if(rs<0)
					rs=0;
				buf16[2*i]=(uint16_t)ls^0x8000;
				buf16[2*i+1]=(uint16_t)rs^0x8000;

				cdbuffpos+=cdbufrate;
				cdbufpos+=(cdbuffpos>>16)*4;
				cdbuffpos&=0xFFFF;
				if (cdbufpos>=cdbuflen)
					cdbufpos-=cdbuflen;
			}
		}
		/* when we copy out of buf16, pass the buffer-len that wraps around end-of-buffer till pass2 */
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
			plrClearBuf((uint16_t *)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, !signedout);
			if (pass2)
				plrClearBuf((uint16_t *)plrbuf, pass2<<stereo, !signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, !signedout);
			plr16to8((uint8_t *)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t *)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;
	}

	plrAdvanceTo(bufpos<<(stereo+bit16));

	if (plrIdle)
		plrIdle();

	clipbusy--;
}

void __attribute__ ((visibility ("internal"))) cdSetSpeed(unsigned short sp)
{
	if (!cfCDAdigital)
		return;

	if (sp<32)
		sp=32;

	speed=sp;

	cdbufrate=imuldiv(256*sp, 44100, plrRate);
}

int __attribute__ ((visibility ("internal"))) cdIsCDDrive(int fd)
{
	if (ioctl(fd, CDROM_GET_CAPABILITY, 0)>=0)
		return 1;
	return 0;
}

void __attribute__ ((visibility ("internal"))) cdPause(int fd)
{
	doPause=1;

	if (!cfCDAdigital)
	{
		if (ioctl(fd, CDROMPAUSE))
			perror("cdaplay: ioctl(fd, CDROMPAUSE)");
	}
}

void __attribute__ ((visibility ("internal"))) cdStop(int fd)
{
	doPause=1;

	if (!cfCDAdigital)
	{
		if (ioctl(fd, CDROMPAUSE))
			perror("cdaplay: ioctl(fd, CDROMPAUSE)");
	}

	if (device==1)
		smpCloseSampler();
	else if (device==2)
	{
		pollClose();
		plrStop();
		if (buf16)
		{
			free(buf16);
			buf16=NULL;
		}
		if (cdbuf)
		{
			free(cdbuf);
			cdbuf=NULL;
		}
	}
}

void __attribute__ ((visibility ("internal"))) cdRestart(int fd)
{
	doPause=0;

	if (!cfCDAdigital)
	{
		if (ioctl(fd, CDROMRESUME))
			perror("cdaplay: ioctl(fd, CDROMRESUME)");
	}
}

unsigned short __attribute__ ((visibility ("internal"))) cdGetTracks(int fd, unsigned long *starts, unsigned char *first, unsigned short maxtracks)
{
	int min=0, max=0, i;
	struct cdrom_tochdr tochdr;
	struct cdrom_tocentry tocentry;

	*first=0;
	if (!ioctl(fd, CDROMREADTOCHDR, &tochdr))
	{
		if ((min=tochdr.cdth_trk0)<0)
			min=0;
		max=tochdr.cdth_trk1;
		if (max>maxtracks)
			max=maxtracks;
		for (i=min;i<=max;i++)
		{
			tocentry.cdte_track=i;
			tocentry.cdte_format= CDROM_LBA;
			if (!ioctl(fd, CDROMREADTOCENTRY, &tocentry))
				starts[i-min]=tocentry.cdte_addr.lba;
			else {
				perror("cdaplay: ioctl(fd, CDROMREADTOCENTRY, &tocentry)");
				max=i-1;
			}
		}
		tocentry.cdte_track=CDROM_LEADOUT;
		tocentry.cdte_format= CDROM_LBA;
		if (!ioctl(fd, CDROMREADTOCENTRY, &tocentry))
			starts[max+1-min]=tocentry.cdte_addr.lba;
		else {
			perror("cdaplay: ioctl(fd, CDROMREADTOCENTRY, &tocentry)");
			max-=1;
		}
	} else
		perror("cdaplay: ioctl(fd, CDROMREADTOCHDR, &tochdr)");
	if (max<0)
		min=max=0;
	*first=min;
	return max-min;
}

void __attribute__ ((visibility ("internal"))) cdRestartAt(int fd, unsigned long start)
{
	doPause=0;

	lba_next=start;

	if (cfCDAdigital)
	{
		cd_fd=fd;
		rip_pcm_left=0;
	} else {
		struct cdrom_blk blk;
		blk.from=lba_next;
		blk.len=lba_stop;

		if (ioctl(fd, CDROMPLAYBLK, &blk))
			perror("cdaplay: ioctl(fd, CDROMPLAYBLK, &blk)");
	}
}


int __attribute__ ((visibility ("internal"))) cdPlay(int fd, unsigned long start, unsigned long len)
{
	cfCDAtLineIn=cfGetProfileBool2(cfSoundSec, "sound", "cdsamplelinein", 0, 0); /* moved from global initclose */
	cfCDAdigital=cfGetProfileBool2(cfSoundSec, "sound", "digitalcd", 1, 1);

	doPause=0;

	lba_next=lba_start=start;
	lba_stop=start+len;

	if (cfCDAdigital)
	{
		cd_fd=fd;
		clipbusy=0;
		rip_pcm_left=0;

		cdbuflen=88200; /* 0.5 seconds */
		if (!(cdbuf=malloc(cdbuflen)))
			return -1;
		cdbufpos=0;
		cdbuffpos=0;
		cdbufread=1<<(1/* stereo */+1 /*16bit*/);

		plGetMasterSample=plrGetMasterSample;
		plGetRealMasterVolume=plrGetRealMasterVolume;

		plrSetOptions(44100, (PLR_SIGNEDOUT|PLR_16BIT)|PLR_STEREO);
		stereo=!!(plrOpt&PLR_STEREO);
		bit16=!!(plrOpt&PLR_16BIT);
		signedout=!!(plrOpt&PLR_SIGNEDOUT);
		reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

		if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize * plrRate / 1000))
		{
			free(cdbuf);
			cdbuf=NULL;
			return -1;
		}

		if (!(buf16=malloc(sizeof(uint16_t)*buflen*2)))
		{
			plrClosePlayer();
			free(cdbuf);
			cdbuf=NULL;
			return -1;
		}
		bufpos=0;

		cdSetSpeed(256);
		cdSetLoop(1);

		if (!pollInit(cdIdle))
		{
			free(buf16);
			buf16=NULL;
			plrClosePlayer();
			free(cdbuf);
			cdbuf=NULL;
			return -1;
		}
		device=2;
	} else {
		struct cdrom_blk blk;
		blk.from=start;
		blk.len=len;

		if (!smpSample)
			device=0;
		else {
			void *buf;
			int len;

			plGetMasterSample=smpGetMasterSample;
			plGetRealMasterVolume=smpGetRealMasterVolume;
			smpSetSource(cfCDAtLineIn?SMP_LINEIN:SMP_CD);
			smpSetOptions(plsmpRate, plsmpOpt);
			if (!smpOpenSampler(&buf, &len, smpBufSize))
				return -1;
			device=1;
		}
		if (ioctl(fd, CDROMPLAYBLK, &blk))
			perror("cdaplay: ioctl(fd, CDROMPLAYBLK, &blk)");
	}

	return 0;
}

void __attribute__ ((visibility ("internal"))) cdGetStatus(int fd, struct cdStat *stat)
{
	if (!cfCDAdigital)
	{
		struct cdrom_subchnl subchn;

		subchn.cdsc_format=CDROM_LBA;
		if (ioctl(fd, CDROMSUBCHNL, &subchn))
		{
			perror("cdaplay: ioctl(fd, CDROMSUBCHNL, &subchn)");
			stat->paused=0;
			stat->error=1;
			stat->looped=0;
			return;
		}
		switch (subchn.cdsc_audiostatus)
		{
			case CDROM_AUDIO_NO_STATUS:
			case CDROM_AUDIO_PLAY:
				stat->paused=0;
				stat->error=0;
				stat->looped=0;
				break;
			case CDROM_AUDIO_PAUSED:
			case CDROM_AUDIO_COMPLETED:
				stat->paused=0;
				stat->looped=1;
				stat->error=0;
				break;
			default:
				stat->error=1;
				stat->paused=0;
				stat->looped=0;
		}
		stat->position=subchn.cdsc_absaddr.lba;
		stat->speed=256;
	} else {
		/* TODO */
		stat->error=0;
		stat->paused=doPause;
		stat->position=lba_next; /* TODO, needs feedback */
		stat->speed=(doPause?0:speed);
		stat->looped=(lba_next==lba_stop)&&(!doLoop)&&(!rip_pcm_left)&&cdflushed;
	}
}

void __attribute__ ((visibility ("internal"))) cdSetLoop(int loop)
{
	doLoop=loop;
}

