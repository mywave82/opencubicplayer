/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * assembler-emulated version of dwmixa
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

#include "asm_emu/x86.h"

/*static uint32_t (*mixrFadeChannelvoltab)[256];*/
static int32_t ramping[2];
static int32_t (*mixrFadeChannelvoltab)[256];
static uint8_t (*mixrFadeChannelintrtab)[256][2];

void mixrSetupAddresses(int32_t (*vol)[256], uint8_t (*intr)[256][2])
{
	mixrFadeChannelvoltab=vol;
	mixrFadeChannelintrtab=intr;
}

#include <stdio.h>
void mixrFadeChannel(int32_t *fade, struct channel *chan)
{
	if (chan->status&MIXRQ_PLAY16BIT)
	{
		fade[0]+=mixrFadeChannelvoltab[chan->curvols[0]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
		fade[1]+=mixrFadeChannelvoltab[chan->curvols[1]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
	} else {
		fade[0]+=mixrFadeChannelvoltab[chan->curvols[0]][(uint8_t)chan->realsamp.bit8[chan->pos]];
		fade[1]+=mixrFadeChannelvoltab[chan->curvols[1]][(uint8_t)chan->realsamp.bit8[chan->pos]];
	}
	chan->curvols[0]=0;
	chan->curvols[1]=0;
}


static void playmono(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		*(buf++)+=mixrFadeChannelvoltab[vol0][(uint8_t)(chan->realsamp.bit8[pos])];
		fpos+=chan->step&0xffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playmono16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		*(buf++)+=mixrFadeChannelvoltab[vol0][((uint16_t)(chan->realsamp.bit16[pos]))>>8];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playmonoi(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos]))][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos+1]))][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playmonoi16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache =
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos]))>>8][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos+1]))>>8][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playstereo(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=chan->realsamp.bit8[pos];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void playstereo16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=((uint16_t)(chan->realsamp.bit16[pos]))>>8;
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void playstereoi(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos]))][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos+1]))][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void playstereoi16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos]))>>8][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos+1]))>>8][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void routequiet(int32_t *buf, uint32_t len, struct channel *chan)
{
}

typedef void (*route_func)(int32_t *buf, uint32_t len, struct channel *chan);
static const route_func routeptrs[8]=
{
	playmono,
	playmono16,
	playmonoi,
	playmonoi16,
	playstereo,
	playstereo16,
	playstereoi,
	playstereoi16
};

#ifdef I386_ASM_EMU
static void writecallback(uint_fast16_t selector, uint_fast32_t addr, int size, uint_fast32_t data)
{
}

static uint_fast32_t readcallback(uint_fast16_t selector, uint_fast32_t addr, int size)
{
	return 0;
}

void mixrPlayChannel(int32_t *buf, int32_t *fadebuf, uint32_t len, struct channel *chan, int stereo)
{
	int32_t *orgbuf = buf;
	uint32_t orglen = len;

	uint32_t filllen;
/*
        ramping[2];*/
	uint32_t inloop;
	uint32_t ramploop;
	uint32_t dofade;
	route_func routeptr;

	uint32_t step_high;
	uint32_t step_low;
	uint32_t ramping0;
	uint32_t ramping1;
	uint32_t endofbuffer;
	uint32_t bufferlen;

	struct assembler_state_t state;
	init_assembler_state(&state, writecallback, readcallback);

	asm_movl(&state, 0xf0000000, &state.edi); /* &chan == 0xf0000000   only dummy, since we won't deref chan directly */
	asm_testb(&state, MIXRQ_PLAYING, chan->status);

	asm_jz(&state, mixrPlayChannelexit);

	asm_movl(&state, 0, (uint32_t *)&filllen);
	asm_movl(&state, 0, (uint32_t *)&dofade);

	asm_xorl(&state, state.eax, &state.eax);
	asm_cmpl(&state, 0, stereo);
	asm_je(&state, mixrPlayChannelnostereo);
		asm_addl(&state, 4, &state.eax);
mixrPlayChannelnostereo:
	asm_testb(&state, MIXRQ_INTERPOLATE, chan->status);

	asm_jz(&state, mixrPlayChannelnointr);
		asm_addl(&state, 2, &state.eax);
mixrPlayChannelnointr:
	asm_testb(&state, MIXRQ_PLAY16BIT, chan->status);

	asm_jz(&state, mixrPlayChannelpsetrtn);
		asm_incl(&state, &state.eax);
mixrPlayChannelpsetrtn:
	asm_shll(&state, 5, &state.eax);
	asm_addl(&state, 0xf8000000, &state.eax);
	{
		unsigned int tmp = 0;
		asm_movl(&state, state.eax, &tmp);
		tmp-=0xf8000000;
		tmp/=8; /* 8 columns of information in the assembler version */
		if (tmp > sizeof(routeptrs))
		{
			fprintf(stderr, "#GP Exception occurred (tmp=%d)\n", tmp);
			tmp = 0;
		}
		else if (tmp&3)
		{
			fprintf(stderr, "#AC(0) Exception occurred\n");
			tmp = 0;
		}
		routeptr = routeptrs[tmp>>2];
	}

mixrPlayChannelbigloop:
	asm_movl(&state, len, &state.ecx);
	asm_movl(&state, chan->step, &state.ebx);
	asm_movl(&state, chan->pos, &state.edx);
	asm_movw(&state, chan->fpos, &state.si);
	asm_movl(&state, 0, &inloop);
	asm_cmpl(&state, 0, state.ebx);

	asm_je(&state, mixrPlayChannelplayecx);
	asm_jg(&state, mixrPlayChannelforward);
		asm_negl(&state, &state.ebx);
		asm_movl(&state, state.edx, &state.eax);
		asm_testb(&state, MIXRQ_LOOPED, chan->status);

		asm_jz(&state, mixrPlayChannelmaxplaylen);
		asm_cmpl(&state, chan->loopstart, state.edx);
		asm_jb(&state, mixrPlayChannelmaxplaylen);
		asm_subl(&state, chan->loopstart, &state.eax);
		asm_movl(&state, 1, &inloop);
		asm_jmp(&state, mixrPlayChannelmaxplaylen);
mixrPlayChannelforward:
		asm_movl(&state, chan->length, &state.eax);
		asm_negw(&state, &state.si);
		asm_sbbl(&state, state.edx, &state.eax);
		asm_testb(&state, MIXRQ_LOOPED, chan->status);

		asm_jz(&state, mixrPlayChannelmaxplaylen);
		asm_cmpl(&state, chan->loopend, state.edx);
		asm_jae(&state, mixrPlayChannelmaxplaylen);
		asm_subl(&state, chan->length, &state.eax);
		asm_addl(&state, chan->loopend, &state.eax);
		asm_movl(&state, 1, &inloop);

mixrPlayChannelmaxplaylen:
	asm_xorl(&state, state.edx, &state.edx);
	asm_shldl(&state, 16, state.eax, &state.edx);
	asm_shll(&state, 16, &state.esi);
	asm_shldl(&state, 16, state.esi, &state.eax);
	asm_addl(&state, state.ebx, &state.eax);
	asm_adcl(&state, 0, &state.edx);
	asm_subl(&state, 1, &state.eax);
	asm_sbbl(&state, 0, &state.edx);
	asm_cmpl(&state, state.ebx, state.edx);
	asm_jae(&state, mixrPlayChannelplayecx);
	asm_divl(&state, state.ebx);
	asm_cmpl(&state, state.eax, state.ecx);
	asm_jb(&state, mixrPlayChannelplayecx);
		asm_movl(&state, state.eax, &state.ecx);
		asm_cmpl(&state, 0, inloop);
		asm_jnz(&state, mixrPlayChannelplayecx);

		asm_andw(&state, ~MIXRQ_PLAYING, (uint16_t *)&chan->status);

		asm_movl(&state, 1, &dofade);
		asm_movl(&state, len, &state.eax);
		asm_subl(&state, state.ecx, &state.eax);
		asm_addl(&state, state.eax, &filllen);
		asm_movl(&state, state.ecx, &len);

mixrPlayChannelplayecx:
	asm_movl(&state, 0, &ramploop);
	asm_movl(&state, 0, (uint32_t *)&ramping[0]);
	asm_movl(&state, 0, (uint32_t *)&ramping[1]);

	asm_cmpl(&state, 0, state.ecx);
	asm_je(&state, mixrPlayChannelnoplay);

	asm_movl(&state, chan->dstvols[0], &state.edx);
	asm_subl(&state, chan->curvols[0], &state.edx);
	asm_je(&state, mixrPlayChannelnoramp0);
	asm_jl(&state, mixrPlayChannelramp0down);
		asm_movl(&state, 1, (uint32_t *)&ramping[0]);
		asm_cmpl(&state, state.edx, state.ecx);
		asm_jbe(&state, mixrPlayChannelnoramp0);
			asm_movl(&state, 1, &ramploop);
			asm_movl(&state, state.edx, &state.ecx);
			asm_jmp(&state, mixrPlayChannelnoramp0);
mixrPlayChannelramp0down:
	asm_negl(&state, &state.edx);
	asm_movl(&state, -1, (uint32_t *)&ramping[0]);
	asm_cmpl(&state, state.edx, state.ecx);
	asm_jbe(&state, mixrPlayChannelnoramp0);
		asm_movl(&state, 1, &ramploop);
		asm_movl(&state, state.edx, &state.ecx);
mixrPlayChannelnoramp0:

	asm_movl(&state, chan->dstvols[1], &state.edx);
	asm_subl(&state, chan->curvols[1], &state.edx);
	asm_je(&state, mixrPlayChannelnoramp1);
	asm_jl(&state, mixrPlayChannelramp1down);
		asm_movl(&state, 1, (uint32_t *)&ramping[1]);
		asm_cmpl(&state, state.edx, state.ecx);
		asm_jbe(&state, mixrPlayChannelnoramp1);
			asm_movl(&state, 1, &ramploop);
			asm_movl(&state, state.edx, &state.ecx);
			asm_jmp(&state, mixrPlayChannelnoramp1);
mixrPlayChannelramp1down:
		asm_negl(&state, &state.edx);
		asm_movl(&state, -1, (uint32_t *)&ramping[1]);
		asm_cmpl(&state, state.edx, state.ecx);
		asm_jbe(&state, mixrPlayChannelnoramp1);
			asm_movl(&state, 1, &ramploop);
			asm_movl(&state, state.edx, &state.ecx);
mixrPlayChannelnoramp1:

	asm_movl(&state, /*(uint32_t)routeptr*/0x12345678, &state.edx); /* NC value */
	asm_cmpl(&state, 0, ramping[0]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_cmpl(&state, 0, ramping[1]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_cmpl(&state, 0, chan->curvols[0]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_cmpl(&state, 0, chan->curvols[1]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_movl(&state, /*(uint32_t)routq*/0x12345678, &state.edx);
		routeptr = routequiet; /* work-around */

mixrPlayChannelnotquiet:
	asm_movl(&state, 0x12345678+4, &state.ebx);
	asm_movl(&state, chan->step, &state.eax);
	asm_shll(&state, 16, &state.eax);
	asm_movl(&state, state.eax, &step_high); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+8, &state.ebx);
	asm_movl(&state, chan->step, &state.eax);
	asm_sarl(&state, 16, &state.eax);
	asm_movl(&state, state.eax, &step_low); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+12, &state.ebx);
	asm_movl(&state, ramping[0], &state.eax);
	asm_shll(&state, 8, &state.eax);
	asm_movl(&state, state.eax, &ramping0); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+16, &state.ebx);
	asm_movl(&state, ramping[1], &state.eax);
	asm_shll(&state, 8, &state.eax);
	asm_movl(&state, state.eax, &ramping1); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+20, &state.ebx);
	bufferlen = state.ecx;
	endofbuffer = buf[state.ecx];
	asm_leal(&state, state.ecx*4, &state.eax);
	asm_cmpl(&state, 0, stereo);
	asm_je(&state, mixrPlayChannelm1);
		asm_shll(&state, 1, &state.eax);
mixrPlayChannelm1:
	asm_addl(&state, 0x10000000, &state.eax); /* buf */
	//asm_movl(&state, eax, *(uint32_t **)(state.ebx));  we save these above */

	asm_pushl(&state, state.ecx);
	/* asm_movl(&state, *(uint32_t *)(state.edx)), &state.eax); */ asm_movl(&state, 0x12345678, &state.eax);

	asm_movl(&state, chan->curvols[0], &state.ebx);
	asm_shll(&state, 8, &state.ebx);
	asm_movl(&state, chan->curvols[1], &state.ecx);
	asm_shll(&state, 8, &state.ecx);
	asm_movw(&state, chan->fpos, &state.dx);
	asm_shll(&state, 16, &state.edx);
	asm_movl(&state, chan->pos, &state.esi);
	asm_addl(&state, /*chan->samp*/0x12345678, &state.esi);
	asm_movl(&state, 0x10000000, &state.edi); /* buf */

	/* asm_call(&state, *(uint32_t **)(state.eax)); */
	if (buf<orgbuf)
	{
		fprintf(stderr, "#GP buf<orgbuf\n");
	} else if ((buf+(len<<stereo)) > (orgbuf+(orglen<<stereo)))
	{
		fprintf(stderr, "#GP (buf+len) > (orgbuf+orglen)\n");
	} else
		routeptr(buf, bufferlen, chan);

	asm_popl(&state, &state.ecx);
	asm_movl(&state, 0xf0000000, &state.edi); /* &chan == 0xf0000000   only dummy, since we won't deref chan directly */

mixrPlayChannelnoplay:
	asm_movl(&state, state.ecx, &state.eax);
	asm_shll(&state, 2, &state.eax);
	asm_cmpl(&state, 0, stereo);
	asm_je(&state, mixrPlayChannelm2);
		asm_shll(&state, 1, &state.eax);
mixrPlayChannelm2:
	/*asm_addl(&state, state.eax, &buf);*/ buf = (int32_t *)((char *)buf + state.eax);
	/*asm_subl(&state, state.ecx, &len);*/ len -= state.ecx;

	asm_movl(&state, chan->step, &state.eax);
	asm_imul_1_l(&state, state.ecx);
	asm_shldl(&state, 16, state.eax, &state.edx);
	asm_addw(&state, state.eax, &chan->fpos);
	asm_adcl(&state, state.edx, &chan->pos);

	asm_movl(&state, ramping[0], &state.eax);
	asm_imul_2_l(&state, state.ecx, &state.eax);
	asm_addl(&state, state.eax, (uint32_t *)chan->curvols+0);
	asm_movl(&state, ramping[1], &state.eax);
	asm_imul_2_l(&state, state.ecx, &state.eax);
	asm_addl(&state, state.eax, (uint32_t *)chan->curvols+1);

	asm_cmpl(&state, 0, ramploop);
	asm_jnz(&state, mixrPlayChannelbigloop);

	asm_cmpl(&state, 0, inloop);
	asm_jz(&state, mixrPlayChannelfill);

	asm_movl(&state, chan->pos, &state.eax);
	asm_cmpl(&state, 0, chan->step);
	asm_jge(&state, mixrPlayChannelforward2);
		asm_cmpl(&state, chan->loopstart, state.eax);
		asm_jge(&state, mixrPlayChannelexit);
		asm_testb(&state, MIXRQ_PINGPONGLOOP, chan->status);

		asm_jnz(&state, mixrPlayChannelpong);
			asm_addl(&state, chan->replen, &state.eax);
			asm_jmp(&state, mixrPlayChannelloopiflen);
mixrPlayChannelpong:
			asm_negl(&state, (uint32_t *)&chan->step);
			asm_negw(&state, &chan->fpos);
			asm_adcl(&state, 0, &state.eax);
			asm_negl(&state, &state.eax);
			asm_addl(&state, chan->loopstart, &state.eax);
			asm_addl(&state, chan->loopstart, &state.eax);
			asm_jmp(&state, mixrPlayChannelloopiflen);
mixrPlayChannelforward2:
		asm_cmpl(&state, chan->loopend, state.eax);
		asm_jb(&state, mixrPlayChannelexit);
		asm_testb(&state, MIXRQ_PINGPONGLOOP, chan->status);

		asm_jnz(&state, mixrPlayChannelping);
			asm_subl(&state, chan->replen, &state.eax);
			asm_jmp(&state, mixrPlayChannelloopiflen);
mixrPlayChannelping:
			asm_negl(&state, (uint32_t *)&chan->step);
			asm_negw(&state, &chan->fpos);
			asm_adcl(&state, 0, &state.eax);
			asm_negl(&state, &state.eax);
			asm_addl(&state, chan->loopend, &state.eax);
			asm_addl(&state, chan->loopend, &state.eax);
mixrPlayChannelloopiflen:
	asm_movl(&state, state.eax, &chan->pos);
	asm_cmpl(&state, 0, len);
	asm_jne(&state, mixrPlayChannelbigloop);
	asm_jmp(&state, mixrPlayChannelexit);

mixrPlayChannelfill:
	asm_cmpl(&state, 0, filllen);
	asm_je(&state, mixrPlayChannelfadechk);
	asm_movl(&state, chan->length, &state.eax);
	asm_movl(&state, state.eax, &chan->pos);
	asm_addl(&state, /*chan->samp*/0x12345678, &state.eax);
	asm_movl(&state, chan->curvols[0], &state.ebx);
	asm_movl(&state, chan->curvols[1], &state.ecx);
	asm_shll(&state, 8, &state.ebx);
	asm_shll(&state, 8, &state.ecx);
	asm_testb(&state, MIXRQ_PLAY16BIT, chan->status);

	asm_jnz(&state, mixrPlayChannelfill16);
		asm_movb(&state, chan->realsamp.bit8[state.eax-0x12345678], &state.bl);
		asm_jmp(&state, mixrPlayChannelfilldo);
mixrPlayChannelfill16:
		asm_movb(&state, ((uint16_t *)chan->realsamp.bit16)[state.eax-0x12345678]>>8, &state.bl);
mixrPlayChannelfilldo:
	asm_movb(&state, state.bl, &state.cl);
	asm_movl(&state, ((uint32_t *)mixrFadeChannelvoltab)[state.ebx], &state.ebx);
	asm_movl(&state, ((uint32_t *)mixrFadeChannelvoltab)[state.ecx], &state.ecx);
	asm_movl(&state, filllen, &state.eax);
	asm_movl(&state, /*buf*/0x30000000, &state.edi);
	asm_cmpl(&state, 0, stereo);
	asm_jne(&state, mixrPlayChannelfillstereo);
mixrPlayChannelfillmono:
/*
		asm_addl(&state, ebx, &(uint32_t *)state.edi);*/ buf[(state.edi-0x30000000)/4] += state.ebx;
		asm_addl(&state, 4, &state.edi);
	asm_decl(&state, &state.eax);
	asm_jnz(&state, mixrPlayChannelfillmono);
	asm_jmp(&state, mixrPlayChannelfade);
mixrPlayChannelfillstereo:
/*
		asm_addl(&state, ebx, &(uint32_t *)state.edi);*/ buf[(state.edi-0x30000000)/4] += state.ebx;
/*
		asm_addl(&state, ecx, &(uint32_t *)(state.edi+4));*/ buf[(state.edi-0x30000000+4)/4] += state.ecx;
		asm_addl(&state, 8, &state.edi);
	asm_decl(&state, &state.eax);
	asm_jnz(&state, mixrPlayChannelfillstereo);
	asm_jmp(&state, mixrPlayChannelfade);

mixrPlayChannelfadechk:
	asm_cmpl(&state, 0, dofade);
	asm_je(&state, mixrPlayChannelexit);
mixrPlayChannelfade:
	asm_movl(&state, /*chan*/0xf0000000, &state.edi);
	asm_movl(&state, /*fadebuf*/0xe0000000, &state.esi);
	/*asm_call(&state, mixrFadeChannel);*/ mixrFadeChannel(fadebuf, chan);
mixrPlayChannelexit:
	{
	}
}
