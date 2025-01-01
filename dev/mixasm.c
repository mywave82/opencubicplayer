/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Mixer asm routines for display etc
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
 *  -ss04????   Stian Skjelstad <stian@nixia.no>
 *    -ported to gcc assembler
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made assembler optimizesafe
 */

#include "config.h"
#include <stddef.h>
#include <unistd.h>
#include "types.h"
#include "mix.h"
#include "mixasm.h"

int8_t (*mixIntrpolTab)[256][2];
int16_t (*mixIntrpolTab2)[256][2];

uint32_t mixAddAbs(const struct mixchannel *chan, uint32_t len)
{
	unsigned long retval=0;

	if (chan->status&MIX_PLAY16BIT)
	{
		int replen=chan->replen;
		int16_t *pos=chan->realsamp.fmt16+chan->pos; /* actual position */
		int16_t *end=chan->realsamp.fmt16+chan->length; /* actual EOS */
		int16_t *target=pos+len; /* target EOS */
		while(1)
		{
			int16_t *_temptarget=target;
			if (_temptarget<end)
			{
				replen=0;
			} else {
				_temptarget=end;
			}
			do {
				int16_t sample;
				sample=*pos++;
				if (sample<0)
				{
					sample=0-sample;
				}
				retval+=sample;
			} while (pos<_temptarget);

			if (!replen)
				goto exit;
			target-=replen;
			pos-=replen;
		}

	} else if (chan->status&MIX_PLAYFLOAT)
	{
		int replen=chan->replen;
		float *pos=chan->realsamp.fmtfloat+chan->pos; /* actual position */
		float *end=chan->realsamp.fmtfloat+chan->length; /* actual EOS */
		float *target=pos+len; /* target EOS */
		while(1)
		{
			float *_temptarget=target;
			if (_temptarget<end)
			{
				replen=0;
			} else {
				_temptarget=end;
			}
			do {
				float sample;
				sample=*pos++;
				if (sample<0)
				{
					sample=0-sample;
				}
				retval+=sample;
			} while (pos<_temptarget);
			if (!replen)
				goto exit;
			target-=replen;
			pos-=replen;
		}

	} else { /* 8 bit*/
		int replen=chan->replen;
		int8_t *pos=chan->realsamp.fmt8+chan->pos; /* actual position */
		int8_t *end=chan->realsamp.fmt8+chan->length; /* actual EOS */
		int8_t *target=pos+len; /* target EOS */
		while(1)
		{
			int8_t *_temptarget=target;
			if (_temptarget<end)
			{
				replen=0;
			} else {
				_temptarget=end;
			}
			do {
				int8_t sample;
				sample=*pos++;
				if (sample<0)
				{
					sample=0-sample;
				}
				retval+=sample;
			} while (pos<_temptarget);

			if (!replen)
				goto exit;
			target-=replen;
			pos-=replen;
		}
	}
exit:
	return retval;
}

void mixClip(int16_t *dst, const int32_t *src, uint32_t len, int16_t (*tab)[256], int32_t max)
{
	int16_t *amp1=tab[0];
	int16_t *amp2=tab[1];
	int16_t *amp3=tab[2];
	int32_t min=~max;
	int16_t minv =
		tab[0][min&0xff] +
		tab[1][(min&0xff00)>>8] +
		tab[2][(min&0xff0000)>>16];
	int16_t maxv =
		tab[0][max&0xff] +
		tab[1][(max&0xff00)>>8] +
		tab[2][(max&0xff0000)>>16];
	int16_t *endp=dst+len;

	do
	{
		if ((*src)<min)
		{
			*(dst++)=minv;
		} else if ((*src)>max)
		{
			*(dst++)=maxv;
		} else
		{
			*(dst++)=amp1[(*src)&0xff]+
			         amp2[((*src)&0xff00)>>8]+
			         amp3[((*src)&0xff0000)>>16];
		}
		src++;
	} while (dst<endp);
}

static uint32_t *voltabs[2]={0,0};

static void playmono(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][*src];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playstereo(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][*src];
		*(dst++)+=voltabs[1][*src];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoi(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0]
			[
				(uint8_t)(
					mixIntrpolTab[fpos>>12][src[0]][0]+
					mixIntrpolTab[fpos>>12][src[1]][1]
				)
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playstereoi(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0]
			[
				(uint8_t)(
					mixIntrpolTab[fpos>>12][src[0]][0]+
					mixIntrpolTab[fpos>>12][src[1]][1]
				)
			];
		*(dst++)+=voltabs[1]
			[
				(uint8_t)(
					mixIntrpolTab[fpos>>12][src[0]][0]+
					mixIntrpolTab[fpos>>12][src[1]][1]
				)
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoir(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]][0]+
			       mixIntrpolTab2[fpos>>11][src[1]][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playstereoir(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint8_t *src=(uint8_t *)ch->realsamp.fmt8+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]][0]+
			       mixIntrpolTab2[fpos>>11][src[1]][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		*(dst++)+=voltabs[1]
			[
				(vol>>8)
			]
			+
			voltabs[1]
			[
				(vol&255)
				+256
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}
static void playmono16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][(*src)>>8];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playstereo16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		*(dst++)+=voltabs[0][(*src)>>8];
		*(dst++)+=voltabs[1][(*src)>>8];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoi16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;


	while (len)
	{
		*(dst++)+=voltabs[0]
			[
				(uint8_t)(
					mixIntrpolTab[fpos>>12][src[0]>>8][0]+
					mixIntrpolTab[fpos>>12][src[1]>>8][0]
				)
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playstereoi16(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;


	while (len)
	{
		*(dst++)+=voltabs[0]
			[
			(uint8_t)(mixIntrpolTab[fpos>>12][src[0]>>8][0]+
			mixIntrpolTab[fpos>>12][src[1]>>8][0])
			];
		*(dst++)+=voltabs[1]
			[
			(uint8_t)(mixIntrpolTab[fpos>>12][src[0]>>8][0]+
			mixIntrpolTab[fpos>>12][src[1]>>8][0])
			];

		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmonoi16r(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]>>8][0]+
			       mixIntrpolTab2[fpos>>11][src[1]>>8][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playstereoi16r(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	uint16_t *src=(uint16_t *)ch->realsamp.fmt16+ch->pos; /* Remove sign */
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;

	while (len)
	{
		uint16_t vol = mixIntrpolTab2[fpos>>11][src[0]>>8][0]+
			       mixIntrpolTab2[fpos>>11][src[1]>>8][1];
		*(dst++)+=voltabs[0]
			[
				(vol>>8)
			]
			+
			voltabs[0]
			[
				(vol&255)
				+256
			];
		*(dst++)+=voltabs[1]
			[
				(vol>>8)
			]
			+
			voltabs[1]
			[
				(vol&255)
				+256
			];
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playmono32(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	float scale = ch->vol.volfs[0] * 64.0;
	float *src = ch->realsamp.fmtfloat+ch->pos;
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;
	while (len)
	{
		*(dst++)+=(int32_t)((*src)*scale);
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}

static void playstereof(int32_t *dst, uint32_t len, struct mixchannel *ch)
{
	float scale0 = ch->vol.volfs[0] * 64.0;
	float scale1 = ch->vol.volfs[1] * 64.0;
	float *src = ch->realsamp.fmtfloat+ch->pos;
	uint32_t fpos = ch->fpos;
	uint16_t fadd = ch->step&0xffff;
	int16_t add = ch->step>>16;
	while (len)
	{
		*(dst++)+=(int32_t)((*src)*scale0);
		*(dst++)+=(int32_t)((*src)*scale1);
		if ((fpos+=fadd)>=0x10000)
		{
			fpos-=0x10000;
			src++;
		}
		src+=add;
		len--;
	}
}
void mixPlayChannel(int32_t *dst, uint32_t len, struct mixchannel *ch, int stereo)
{
	void (*playrout)(int32_t *dst, uint32_t len, struct mixchannel *ch);

	int interpolate=0;
	int play16bit=0;
	int playfloat=0;
	int render16bit=0;
	int32_t step;
	uint16_t fpos;
	int inloop;
	uint32_t eax;
	uint64_t pos;
	uint32_t mylen;

	if (!(ch->status&MIX_PLAYING))
		return;
	if (ch->status&MIX_INTERPOLATE)
	{
		interpolate=1;
		render16bit=ch->status&MIX_MAX;
	}

	play16bit=(ch->status&MIX_PLAY16BIT);

	playfloat=(ch->status&MIX_PLAYFLOAT);

	if (stereo)
	{
		voltabs[0]=ch->vol.voltabs[0];
		voltabs[1]=ch->vol.voltabs[1];
		if (playfloat)
			playrout=playstereof; /* this overrides all other settings, since we only have one (dummy) 32bit player odd */
		else {
			if (interpolate)
			{
				if (render16bit)
				{
					if (play16bit)
						playrout=playstereoi16r;
					else
						playrout=playstereoir;
				} else {
					if (play16bit)
						playrout=playstereoi16;
					else
						playrout=playstereoi;
				}
			} else  { /* no interpolate */
				if (play16bit)
					playrout=playstereo16;
				else
					playrout=playstereo;
			}
		}
	} else {
		voltabs[0]=ch->vol.voltabs[0];

		if (playfloat)
			playrout=playmono32; /* this overrides all other settings, since we only have one 32bit player mono */
		else {
			if (interpolate)
			{
				if (render16bit)
				{
					if (play16bit)
						playrout=playmonoi16r;
					else
						playrout=playmonoir;
				} else {
					if (play16bit)
						playrout=playmonoi16;
					else
						playrout=playmonoi;
				}
			} else  { /* no interpolate */
				if (play16bit)
					playrout=playmono16;
				else
					playrout=playmono;
			}
		}
	}

	do
	{
/*mixPlayChannel_bigloop:*/
		inloop=0;
		step=ch->step; /* ebx */
		fpos=ch->fpos; /* si */
		mylen=len;
		/* count until end of sample/fold... eax */
		if (!step)
			return;
		if (step>0)
		{
/*mixPlayChannel_forward:*/
			eax=ch->length-ch->pos;
			if ((fpos^=-1))
				eax--;
			if ((ch->status&MIX_LOOPED)&&(ch->pos<ch->loopend))
			{
				eax-=(ch->length-ch->loopend);
				inloop=1;
			}
		} else /*if (step<0)*/
		{
/*'mixP	layChannel_backforward:*/
			step=~step;
			eax=ch->pos;
			if ((ch->status&MIX_LOOPED)&&(ch->pos>=ch->loopstart))
			{
				eax-=ch->loopstart;
				inloop=1;
			}
		}
/*mixPlayChannel_maxplaylen:*/
		/* eax now contains numbers of samples left in input */
		pos=((eax<<16)|fpos)+ch->step;
		if ((pos>>32)<=ch->step)
		{/* avoid overflow exception */
			eax=pos/ch->step;
			if (eax<=mylen)
			{
				if (!inloop)
				{
					ch->status&=(MIX_ALL-MIX_PLAYING);
				}
				mylen=eax;
			}
		}

/*mixPlayChannel_playecx:*/
		playrout(dst, mylen, ch);

		dst+=mylen<<(!!stereo);
		len-=mylen;

		pos=((uint64_t)ch->pos<<16)|ch->fpos;
		pos+=(int64_t)mylen*(int64_t)ch->step;

		if (!inloop)
			break;

		eax=ch->pos;

		if (ch->step<0)
		{
			if (eax>=ch->loopstart)
				break;
			if (!(ch->status&MIX_PINGPONGLOOP))
			{
				eax+=ch->replen;
			} else {
/*mixPlayChannel_pong:*/
				ch->step=-ch->step;
				if ((ch->fpos=-ch->fpos))
					eax++;
				eax=-eax;
				eax+=ch->loopstart;
				eax+=ch->loopstart;
			}
		} else {
/*mixPlayChannel_forward2:*/
			if (eax<ch->loopend)
				break;

			if (ch->status&MIX_PINGPONGLOOP)
			{
/*mixPlaychannel_ping:*/
				if ((ch->fpos=-ch->fpos))
					eax++;
				eax=-eax;
				eax+=ch->loopend;
				eax+=ch->loopend;
			} else {
				eax=ch->replen;
			}
		}
/*mixPlayChannel_loopiflen:*/

		ch->pos=eax;
	} while (len);
}
