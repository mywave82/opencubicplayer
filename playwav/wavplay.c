/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * WAVPlay - wave file player
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
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -added a few lines in idle routine to make win95 background
 *     playing possible
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "wave.h"
#include "dev/deviplay.h"
#include "dev/mixclip.h"
#include "stuff/imsrtns.h"
#include "stuff/poll.h"

#ifdef WAVE_DEBUG
# define PRINT(fmt, args...) fprintf(stderr, "%s %s: " fmt, __FILE__, __func__, ##args)
#else
# define PRINT(a, ...) do {} while(0)
#endif

static uint8_t stereo;
static uint8_t bit16;
static uint8_t signedout;
static uint32_t samprate;
static uint8_t reversestereo;
static uint16_t *buf16=0;
static uint32_t bufpos;
static uint32_t buflen;
static void *plrbuf;

static uint16_t *cliptabl=0;
static uint16_t *cliptabr=0;
static uint32_t amplify;
static uint32_t voll,volr;
static int pan;
static int convtostereo;

/*
static binfile *wavefile;
static abinfile rawwave;
*/
static FILE *wavefile;
#define rawwave wavefile


static int wavestereo;
static int wave16bit;
static uint32_t waverate;
static uint32_t wavepos;
static uint32_t wavelen;
static uint32_t waveoffs;
static uint8_t *wavebuf=0;
static uint32_t wavebuflen;
static uint32_t wavebufpos;
static uint32_t wavebuffpos;
static uint32_t wavebufread;
static uint32_t wavebufrate;
static int active;
static int looped;
static int donotloop;
static uint32_t bufloopat;

static int pause;

static volatile int clipbusy=0;
static volatile int readbusy=0;

#ifdef WAVE_DEBUG
static const char *compression_code_str(uint_fast16_t code)
{
	switch (code)
	{
		case 1:
			return "PCM/uncompressed";
		case 2:
			return "Microsoft ADPCM";
		case 3:
			return "Floating point PCM";
		case 5:
			return "Digispeech CVSD / IBM PS/2 Speech Adapter (Motorola MC3418)";
		case 6:
			return "ITU G.711 a-law";
		case 7:
			return "ITU G.711 mu-law";
		case 0x10:
			return "OKI ADPCM";
		case 0x11:
			return "DVI ADPCM";
		case 0x15:
			return "Digispeech DIGISTD";
		case 0x16:
			return "Digispeech DigiFix";
		case 0x17:
			return "IMA ADPCM";
		case 0x20:
			return "ITU G.723 ADPCM (Yamaha)";
		case 0x22:
			return "DSP Group TrueSpeech";
		case 0x31:
			return "GSM6.10";
		case 0x49:
			return "GSM 6.10";
		case 0x64:
			return "ITU G.721 ADPCM";
		case 0x70:
			return "Lernout & Hauspie CELP";
		case 0x72:
			return "Lernout & Hauspie SBC";
		case 0x80:
			return "MPEG";
		case 65535:
			return "Experimental";
		case 0:
		default:
			return "Unknown";
	}
}
#endif

static void calccliptab(int32_t ampl, int32_t ampr)
{
	int i;

	clipbusy++;

	if (!stereo)
	{
		ampl=(abs(ampl)+abs(ampr))>>1;
		ampr=0;
	}

	mixCalcClipTab(cliptabl, abs(ampl));
	mixCalcClipTab(cliptabr, abs(ampr));

	if (signedout)
	{
		for (i=0; i<256; i++)
		{
			cliptabl[i+512]^=0x8000;
			cliptabr[i+512]^=0x8000;
		}
	}

	clipbusy--;
}

#define PANPROC \
do { \
	if(pan==-64 || reversestereo) \
	{ \
		int32_t t=ls; \
		ls=rs; \
		rs=t; \
	} else if(pan==64) \
		; /*do nothing */ \
	else if(pan==0) \
		rs=ls=(rs+ls)/2; \
	else if(pan<0) \
	{ \
		float l=(float)ls / (-pan/-64.0+2.0) + (float)rs*(64.0+pan)/128.0; \
		float r=(float)rs / (-pan/-64.0+2.0) + (float)ls*(64.0+pan)/128.0; \
		ls=r; \
		rs=l; \
	} else if(pan<64) \
	{ \
		float l=(float)ls / (pan/-64.0+2.0) + (float)rs*(64.0-pan)/128.0; \
		float r=(float)rs / (pan/-64.0+2.0) + (float)ls*(64.0-pan)/128.0; \
		ls=l; \
		rs=r; \
	} \
} while(0)


static void timerproc(void)
{
	uint_fast32_t bufplayed;
	uint_fast32_t bufdelta;
	uint_fast32_t pass2;
	uint_fast32_t quietlen = 0;
	uint_fast32_t toloop;

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	bufplayed=plrGetBufPos()>>(stereo+bit16);
	/* if (bufplayed == bufpos), there is no more room for data. This is a special case */
	if (bufplayed==bufpos)
	{
		//PRINT ("%s %s: devp buffer was full, so no write performed\n", __FILE__, __FUNCTION__);
		clipbusy--;
		if (plrIdle)
			plrIdle();
		return;
	}
	/* fill up wave file buffer */
	wpIdle();

	quietlen=0;
	bufdelta=(buflen+bufplayed-bufpos)%buflen;

	if (wavebuflen!=wavelen)
	{
		uint_fast32_t towrap=(unsigned)imuldiv((((wavebuflen+wavebufread-wavebufpos-1)%wavebuflen)>>(wavestereo+wave16bit)), 65536, wavebufrate);
		if (bufdelta>towrap)
		{
			PRINT ("We were going to read past the end of the wavebuffer, bufdelta shrunk from %d to %d\n", bufdelta, towrap);
			bufdelta=towrap;
		}
	}

	if (pause)
	{
		quietlen=bufdelta;
	}

	/* when pitching low, the wave file can appear to be VERY long in samples, cauding overflow, thereby we first need this check */
	if ((wavebufpos + imuldiv(bufdelta, wavebufrate, 65536) + 2) > bufloopat)
	{
		toloop=(unsigned)imuldiv(((bufloopat-wavebufpos)>>(wave16bit+wavestereo)), 65536, wavebufrate);
		if (looped)
			toloop=0;

		if (bufdelta>=toloop)
		{
			looped=1;
			if (donotloop)
				bufdelta=toloop;
		}
	}

	if (bufdelta)
	{
		unsigned int i;

		if ((bufpos+bufdelta)>buflen)
			pass2=bufpos+bufdelta-buflen;
		else
			pass2=0;

		//PRINT ("bufdelta=0x%04x pass2=0x%04x wavebufpos=0x%08x\n", bufdelta, pass2, wavebufpos);

		plrClearBuf(buf16, bufdelta*2, 1);

		if (wave16bit)
		{
			if (wavestereo)
			{
				int32_t wpm1, c0, c1, c2, c3, ls, rs, vm1,v1,v2;
				unsigned wp1, wp2;
				for (i=0; i<bufdelta; i++)
				{

					wpm1=wavebufpos-4; if (wpm1<0) wpm1+=wavebuflen;
					wp1=wavebufpos+4; if (wp1>=wavebuflen) wp1-=wavebuflen;
					wp2=wavebufpos+8; if (wp2>=wavebuflen) wp2-=wavebuflen;

					/* interpolation ? */
					c0 = int16_little(*(uint16_t*)(wavebuf+wavebufpos))^0x8000;
					vm1= int16_little(*(uint16_t*)(wavebuf+wpm1))^0x8000;
					v1 = int16_little(*(uint16_t*)(wavebuf+wp1))^0x8000;
					v2 = int16_little(*(uint16_t*)(wavebuf+wp2))^0x8000;
					c1 = v1-vm1;
					c2 = 2*vm1-2*c0+v1-v2;
					c3 = c0-vm1-v1+v2;
					c3 =  imulshr16(c3,wavebuffpos);
					c3 += c2;
					c3 =  imulshr16(c3,wavebuffpos);
					c3 += c1;
					c3 =  imulshr16(c3,wavebuffpos);
					ls = c3+c0;

					c0 = int16_little(*(uint16_t*)(wavebuf+wavebufpos+2))^0x8000;
					vm1= int16_little(*(uint16_t*)(wavebuf+wpm1+2))^0x8000;
					v1 = int16_little(*(uint16_t*)(wavebuf+wp1+2))^0x8000;
					v2 = int16_little(*(uint16_t*)(wavebuf+wp2+2))^0x8000;
					c1 = v1-vm1;
					c2 = 2*vm1-2*c0+v1-v2;
					c3 = c0-vm1-v1+v2;
					c3 =  imulshr16(c3,wavebuffpos);
					c3 += c2;
					c3 =  imulshr16(c3,wavebuffpos);
					c3 += c1;
					c3 =  imulshr16(c3,wavebuffpos);
					rs = c3+c0;

					PANPROC;
					buf16[2*i]=(uint16_t)ls;
					buf16[2*i+1]=(uint16_t)rs;

					wavebuffpos+=wavebufrate;
					wavebufpos+=(wavebuffpos>>16)*4;
					wavebuffpos&=0xFFFF;
					if (wavebufpos>=wavebuflen)
						wavebufpos-=wavebuflen;
				}
			} else { /* wavestereo */
				int32_t wpm1, c0, c1, c2, c3, vm1,v1,v2;
				uint32_t wp1, wp2;
				for (i=0; i<bufdelta; i++)
				{

					wpm1=wavebufpos-2; if (wpm1<0) wpm1+=wavebuflen;
					wp1=wavebufpos+2; if (wp1>=wavebuflen) wp1-=wavebuflen;
					wp2=wavebufpos+4; if (wp2>=wavebuflen) wp2-=wavebuflen;

					c0 = int16_little(*(uint16_t*)(wavebuf+wavebufpos))^0x8000;
					vm1= int16_little(*(uint16_t*)(wavebuf+wpm1))^0x8000;
					v1 = int16_little(*(uint16_t*)(wavebuf+wp1))^0x8000;
					v2 = int16_little(*(uint16_t*)(wavebuf+wp2))^0x8000;
					c1 = v1-vm1;
					c2 = 2*vm1-2*c0+v1-v2;
					c3 = c0-vm1-v1+v2;
					c3 =  imulshr16(c3,wavebuffpos);
					c3 += c2;
					c3 =  imulshr16(c3,wavebuffpos);
					c3 += c1;
					c3 =  imulshr16(c3,wavebuffpos);
					c3 += c0;

					buf16[2*i]=buf16[2*i+1]=(uint16_t)c3;

					wavebuffpos+=wavebufrate;
					wavebufpos+=(wavebuffpos>>16)*2;
					wavebuffpos&=0xFFFF;
					if (wavebufpos>=wavebuflen)
						wavebufpos-=wavebuflen;
				}
			}
		} else { /* wave16bit */
			if (wavestereo)
				for (i=0; i<bufdelta; i++)
				{
					int32_t ls=wavebuf[wavebufpos]<<8;
					int32_t rs=wavebuf[wavebufpos+1]<<8;

					PANPROC;
					buf16[2*i]=(uint16_t)ls;
					buf16[2*i+1]=(uint16_t)rs;

					wavebuffpos+=wavebufrate;
					wavebufpos+=(wavebuffpos>>16)*2;
					wavebuffpos&=0xFFFF;
					if (wavebufpos>=wavebuflen)
						wavebufpos-=wavebuflen;
				} else /* wavestereo */
					for (i=0; i<bufdelta; i++)
					{
						buf16[2*i+1]=buf16[2*i]=wavebuf[wavebufpos]<<8;
						wavebuffpos+=wavebufrate;
						wavebufpos+=wavebuffpos>>16;
						wavebuffpos&=0xFFFF;
						if (wavebufpos>=wavebuflen)
							wavebufpos-=wavebuflen;
					}
		}

		if (!stereo)
		{
			for (i=0; i<bufdelta; i++)
				buf16[i]=(buf16[2*i]+buf16[2*i+1])>>1;
		}

		if (bit16)
		{
			if (stereo)
			{
				mixClipAlt2((uint16_t*)plrbuf+bufpos*2, buf16, bufdelta-pass2, cliptabl);
				mixClipAlt2((uint16_t*)plrbuf+bufpos*2+1, buf16+1, bufdelta-pass2, cliptabr);
				if (pass2)
				{
					mixClipAlt2((uint16_t*)plrbuf, buf16+2*(bufdelta-pass2), pass2, cliptabl);
					mixClipAlt2((uint16_t*)plrbuf+1, buf16+2*(bufdelta-pass2)+1, pass2, cliptabr);
				}
			} else {
				mixClipAlt((uint16_t*)plrbuf+bufpos, buf16, bufdelta-pass2, cliptabl);
				if (pass2)
					mixClipAlt((uint16_t*)plrbuf, buf16+bufdelta-pass2, pass2, cliptabl);
			}
		} else {
			if (stereo)
			{
				mixClipAlt2(buf16, buf16, bufdelta, cliptabl);
				mixClipAlt2(buf16+1, buf16+1, bufdelta, cliptabr);
			} else
				mixClipAlt(buf16, buf16, bufdelta, cliptabl);
			plr16to8((uint8_t*)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t*)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
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
			plrClearBuf((uint16_t*)plrbuf+(bufpos<<stereo), (bufdelta-pass2)<<stereo, !signedout);
			if (pass2)
				plrClearBuf((uint16_t*)plrbuf, pass2<<stereo, !signedout);
		} else {
			plrClearBuf(buf16, bufdelta<<stereo, !signedout);
			plr16to8((uint8_t*)plrbuf+(bufpos<<stereo), buf16, (bufdelta-pass2)<<stereo);
			if (pass2)
				plr16to8((uint8_t*)plrbuf, buf16+((bufdelta-pass2)<<stereo), pass2<<stereo);
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

void __attribute__ ((visibility ("internal"))) wpIdle(void)
{
	uint32_t bufplayed=plrGetBufPos()>>(stereo+bit16);
	uint32_t bufdelta=(buflen+bufplayed-bufpos)%buflen;
	uint32_t clean;

	if (bufdelta>(buflen>>3))
		timerproc();

	if (readbusy++)
	{
		PRINT ("readbusy kicked us out\n");
		readbusy--;
		return;
	}

	if ((wavelen==wavebuflen)||!active)
	{
		readbusy--;
		return;
	}

	clean=(wavebufpos+wavebuflen-wavebufread)%wavebuflen;
	if (clean*8>wavebuflen)
	{
		while (clean)
		{
			int read=clean;
			int result;

			fseek(rawwave, wavepos+waveoffs, SEEK_SET);
			if ((wavebufread+read)>wavebuflen)
				read=wavebuflen-wavebufread;
			if ((wavepos+read)>=wavelen)
			{
				read=wavelen-wavepos;
				bufloopat=wavebufread+read;
			}
			if (read>0x10000)
				read=0x10000;
			PRINT ("fread %d\n", read);
			result=fread(wavebuf+wavebufread, 1, read, rawwave);
			if (result<=0)
				break;
			wavebufread=(wavebufread+result)%wavebuflen;
			wavepos=(wavepos+result)%wavelen;
			clean-=result;
		}
	}

	readbusy--;
}

uint8_t __attribute__ ((visibility ("internal"))) wpOpenPlayer(FILE *wav, int tostereo, int tolerance)
{
	uint32_t temp;
	uint32_t fmtlen;
	uint16_t sh;

	if (!plrPlay)
		return 0;

	convtostereo=tostereo;

	if (!(cliptabl=malloc(sizeof(uint16_t)*1793)))
	{
		return 0;
	}
	if (!(cliptabr=malloc(sizeof(uint16_t)*1793)))
	{
		free(cliptabl);
		cliptabl=NULL;
		return 0;
	}

	wavefile=wav;
	fseek(wavefile, 0, SEEK_SET);

	if (fread(&temp, sizeof(temp), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #1\n");
		return 0;
	}
	PRINT("comparing header for RIFF: 0x%08x 0x%08x\n", temp, uint32_little(0x46464952));
	if (temp!=uint32_little(0x46464952))
		return 0;

	if (fread(&temp, sizeof(temp), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #2\n");
		return 0;
	}
	PRINT("ignoring next 32bit: 0x%08x\n", temp);

	if (fread(&temp, sizeof(temp), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #3\n");
		return 0;
	}
	PRINT("comparing next header for WAVE: 0x%08x 0x%08x\n", temp, uint32_little(0x45564157));

	if (temp!=uint32_little(0x45564157))
		return 0;

	PRINT("going to locate \"fmt \" header\n");
	while (1)
	{
		if (fread(&temp, sizeof(temp), 1, wavefile) != 1)
		{
			fprintf(stderr, __FILE__ ": fread failed #4\n");
			return 0;
		}
		PRINT("checking 0x%08x 0x%08x\n", temp, uint32_little(0x20746d66));
		if (temp==uint32_little(0x20746D66))
			break;
		if (fread(&temp, sizeof(temp), 1, wavefile) != 1)
		{
			fprintf(stderr, __FILE__ ": fread failed #5\n");
			return 0;
		}
		temp = uint32_little(temp);
		PRINT("failed, skiping next %d bytes\n", temp);
		fseek(wavefile, temp, SEEK_CUR);
	}
	if (fread(&fmtlen, sizeof(fmtlen), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #6\n");
		return 0;
	}
	fmtlen = uint32_little(fmtlen);
	PRINT("fmtlen=%d (must be bigger or equal to 16)\n", fmtlen);
	if (fmtlen<16)
		return 0;
	if (fread(&sh, sizeof(uint16_t), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #7\n");
		return 0;
	}
	sh = uint16_little (sh);
	PRINT("compression code (only 1/pcm is supported): %d %s\n", sh, compression_code_str(sh));
	if ((sh!=1))
	{
		fprintf(stderr, __FILE__ ": not uncomressed raw pcm data\n");
		return 0;
	}

	if (fread(&sh, sizeof(uint16_t), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #8\n");
		return 0;
	}
	sh = uint16_little (sh);
	PRINT("number of channels: %d\n", (int)sh);
	if ((sh==0)||(sh>2))
	{
		fprintf(stderr, __FILE__ ": unsupported number of channels: %d\n", sh);
		return 0;
	}
	wavestereo=(sh==2);

	if (fread(&waverate, sizeof(uint32_t), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #9\n");
		return 0;
	}
	waverate = uint32_little (waverate);
	PRINT("waverate %d\n", (int)waverate);

	if (fread(&temp, sizeof(uint32_t), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #10\n");
		return 0;
	}
#ifdef WAVE_DEBUG
	fprintf(stderr, __FILE__ ": average number of bytes per second: %d\n", (int)(uint32_little(temp)));
#endif

	if (fread(&sh, sizeof(uint16_t), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #11\n");
		return 0;
	}
	PRINT("block align: %d\n", (int)(uint16_little(sh)));

	if (fread(&sh, sizeof(uint16_t), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #12\n");
		return 0;
	}
	sh = uint16_little (sh);
	PRINT("bits per sample: %d\n", (int)sh);
	if ((sh!=8)&&(sh!=16))
	{
		fprintf(stderr, __FILE__ ": unsupported bits per sample: %d\n", (int)sh);
		return 0;
	}
	wave16bit=(sh==16);
	fseek(wavefile, fmtlen-16, SEEK_CUR);

	PRINT("going to locate \"data\" header\n");
	while (1)
	{
		if (fread(&temp, sizeof(uint32_t), 1, wavefile) != 1)
		{
			fprintf(stderr, __FILE__ ": fread failed #13\n");
			return 0;
		}
		PRINT("checking 0x%08x 0x%08x\n", temp, uint32_little(0x61746164));
		if (temp==uint32_little(0x61746164))
			break;
		if (fread(&temp, sizeof(uint32_t), 1, wavefile) != 1)
		{
			fprintf(stderr, __FILE__ ": fread failed #14\n");
			return 0;
		}
		temp = uint32_little (temp);
		PRINT("failed, skiping next %d bytes\n", temp);
		fseek(wavefile, temp, SEEK_CUR);
	}

	if (fread(&wavelen, sizeof(uint32_t), 1, wavefile) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #15\n");
		return 0;
	}
	wavelen = uint32_little (wavelen);
	PRINT("datalength: %d\n", (int)wavelen);
	waveoffs=ftell(wavefile);
	PRINT("waveoffs: %d\n", waveoffs);
	/*rawwave.open(*wavefile, waveoffs, wavelen);*/
	/*fseek(wavefile, waveoffs, SEEK_CUR); */

	if (!wavelen)
	{
		fprintf(stderr, __FILE__ ": no data\n");
		return 0;
	}
	wavebuflen=1024*1024;
	if (wavebuflen>wavelen)
	{
		wavebuflen=wavelen;
		bufloopat=wavebuflen;
	} else
		bufloopat=0x40000000;
	wavebuf=malloc(wavebuflen);
	if (!wavebuf)
	{
		wavebuflen=256*1024;
		wavebuf=malloc(wavebuflen);
		if (!wavebuf)
			return 0;
	}
	wavelen=wavelen&~((1<<(wavestereo+wave16bit))-1);
	wavebufpos=0;
	wavebuffpos=0;
	wavebufread=0;

	if (fread(wavebuf, wavebuflen, 1, rawwave) != 1)
	{
		fprintf(stderr, __FILE__ ": fread failed #16\n");
		return 0;
	}
	wavepos=wavebuflen;

	plrSetOptions(waverate, (convtostereo||wavestereo)?(PLR_STEREO|PLR_16BIT):PLR_16BIT);

	if (!plrOpenPlayer(&plrbuf, &buflen, plrBufSize))
		return 0;

	stereo=!!(plrOpt&PLR_STEREO);
	bit16=!!(plrOpt&PLR_16BIT);
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);
	samprate=plrRate;
	if (abs(samprate-waverate)<((waverate*tolerance)>>16))
		waverate=samprate;

	wavebufrate=imuldiv(65536, waverate, samprate);

	pause=0;
	looped=0;
	amplify=65536;
	voll=256;
	volr=256;
	pan=64;
	calccliptab((amplify*voll)>>8, (amplify*volr)>>8);

	buf16=malloc(sizeof(uint16_t)*(buflen*2));
	if (!buf16)
	{
		plrClosePlayer();
			return 0;
	}

	bufpos=0;

	if (!pollInit(timerproc))
	{
		plrClosePlayer();
		return 0;
	}

	active=1;

	return 1;
}

void __attribute__ ((visibility ("internal"))) wpClosePlayer(void)
{
	active=0;

	PRINT("Freeing resources\n");

	pollClose();

	plrClosePlayer();
	if (wavebuf)
		free(wavebuf);
	if (buf16)
		free(buf16);
	if (cliptabl)
		free(cliptabl);
	if (cliptabr)
		free(cliptabr);
	wavebuf=0;
	buf16=0;
	cliptabl=0;
	cliptabr=0;
/*
	fclose(rawwave);*/
}

char __attribute__ ((visibility ("internal"))) wpLooped(void)
{
	return looped;
}

void __attribute__ ((visibility ("internal"))) wpSetLoop(uint8_t s)
{
	donotloop=!s;
}

void __attribute__ ((visibility ("internal"))) wpPause(uint8_t p)
{
	pause=p;
}

void __attribute__ ((visibility ("internal"))) wpSetAmplify(uint32_t amp)
{
	amplify=amp;
	calccliptab((amplify*voll)>>8, (amplify*volr)>>8);
}

void __attribute__ ((visibility ("internal"))) wpSetSpeed(uint16_t sp)
{
	if (sp<32)
		sp=32;
	wavebufrate=imuldiv(256*sp, waverate, samprate);
}

void __attribute__ ((visibility ("internal"))) wpSetVolume(uint8_t vol_, signed char bal_, signed char pan_, uint8_t opt_)
{
	pan=pan_;
	volr=voll=vol_*4;
	if (bal_<0)
		volr=(volr*(64+bal_))>>6;
	else
		voll=(voll*(64-bal_))>>6;
	wpSetAmplify(amplify);
}

uint32_t __attribute__ ((visibility ("internal"))) wpGetPos(void)
{
	if (wavelen==wavebuflen)
		return wavebufpos>>(wavestereo+wave16bit);
	else
		return ((wavepos+wavelen-wavebuflen+((wavebufpos-wavebufread+wavebuflen)%wavebuflen))%wavelen)>>(wavestereo+wave16bit);
}

void __attribute__ ((visibility ("internal"))) wpGetInfo(struct waveinfo *i)
{
	i->pos=wpGetPos();
	i->len=wavelen>>(wavestereo+wave16bit);
	i->rate=waverate;
	i->stereo=wavestereo;
	i->bit16=wave16bit;
}

void __attribute__ ((visibility ("internal"))) wpSetPos(uint32_t pos)
{
	PRINT("wpSetPos called for pos %lu", (unsigned long)pos);
	pos=((pos<<(wave16bit+wavestereo))+wavelen)%wavelen;
	if (wavelen==wavebuflen)
		wavebufpos=pos;
	else {
		if (((pos+wavebuflen)>wavepos)&&(pos<wavepos))
			wavebufpos=(wavebufread-(wavepos-pos)+wavebuflen)%wavebuflen;
		else {
			wavepos=pos;
			wavebufpos=0;
			wavebufread=1<<(wave16bit+wavestereo);
		}
	}
}
