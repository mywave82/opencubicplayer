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
		if (chan->step & 0x80000000)
		{
			pos+=0xffff0000 | (chan->step>>16);
		} else {
			pos+=chan->step>>16;
		}
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
	uint8_t inloop;
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

	asm_movl(&state, 0xf0000000, &state.edi);          // "  movl %3, %%edi\n" /* &chan == 0xf0000000   only dummy, since we won't deref chan directly */
	asm_testw(&state, MIXRQ_PLAYING, chan->status);    // "  testb %25, %c12(%%edi)\n"      /* %25 = MIXRQ_PLAYING */

	asm_jz(&state, mixrPlayChannelexit);               // "  jz mixrPlayChannelexit\n"

	asm_movl(&state, 0, (uint32_t *)&filllen);         // "  movl $0, %6\n"                 /*  %6 = fillen */
	asm_movl(&state, 0, (uint32_t *)&dofade);          // "  movl $0, %11\n"                /* %11 = dofade */

	asm_xorl(&state, state.eax, &state.eax);           // "  xorl %%eax, %%eax\n"
	asm_cmpl(&state, 0, stereo);                       // "  cmpl $0, %4\n"                 /*  %4 = stereo */
	asm_je(&state, mixrPlayChannelnostereo);           // "  je mixrPlayChannelnostereo\n"
		asm_addl(&state, 4, &state.eax);           // "    addl $4, %%eax\n"
mixrPlayChannelnostereo:
	asm_testw(&state, MIXRQ_INTERPOLATE, chan->status);// "  testb %27, %c12(%%edi)\n"      /* %27 = MIXRQ_INTERPOLATE */
	                                                                                        /* %12 = ch->status */
	asm_jz(&state, mixrPlayChannelnointr);             //"  jz mixrPlayChannelnointr\n
		asm_addl(&state, 2, &state.eax);           //"    addl $2, %%eax\n"
mixrPlayChannelnointr:
	asm_testw(&state, MIXRQ_PLAY16BIT, chan->status);  //"  testb %26, %c12(%%edi)\n"      /* %26 = MIXRQ_PLAY16BIT */
	                                                                                       /* %12 = ch->status */
	asm_jz(&state, mixrPlayChannelpsetrtn);            //"  jz mixrPlayChannelpsetrtn\n"
		asm_incl(&state, &state.eax);              //"    incl %%eax\n"
mixrPlayChannelpsetrtn:
	asm_shll(&state, 5, &state.eax);                   //"  shll $5, %%eax\n"
	asm_addl(&state, 0xf8000000, &state.eax);          //"  addl $routtab, %%eax\n"
	{
		unsigned int tmp = 0;
		asm_movl(&state, state.eax, &tmp);         //"  movl %%eax, %5\n"              /*  %5 = routeptr*/
		tmp-=0xf8000000;
		tmp/=8; /* 8 columns of information in the assembler version */
		if (tmp > sizeof(routeptrs))
		{
			fprintf(stderr, "#GP Exception occured (tmp=%d)\n", tmp);
			tmp = 0;
		}
		else if (tmp&3)
		{
			fprintf(stderr, "#AC(0) Exception occured\n");
			tmp = 0;
		}
		routeptr = routeptrs[tmp>>2];
	}

mixrPlayChannelbigloop:
	asm_movl(&state, len, &state.ecx);                 //"  movl %2, %%ecx\n"              /*  %2 = len */
	asm_movl(&state, chan->step, &state.ebx);          //"  movl %c13(%%edi), %%ebx\n"     /* %13 = ch->step */
	asm_movl(&state, chan->pos, &state.edx);           //"  movl %c14(%%edi), %%edx\n"     /* %14 = ch->pos */
	asm_movw(&state, chan->fpos, &state.si);           //"  movw %c15(%%edi), %%si\n"      /* %15 = ch->fpos */
	asm_movb(&state, 0, &inloop);                      //"  movb $0, %9\n"                 /*  %9 = inloop */
	asm_cmpl(&state, 0, state.ebx);                    //"  cmpl $0, %%ebx\n"

	asm_je(&state, mixrPlayChannelplayecx);            //"  je mixrPlayChannelplayecx\n"
	asm_jg(&state, mixrPlayChannelforward);            //"  jg mixrPlayChannelforward\n"
	asm_negl(&state, &state.ebx);                      //"    negl %%ebx\n"
	asm_movl(&state, state.edx, &state.eax);           //"    movl %%edx, %%eax\n"
	asm_testw(&state, MIXRQ_LOOPED, chan->status);     //"    testb %28, %c12(%%edi)\n"    /* %28 = MIXRQ_LOOPED */
		                                                                               /* %12 = ch->status */

	asm_jz(&state, mixrPlayChannelmaxplaylen);         //"    jz mixrPlayChannelmaxplaylen\n"
	asm_cmpl(&state, chan->loopstart, state.edx);      //"    cmpl %c16(%%edi), %%edx\n"   /* %16 = ch->loopstart */
	asm_jb(&state, mixrPlayChannelmaxplaylen);         //"    jb mixrPlayChannelmaxplaylen\n"
	asm_subl(&state, chan->loopstart, &state.eax);     //"    subl %c16(%%edi), %%eax\n"   /* %16 = ch->loopstart */
	asm_movb(&state, 1, &inloop);                      //"    movb $1, %9\n"               /*  %9 = inloop */
	asm_jmp(&state, mixrPlayChannelmaxplaylen);        //"    jmp mixrPlayChannelmaxplaylen\n"
mixrPlayChannelforward:
	asm_movl(&state, chan->length, &state.eax);        //"    movl %c18(%%edi), %%eax\n"   /* %18 = length */
	asm_negw(&state, &state.si);                       //"    negw %%si\n"

	asm_sbbl(&state, state.edx, &state.eax);           //"    sbbl %%edx, %%eax\n"
	asm_testw(&state, MIXRQ_LOOPED, chan->status);     //"    testb %28, %c12(%%edi)\n"    /* %28 = MIXRQ_LOOPED */
	                                                                                       /* %12 = ch->status */
	asm_jz(&state, mixrPlayChannelmaxplaylen);         //"    jz mixrPlayChannelmaxplaylen\n"
	asm_cmpl(&state, chan->loopend, state.edx);        //"    cmpl %c17(%%edi), %%edx\n"   /* %17 = ch->loopend */
	asm_jae(&state, mixrPlayChannelmaxplaylen);        //"    jae mixrPlayChannelmaxplaylen\n"
	asm_subl(&state, chan->length, &state.eax);        //"    subl %c18(%%edi), %%eax\n"   /* %18 = ch->length */
	asm_addl(&state, chan->loopend, &state.eax);       //"    addl %c17(%%edi), %%eax\n"   /* %17 = ch->loopend*/
	asm_movb(&state, 1, &inloop);                      //"    movb $1, %9\n"               /*  %9 = inloop */

mixrPlayChannelmaxplaylen:
	asm_xorl(&state, state.edx, &state.edx);           //"  xorl %%edx, %%edx\n"
	asm_shldl(&state, 16, state.eax, &state.edx);      //"  shld $16, %%eax, %%edx\n"
	asm_shll(&state, 16, &state.esi);                  //"  shll $16, %%esi\n"
	asm_shldl(&state, 16, state.esi, &state.eax);      //"  shld $16, %%esi, %%eax\n"
	asm_addl(&state, state.ebx, &state.eax);           //"  addl %%ebx, %%eax\n"
	asm_adcl(&state, 0, &state.edx);                   //"  adcl $0, %%edx\n"
	asm_subl(&state, 1, &state.eax);                   //"  subl $1, %%eax\n"

	asm_sbbl(&state, 0, &state.edx);                   //"  sbbl $0, %%edx\n"
	asm_cmpl(&state, state.ebx, state.edx);            //"  cmpl %%ebx, %%edx\n"
	asm_jae(&state, mixrPlayChannelplayecx);           //"  jae mixrPlayChannelplayecx\n"
	asm_divl(&state, state.ebx);                       //"  divl %%ebx\n"
	asm_cmpl(&state, state.eax, state.ecx);            //"  cmpl %%eax, %%ecx\n"
	asm_jb(&state, mixrPlayChannelplayecx);            //"  jb mixrPlayChannelplayecx\n"
	asm_movl(&state, state.eax, &state.ecx);           //"    movl %%eax, %%ecx\n"
	asm_cmpb(&state, 0, inloop);                       //"    cmpb $0, %9\n"               /*  %9 = inloop */
	asm_jnz(&state, mixrPlayChannelplayecx);           //"    jnz mixrPlayChannelplayecx\n"

	asm_andw(&state, ~MIXRQ_PLAYING, (uint16_t *)&chan->status); // "      andb $254, %c12(%%edi)\n"  /* 254 = 255-MIXRQ_PLAYING */

	asm_movl(&state, 1, &dofade);                      //"      movl $1, %11\n"            /* %11 = dofade */
	asm_movl(&state, len, &state.eax);                 //"      movl %2, %%eax\n"          /*  %2 = len */
	asm_subl(&state, state.ecx, &state.eax);           //"      subl %%ecx, %%eax\n"
	asm_addl(&state, state.eax, &filllen);             //"      addl %%eax, %6\n"          /*  %6 = filllen */
	asm_movl(&state, state.ecx, &len);                 //"      movl %%ecx, %2\n"          /*  %2 = len */

mixrPlayChannelplayecx:
	asm_movl(&state, 0, &ramploop);                    //"  movb $0, %10\n"                /* %10 = ramploop */
	asm_movl(&state, 0, (uint32_t *)&ramping[0]);      //"  movl $0, %7\n"                 /*  %7 = ramping[0] */
	asm_movl(&state, 0, (uint32_t *)&ramping[1]);      //"  movl $0, %8\n"                 /*  %8 = ramping[1] */

	asm_cmpl(&state, 0, state.ecx);                    //"  cmpl $0, %%ecx\n"
	asm_je(&state, mixrPlayChannelnoplay);             //"  je mixrPlayChannelnoplay\n"

	asm_movl(&state, chan->dstvols[0], &state.edx);    //"  movl %c21(%%edi), %%edx\n"     /* %21 = ch->dstvols[0] */
	asm_subl(&state, chan->curvols[0], &state.edx);    //"  subl %c19(%%edi), %%edx\n"     /* %19 = ch->curvols[0] */
	asm_je(&state, mixrPlayChannelnoramp0);            //"  je mixrPlayChannelnoramp0\n"
	asm_jl(&state, mixrPlayChannelramp0down);          //"  jl mixrPlayChannelramp0down\n"
	asm_movl(&state, 1, (uint32_t *)&ramping[0]);      //"    movl $1, %7\n"               /*  %7 = ramping[0] */
	asm_cmpl(&state, state.edx, state.ecx);            //"    cmpl %%edx, %%ecx\n"
	asm_jbe(&state, mixrPlayChannelnoramp0);           //"    jbe mixrPlayChannelnoramp0\n"
	asm_movl(&state, 1, &ramploop);                    //"      movb $1, %10\n"            /* %10 = ramploop */
	asm_movl(&state, state.edx, &state.ecx);           //"      movl %%edx, %%ecx\n"
	asm_jmp(&state, mixrPlayChannelnoramp0);           //"      jmp mixrPlayChannelnoramp0\n"
mixrPlayChannelramp0down:
	asm_negl(&state, &state.edx);                      //"    negl %%edx\n"
	asm_movl(&state, -1, (uint32_t *)&ramping[0]);     //"    movl $-1, %7\n"              /*  %7 = ramping[0] */
	asm_cmpl(&state, state.edx, state.ecx);            //"    cmpl %%edx, %%ecx\n"
	asm_jbe(&state, mixrPlayChannelnoramp0);           //"    jbe mixrPlayChannelnoramp0\n"
	asm_movl(&state, 1, &ramploop);                    //"      movb $1, %10\n"            /* %10 = ramploop */
	asm_movl(&state, state.edx, &state.ecx);           //"      movl %%edx, %%ecx\n"
mixrPlayChannelnoramp0:

	asm_movl(&state, chan->dstvols[1], &state.edx);    //"  movl %c22(%%edi), %%edx\n"     /* %22 = ch->dstvols[1] */
	asm_subl(&state, chan->curvols[1], &state.edx);    //"  subl %c20(%%edi), %%edx\n"     /* %20 = ch->curvols[1] */
	asm_je(&state, mixrPlayChannelnoramp1);            //"  je mixrPlayChannelnoramp1\n"
	asm_jl(&state, mixrPlayChannelramp1down);          //"  jl mixrPlayChannelramp1down\n"
	asm_movl(&state, 1, (uint32_t *)&ramping[1]);      //"    movl $1, %8\n"               /*  %8 = ramping[4] */
	asm_cmpl(&state, state.edx, state.ecx);            //"    cmpl %%edx, %%ecx\n"
	asm_jbe(&state, mixrPlayChannelnoramp1);           //"    jbe mixrPlayChannelnoramp1\n"
	asm_movl(&state, 1, &ramploop);                    //"      movb $1, %10\n"            /* %10 = ramploop */
	asm_movl(&state, state.edx, &state.ecx);           //"      movl %%edx, %%ecx\n"
	asm_jmp(&state, mixrPlayChannelnoramp1);           //"      jmp mixrPlayChannelnoramp1\n"
mixrPlayChannelramp1down:
	asm_negl(&state, &state.edx);                      //"    negl %%edx\n"
	asm_movl(&state, -1, (uint32_t *)&ramping[1]);     //"    movl $-1, %8\n"              /*  %8 = ramping[1] */
	asm_cmpl(&state, state.edx, state.ecx);            //"    cmpl %%edx, %%ecx\n"
	asm_jbe(&state, mixrPlayChannelnoramp1);           //"    jbe mixrPlayChannelnoramp1\n"
	asm_movl(&state, 1, &ramploop);                    //"      movb $1, %10\n"            /* %10 = ramploop */
	asm_movl(&state, state.edx, &state.ecx);           //"      movl %%edx, %%ecx\n"
mixrPlayChannelnoramp1:

	asm_movl(&state, /*(uint32_t)routeptr*/0x12345678, &state.edx); /* NC value */ //"  movl %5, %%edx\n"              /*  %5 = routptr */
	asm_cmpl(&state, 0, ramping[0]);                   //"  cmpl $0, %7\n"                 /*  %7 = ramping[0] */
	asm_jne(&state, mixrPlayChannelnotquiet);          //"  jne mixrPlayChannelnotquiet\n"
	asm_cmpl(&state, 0, ramping[1]);                   //"  cmpl $0, %8\n"                 /*  %8 = ramping[1] */
	asm_jne(&state, mixrPlayChannelnotquiet);          //"  jne mixrPlayChannelnotquiet\n"
	asm_cmpl(&state, 0, chan->curvols[0]);             //"  cmpl $0, %c19(%%edi)\n"        /* %19 = ch->curvols[0] */
	asm_jne(&state, mixrPlayChannelnotquiet);          //"  jne mixrPlayChannelnotquiet\n"
	asm_cmpl(&state, 0, chan->curvols[1]);             //"  cmpl $0, %c20(%%edi)\n"        /* %20 = ch->curvols[1] */
	asm_jne(&state, mixrPlayChannelnotquiet);          //"  jne mixrPlayChannelnotquiet\n"
	asm_movl(&state, /*(uint32_t)routq*/0x12345678, &state.edx); // "    movl $routq, %%edx\n"
	routeptr = routequiet; /* work-around */

mixrPlayChannelnotquiet:
	asm_movl(&state, 0x12345678+4, &state.ebx);        //"  movl 4(%%edx), %%ebx\n"
	asm_movl(&state, chan->step, &state.eax);          //"  movl %c13(%%edi), %%eax\n"     /* %13 = ch->step */
	asm_shll(&state, 16, &state.eax);                  //"  shll $16, %%eax\n"
	asm_movl(&state, state.eax, &step_high); /* ref ebx to reach step_high */ //"  movl %%eax, (%%ebx)\n"
	asm_movl(&state, 0x12345678+8, &state.ebx);        //"  movl 8(%%edx), %%ebx\n"
	asm_movl(&state, chan->step, &state.eax);          //"  movl %c13(%%edi), %%eax\n"     /* %13 = ch->step */
	asm_sarl(&state, 16, &state.eax);                  //"  sarl $16, %%eax\n"
	asm_movl(&state, state.eax, &step_low); /* ref ebx to reach step_low */ //"  movl %%eax, (%%ebx)\n"
	asm_movl(&state, 0x12345678+12, &state.ebx);       //"  movl 12(%%edx), %%ebx\n"
	asm_movl(&state, ramping[0], &state.eax);          //"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
	asm_shll(&state, 8, &state.eax);                   //"  shll $8, %%eax\n"
	asm_movl(&state, state.eax, &ramping0); /* ref ebx to reach ramping0 */ //"  movl %%eax, (%%ebx)\n"
	asm_movl(&state, 0x12345678+16, &state.ebx);       //"  movl 16(%%edx), %%ebx\n"
	asm_movl(&state, ramping[1], &state.eax);          //"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
	asm_shll(&state, 8, &state.eax);                   //"  shll $8, %%eax\n"
	asm_movl(&state, state.eax, &ramping1); /* ref ebx to reach ramping1 */ //"  movl %%eax, (%%ebx)\n"
	asm_movl(&state, 0x12345678+20, &state.ebx);       //"  movl 20(%%edx), %%ebx\n"
	bufferlen = state.ecx;
	endofbuffer = buf[state.ecx];
	asm_leal(&state, state.ecx*4, &state.eax);         //"leal (,%%ecx,4), %%eax\n"
	asm_cmpl(&state, 0, stereo);                       //"  cmpl $0, %4\n"
	asm_je(&state, mixrPlayChannelm1);                 //"  je mixrPlayChannelm1\n"
	asm_shll(&state, 1, &state.eax);                   //"    shll $1, %%eax\n"
mixrPlayChannelm1:
	asm_addl(&state, 0x10000000, &state.eax); /* buf */ //"  addl %0, %%eax\n"              /*  %0 = buf */
	//asm_movl(&state, eax, *(uint32_t **)(state.ebx));  we save these above */ //   movl %%eax, (%%ebx)\n"
	asm_pushl(&state, state.ecx);                       //"  pushl %%ecx\n"
	asm_movl(&state, 0x12345678, &state.eax);           //"  movl (%%edx), %%eax\n"

	asm_movl(&state, chan->curvols[0], &state.ebx);     //"  movl %c19(%%edi), %%ebx\n"     /* %19 = ch->curvols[0] */
	asm_shll(&state, 8, &state.ebx);                    //"  shll $8, %%ebx\n"
	asm_movl(&state, chan->curvols[1], &state.ecx);     //"  movl %c20(%%edi), %%ecx\n"     /* %20 = ch->curvols[1] */
	asm_shll(&state, 8, &state.ecx);                    //"  shll $8, %%ecx\n"
	asm_movw(&state, chan->fpos, &state.dx);            //"  movw %c15(%%edi), %%dx\n"      /* %15 = ch->fpos */
	asm_shll(&state, 16, &state.edx);                   //"  shll $16, %%edx\n"
	asm_movl(&state, chan->pos, &state.esi);            //"  movl %c14(%%edi), %%esi\n"     /* %14 = ch->chpos */
	asm_addl(&state, /*chan->samp*/0x12345678, &state.esi);//"  addl %c23(%%edi), %%esi\n"     /* %23 = ch->samp */
	asm_movl(&state, 0x10000000, &state.edi); /* buf */ //"  movl %0, %%edi\n"              /*  %0 = buf */

	/* asm_call(&state, *(uint32_t **)(state.eax)); */
	if (buf<orgbuf)
	{
		fprintf(stderr, "#GP buf<orgbuf\n");
	} else if ((buf+(len<<stereo)) > (orgbuf+(orglen<<stereo)))
	{
		fprintf(stderr, "#GP (buf+len) > (orgbuf+orglen)\n");
	} else {
		routeptr(buf, bufferlen, chan);
	}

	asm_popl(&state, &state.ecx);                       // "  popl %%ecx\n"
	asm_movl(&state, 0xf0000000, &state.edi); /* &chan == 0xf0000000   only dummy, since we won't deref chan directly */ //"  movl %3, %%edi\n"              /*  %3 = chan */

mixrPlayChannelnoplay:
	asm_movl(&state, state.ecx, &state.eax);            //"  movl %%ecx, %%eax\n"
	asm_shll(&state, 2, &state.eax);                    //"  shll $2, %%eax\n"
	asm_cmpl(&state, 0, stereo);                        //"  cmpl $0, %4\n"                 /*  %4 = stereo */
	asm_je(&state, mixrPlayChannelm2);                  //"  je mixrPlayChannelm2\n"
	asm_shll(&state, 1, &state.eax);                    //"    shll $1, %%eax\n"
mixrPlayChannelm2:
	buf = (int32_t *)((char *)buf + state.eax);         //"  addl %%eax, %0\n"              /*  %0 = buf */
	len -= state.ecx;                                   //"  subl %%ecx, %2\n"              /*  %2 = len */

	asm_movl(&state, chan->step, &state.eax);           //"  movl %c13(%%edi), %%eax\n"     /* %13 = ch->step */
	asm_imul_1_l(&state, state.ecx);                    //"  imul %%ecx\n"
	asm_shldl(&state, 16, state.eax, &state.edx);       //"  shld $16, %%eax, %%edx\n"
	asm_addw(&state, state.eax, &chan->fpos);           //"  addw %%ax, %c15(%%edi)\n"      /* %15 = ch->fpos */
	asm_adcl(&state, state.edx, &chan->pos);            //"  adcl %%edx, %c14(%%edi)\n"     /* %14 = ch->pos */

	asm_movl(&state, ramping[0], &state.eax);           //"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
	asm_imul_2_l(&state, state.ecx, &state.eax);        //"  imul %%ecx, %%eax\n"
	asm_addl(&state, state.eax, (uint32_t *)chan->curvols+0); //  "  addl %%eax, %c19(%%edi)\n"     /* %19 = ch->curvols[0] */
	asm_movl(&state, ramping[1], &state.eax);           //"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
	asm_imul_2_l(&state, state.ecx, &state.eax);        //"  imul %%ecx, %%eax\n"
	asm_addl(&state, state.eax, (uint32_t *)chan->curvols+1); // "  addl %%eax, %c20(%%edi)\n"     /* %20 = ch->curvols[1] */

	asm_cmpl(&state, 0, ramploop);                      //"  cmpb $0, %10\n"                /* %10 = ramploop */
	asm_jnz(&state, mixrPlayChannelbigloop);            //"  jnz mixrPlayChannelbigloop\n"

	asm_cmpb(&state, 0, inloop);                        //"  cmpb $0, %9\n"                 /*  %9 = inloop */
	asm_jz(&state, mixrPlayChannelfill);                //"  jz mixrPlayChannelfill\n"

	asm_movl(&state, chan->pos, &state.eax);            //"  movl %c14(%%edi), %%eax\n"     /* %14 = ch->pos */
	asm_cmpl(&state, 0, chan->step);                    //"  cmpl $0, %c13(%%edi)\n"        /* %13 = ch->step */
	asm_jge(&state, mixrPlayChannelforward2);           //"  jge mixrPlayChannelforward2\n"
	asm_cmpl(&state, chan->loopstart, state.eax);       //"    cmpl %c16(%%edi), %%eax\n"   /* %16 = ch->loopstart */
	asm_jge(&state, mixrPlayChannelexit);               //"    jge mixrPlayChannelexit\n"
	asm_testw(&state, MIXRQ_PINGPONGLOOP, chan->status);//"    testb %29, %c12(%%edi)\n"    /* %29 = MIXRQ_PINGPONGLOOP */
	                                                                                        /* %12 = ch->status */
	asm_jnz(&state, mixrPlayChannelpong);               //"    jnz mixrPlayChannelpong\n"
	asm_addl(&state, chan->replen, &state.eax);         //"      addl %c24(%%edi), %%eax\n" /* %24 = ch->replen */
	asm_jmp(&state, mixrPlayChannelloopiflen);          //"      jmp mixrPlayChannelloopiflen\n"
mixrPlayChannelpong:
	asm_negl(&state, (uint32_t *)&chan->step);          //"      negl %c13(%%edi)\n"        /* %13 = ch->step */
	asm_negw(&state, &chan->fpos);                      //"      negw %c15(%%edi)\n"        /* %15 = ch->fpos */
	asm_adcl(&state, 0, &state.eax);                    //"      adcl $0, %%eax\n"
	asm_negl(&state, &state.eax);                       //"      negl %%eax\n"
	asm_addl(&state, chan->loopstart, &state.eax);      //"      addl %c16(%%edi), %%eax\n" /* %16 = ch->loopstart */
	asm_addl(&state, chan->loopstart, &state.eax);      //"      addl %c16(%%edi), %%eax\n" /* %16 = ch->loopstart */
	asm_jmp(&state, mixrPlayChannelloopiflen);          //"      jmp mixrPlayChannelloopiflen\n"
mixrPlayChannelforward2:
	asm_cmpl(&state, chan->loopend, state.eax);         //"    cmpl %c17(%%edi), %%eax\n"   /* %17 = ch->loopend */
	asm_jb(&state, mixrPlayChannelexit);                //"    jb mixrPlayChannelexit\n"
	asm_testw(&state, MIXRQ_PINGPONGLOOP, chan->status);//"    testb %29, %c12(%%edi)\n"    /* %29 = MIXRQ_PINGPONGLOOP */
	                                                                                        /* %12 = ch->status */
	asm_jnz(&state, mixrPlayChannelping);               //"    jnz mixrPlayChannelping\n"
	asm_subl(&state, chan->replen, &state.eax);         //"      subl %c24(%%edi), %%eax\n" /* %24 = ch->replen */
	asm_jmp(&state, mixrPlayChannelloopiflen);          //"      jmp mixrPlayChannelloopiflen\n"
mixrPlayChannelping:
	asm_negl(&state, (uint32_t *)&chan->step);          //"      negl %c13(%%edi)\n"        /* %13 = ch->step */
	asm_negw(&state, &chan->fpos);                      //"      negw %c15(%%edi)\n"        /* %15 = ch->fpos */
	asm_adcl(&state, 0, &state.eax);                    //"      adcl $0, %%eax\n"
	asm_negl(&state, &state.eax);                       //"      negl %%eax\n"
	asm_addl(&state, chan->loopend, &state.eax);        //"      addl %c17(%%edi), %%eax\n" /* %17 = ch->loopend */
	asm_addl(&state, chan->loopend, &state.eax);        //"      addl %c17(%%edi), %%eax\n" /* %17 = ch->loopend */
mixrPlayChannelloopiflen:
	asm_movl(&state, state.eax, &chan->pos);            //"  movl %%eax, %c14(%%edi)\n"     /* %14 = ch->pos */
	asm_cmpl(&state, 0, len);                           //"  cmpl $0, %2\n"                 /*  %2 = len */
	asm_jne(&state, mixrPlayChannelbigloop);            //"  jne mixrPlayChannelbigloop\n"
	asm_jmp(&state, mixrPlayChannelexit);               //"  jmp mixrPlayChannelexit\n"

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
	asm_testw(&state, MIXRQ_PLAY16BIT, chan->status);

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

void mixrFade(int32_t *buf, int32_t *fade, int len, int stereo)
{
	struct assembler_state_t state;
	init_assembler_state(&state, writecallback, readcallback);

	asm_movl (&state, 0x0 /*fade*/, &state.esi);
	asm_movl (&state, 0x0 /*buf */, &state.edi);
	asm_movl (&state, len, &state.ecx);
	asm_movl (&state, stereo, &state.edx);

	asm_movl (&state, *(uint32_t *)((char *)fade+state.esi),   &state.eax); //"  movl (%%esi), %%eax\n"
	asm_movl (&state, *(uint32_t *)((char *)fade+state.esi+4), &state.ebx); //"  movl 4(%%esi), %%ebx\n"
	asm_cmpl (&state, 0, state.edx);                                        //"  cmpl $0, %%edx\n"
	asm_jnz (&state, mixrFadestereo);                                       //"  jnz mixrFadestereo\n"
mixrFadelpm:
	asm_movl (&state, state.eax, (uint32_t *)((char *)buf+state.edi));      //"      movl %%eax, (%%edi)\n"
	asm_movl (&state, state.eax, &state.edx);                               //"      movl %%eax, %%edx\n"
	asm_shll (&state, 7, &state.eax);                                       //"      shll $7, %%eax\n"
	asm_subl (&state, state.edx, &state.eax);                               //"      subl %%edx, %%eax\n"
	asm_sarl (&state, 7, &state.eax);                                       //"      sarl $7, %%eax\n"
	asm_addl (&state, 4, &state.edi);                                       //"      addl $4, %%edi\n"
	asm_decl (&state, &state.ecx);                                          //"    decl %%ecx\n"
	asm_jnz  (&state, mixrFadelpm);                                         //"    jnz mixrFadelpm\n"
	asm_jmp  (&state, mixrFadedone);                                        //"  jmp mixrFadedone\n"
mixrFadestereo:
mixrFadelps:
	asm_movl (&state, state.eax, (uint32_t *)((char *)buf+state.edi));      //"      movl %%eax, (%%edi)\n"
	asm_movl (&state, state.ebx, (uint32_t *)((char *)buf+state.edi+4));    //"      movl %%ebx, 4(%%edi)\n"
	asm_movl (&state, state.eax, &state.edx);                               //"      movl %%eax, %%edx\n"
	asm_shll (&state, 7, &state.eax);                                       //"      shll $7, %%eax\n"
	asm_subl (&state, state.edx, &state.eax);                               //"      subl %%edx, %%eax\n"
	asm_sarl (&state, 7, &state.eax);                                       //"      sarl $7, %%eax\n"
	asm_movl (&state, state.ebx, &state.edx);                               //"      movl %%ebx, %%edx\n"
	asm_shll (&state, 7, &state.ebx);                                       //"      shll $7, %%ebx\n"
	asm_subl (&state, state.edx, &state.ebx);                               //"      subl %%edx, %%ebx\n"
	asm_sarl (&state, 7, &state.ebx);                                       //"      sarl $7, %%ebx\n"
	asm_addl (&state, 8, &state.edi);                                       //"      addl $8, %%edi\n"
	asm_decl (&state, &state.ecx);                                          //"    decl %%ecx\n"
	asm_jnz (&state, mixrFadelps);                                          //"    jnz mixrFadelps\n"
mixrFadedone:
	asm_movl (&state, state.eax, (uint32_t *)((char *)fade+state.esi));     //"  movl %%eax, (%%esi)\n"
	asm_movl (&state, state.ebx, (uint32_t *)((char *)fade+state.esi+4));   //"  movl %%ebx, 4(%%esi)\n"
}

void mixrClip(void *dst, int32_t *src, int len, void *tab, int32_t max, int b16)
{
	void *mixrClipamp1_4; // for self-modifing code
	void *mixrClipamp2_4; // for self-modifing code
	void *mixrClipamp3_4; // for self-modifing code
	uint32_t mixrClipmax_4; // for self-modifing code
	uint32_t mixrClipmin_4; // for self-modifing code
	uint16_t mixrClipminv_2; // for self-modifing code
	uint16_t mixrClipmaxv_2; // for self-modifing code
	uint32_t mixrClipendpX_4; // for self-modifing code

	struct assembler_state_t state;
	init_assembler_state(&state, writecallback, readcallback);

	asm_movl (&state, 0 /* src */, &state.esi);
	asm_movl (&state, 0 /* dst */, &state.edi);
	asm_movl (&state, len, &state.ecx);
	asm_movl (&state, max, &state.edx);
	asm_movl (&state, b16, &state.eax);
	asm_movl (&state, 0 /* tab */, &state.ebx);

	asm_cmpl (&state, 0, state.eax); //"  cmpl $0, %%eax\n"
	asm_je (&state, mixrClip8);     //"  je mixrClip8_\n"

	mixrClipamp1_4 = tab; //"  movl %%ebx, (mixrClipamp1-4)\n"

	asm_addl (&state, 512, &state.ebx);  //"  addl $512, %%ebx\n"
	mixrClipamp2_4 = (char *)tab+512; //"  movl %%ebx, (mixrClipamp2-4)\n

	asm_addl (&state, 512, &state.ebx);  //"  addl $512, %%ebx\n"
	mixrClipamp3_4 = (char *)tab+1024; //"  movl %%ebx, (mixrClipamp3-4)\n"

	asm_subl (&state, 1024, &state.ebx); //"  subl $1024, %%ebx\n"
	asm_movl (&state, state.edx, &mixrClipmax_4);//"  movl %%edx, (mixrClipmax-4)\n"
	asm_negl (&state, &state.edx);//"  negl %%edx\n"
	asm_movl (&state, state.edx, &mixrClipmin_4);//"  movl %%edx, (mixrClipmin-4)\n"
	asm_xorl (&state, state.edx, &state.edx);//"  xorl %%edx, %%edx\n"

	asm_movb (&state, mixrClipmin_4, &state.dl);//"  movb (mixrClipmin-4), %%dl\n"
	asm_movl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)), &state.eax);//"  movl (%%ebx, %%edx, 2), %%eax\n"
	asm_movb (&state, mixrClipmin_4>>8, &state.dl); //"  movb (mixrClipmin-3), %%dl\n"
	asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+512), &state.eax);//"  addl 512(%%ebx, %%edx, 2), %%eax\n"
	asm_movb (&state, mixrClipmin_4>>16, &state.dl); //"  movb (mixrClipmin-2), %%dl\n"
	asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+1024), &state.eax);//"  addl 1024(%%ebx, %%edx, 2), %%eax\n"
	asm_movw (&state, state.ax, &mixrClipminv_2); //"  movw %%ax, (mixrClipminv-2)\n"

	asm_movb (&state, mixrClipmax_4, &state.dl);//"  movb (mixrClipmax-4), %%dl\n"
	asm_movl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)), &state.eax);//"  movl (%%ebx, %%edx, 2), %%eax\n"
	asm_movb (&state, mixrClipmax_4>>8, &state.dl); //"  movb (mixrClipmax-3), %%dl\n"
	asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+512), &state.eax);//"  addl 512(%%ebx, %%edx, 2), %%eax\n"
	asm_movb (&state, mixrClipmax_4>>16, &state.dl); //"  movb (mixrClipmax-2), %%dl\n"
	asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+1024), &state.eax);//"  addl 1024(%%ebx, %%edx, 2), %%eax\n"
	asm_movw (&state, state.ax, &mixrClipmaxv_2); //"  movw %%ax, (mixrClipmaxv-2)\n"

	//"  leal (%%edi, %%ecx, 2), %%ecx\n"
	//"  movl %%ecx, (mixrClipendp1-4)\n"
	//"  movl %%ecx, (mixrClipendp2-4)\n"
	//"  movl %%ecx, (mixrClipendp3-4)\n"
	mixrClipendpX_4 = state.ecx*2;
	asm_xorl (&state, state.ebx, &state.ebx);//"  xorl %%ebx, %%ebx\n"
	asm_xorl (&state, state.ecx, &state.ecx);//"  xorl %%ecx, %%ecx\n"
	asm_xorl (&state, state.edx, &state.edx);//"  xorl %%edx, %%edx\n"

mixrCliplp:
	asm_cmpl(&state, mixrClipmin_4, *(uint32_t *)((char *)src+state.esi));//"    cmpl $1234, (%%esi)\nmixrClipmin:\n"
	asm_jl (&state, mixrCliplow);//"    jl mixrCliplow\n"
	asm_cmpl(&state, mixrClipmax_4, *(uint32_t *)((char *)src+state.esi));//"    cmpl $1234, (%%esi)\nmixrClipmax:\n"
	asm_jg (&state, mixrCliphigh);//"    jg mixrCliphigh\n"

	asm_movb (&state, *((uint8_t *)src+state.esi), &state.bl);//"    movb (%%esi), %%bl\n"
	asm_movb (&state, *((uint8_t *)src+state.esi+1), &state.cl);//"    movb 1(%%esi), %%cl\n"
	asm_movb (&state, *((uint8_t *)src+state.esi+2), &state.dl);//"    movb 2(%%esi), %%dl\n"
	asm_movl (&state, *(uint32_t *)((uint8_t *)mixrClipamp1_4+(state.ebx<<1)), &state.eax);//"    movl 1234(,%%ebx,2), %%eax\nmixrClipamp1:\n"
	asm_addl (&state, *(uint32_t *)((uint8_t *)mixrClipamp2_4+(state.ecx<<1)), &state.eax);//"    addl 1234(,%%ecx,2), %%eax\nmixrClipamp2:\n"
	asm_addl (&state, *(uint32_t *)((uint8_t *)mixrClipamp3_4+(state.edx<<1)), &state.eax);//"    addl 1234(,%%edx,2), %%eax\nmixrClipamp3:\n"
	*(uint16_t *)((uint8_t*)dst+state.edi) = state.ax;//"    movw %%ax, (%%edi)\n"
	asm_addl (&state, 2, &state.edi);//"    addl $2, %%edi\n"
	asm_addl (&state, 4, &state.esi);//"    addl $4, %%esi\n"
	asm_cmpl (&state, mixrClipendpX_4, state.edi);//"  cmpl $1234, %%edi\nmixrClipendp1:\n"
	asm_jb (&state, mixrCliplp);//"  jb mixrCliplp\n"
	asm_jmp (&state, mixrClipdone);//"  jmp mixrClipdone\n"

mixrCliplow:
	*(uint16_t *)((uint8_t*)dst+state.edi) = mixrClipminv_2;//"    movw $1234, (%%edi)\nmixrClipminv:\n"
	asm_addl (&state, 2, &state.edi);//"    addl $2, %%edi\n"
	asm_addl (&state, 4, &state.esi);//"    addl $4, %%esi\n"
	asm_cmpl (&state, mixrClipendpX_4, state.edi);//"  cmpl $1234, %%edi\nmixrClipendp1:\n"
	asm_jb (&state, mixrCliplp);//"  jb mixrCliplp\n"
	asm_jmp (&state, mixrClipdone);//"  jmp mixrClipdone\n"
mixrCliphigh:
	*(uint16_t *)((uint8_t*)dst+state.edi) = mixrClipmaxv_2;//"    movw $1234, (%%edi)\nmixrClipmaxv:\n"
	asm_addl (&state, 2, &state.edi);//"    addl $2, %%edi\n"
	asm_addl (&state, 4, &state.esi);//"    addl $4, %%esi\n"
	asm_cmpl (&state, mixrClipendpX_4, state.edi);//"  cmpl $1234, %%edi\nmixrClipendp1:\n"
	asm_jb (&state, mixrCliplp);//"  jb mixrCliplp\n"
mixrClipdone:
mixrClip8out:
		return;

	{
		#define mixrClip8amp1_4 mixrClipamp1_4
		#define mixrClip8amp2_4 mixrClipamp2_4
		#define mixrClip8amp3_4 mixrClipamp3_4
		#define mixrClip8max_4 mixrClipmax_4
		#define mixrClip8min_4 mixrClipmin_4
		uint8_t mixrClip8minv_1;
		uint8_t mixrClip8maxv_1;
		#define mixrClip8endpX_4 mixrClipendpX_4
mixrClip8:
		mixrClipamp1_4 = tab; //"  movl %ebx, (mixrClip8amp1-4)\n"

		asm_addl (&state, 512, &state.ebx); //"  addl $512, %%ebx\n"
		mixrClipamp2_4 = (char *)tab+512; //"  movl %ebx, (mixrClip8amp2-4)\n"

		asm_addl (&state, 512, &state.ebx); //"  addl $512, %%ebx\n"
		mixrClipamp3_4 = (char *)tab+1024; //"  movl %ebx, (mixrClip8amp3-4)\n"

		asm_subl (&state, 1024, &state.ebx); //"  subl $1024, %%ebx\n"
		asm_movl (&state, state.edx, &mixrClipmax_4);//"  movl %%edx, (mixrClip8max-4)\n"
		asm_negl (&state, &state.edx);//"  negl %%edx\n"
		asm_movl (&state, state.edx, &mixrClipmin_4);//"  movl %%edx, (mixrClip8min-4)\n"
		asm_xorl (&state, state.edx, &state.edx);//"  xorl %%edx, %%edx\n"

		asm_movb (&state, mixrClipmin_4, &state.dl);//"  movb (mixrClip8min-4), %dl\n"
		asm_movl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)), &state.eax);//"  movl (%ebx, %edx, 2), %eax\n"
		asm_movb (&state, mixrClipmin_4>>8, &state.dl); //"  movb (mixrClip8min-3), %dl\n"
		asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+512), &state.eax);//"  addl 512(%ebx, %edx, 2), %eax\n"
		asm_movb (&state, mixrClipmin_4>>16, &state.dl); //"  movb (mixrClip8min-2), %dl\n"
		asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+1024), &state.eax);//"  addl 1024(%ebx, %edx, 2), %eax\n"
		asm_movb (&state, state.ah, &mixrClip8minv_1);//"  movb %ah, (mixrClip8minv-1)\n"

		asm_movb (&state, mixrClipmax_4, &state.dl);//"  movb (mixrClip8max-4), %dl\n"
		asm_movl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)), &state.eax);//"  movl (%ebx, %edx, 2), %eax\n"
		asm_movb (&state, mixrClipmax_4>>8, &state.dl); //"  movb (mixrClip8max-3), %dl\n"
		asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+512), &state.eax);//"  addl 512(%ebx, %edx, 2), %eax\n"
		asm_movb (&state, mixrClipmax_4>>16, &state.dl); //"  movb (mixrClip8max-2), %dl\n"
		asm_addl (&state, *(uint32_t *)((char *)tab+(state.edx<<1)+1024), &state.eax);//"  addl 1024(%ebx, %edx, 2), %eax\n"
		asm_movb (&state, state.ah, &mixrClip8maxv_1);//"  movb %ah, (mixrClip8maxv-1)\n"

		//"  leal (%ecx, %edi), %ecx\n"
		//"  movl %ecx, (mixrClip8endp1-4)\n"
		//"  movl %ecx, (mixrClip8endp2-4)\n"
		//"  movl %ecx, (mixrClip8endp3-4)\n"
		mixrClip8endpX_4 = state.ecx;
		asm_xorl (&state, state.ebx, &state.ebx);//"  xorl %%ebx, %%ebx\n"
		asm_xorl (&state, state.ecx, &state.ecx);//"  xorl %%ecx, %%ecx\n"
		asm_xorl (&state, state.edx, &state.edx);//"  xorl %%edx, %%edx\n"

mixrClip8lp:
		asm_cmpl(&state, mixrClip8min_4, *(uint32_t *)((char *)src+state.esi));//"  cmpl $1234, (%esi)\nmixrClip8min:\n"
		asm_jl (&state, mixrClip8low);//"  jl mixrClip8low\n"
		asm_cmpl(&state, mixrClip8max_4, *(uint32_t *)((char *)src+state.esi));//"  cmpl $1234, (%esi)\nmixrClip8max:\n"
		asm_jg (&state, mixrClip8high);//"  jg mixrClip8high\n"

		asm_movb (&state, *((uint8_t *)src+state.esi), &state.bl);//"    movb (%esi), %bl\n"
		asm_movb (&state, *((uint8_t *)src+state.esi+1), &state.cl);//"    movb 1(%esi), %cl\n"
		asm_movb (&state, *((uint8_t *)src+state.esi+2), &state.dl);//"    movb 2(%esi), %dl\n"
		asm_movl (&state, *(uint32_t *)((uint8_t *)mixrClip8amp1_4+(state.ebx<<1)), &state.eax);//"    movl 1234(,%ebx,2), %eax\nmixrClip8amp1:\n"
		asm_addl (&state, *(uint32_t *)((uint8_t *)mixrClip8amp2_4+(state.ecx<<1)), &state.eax);//"    addl 1234(,%ecx,2), %eax\nmixrClip8amp2:\n"
		asm_addl (&state, *(uint32_t *)((uint8_t *)mixrClip8amp3_4+(state.edx<<1)), &state.eax);//"    addl 1234(,%edx,2), %eax\nmixrClip8amp3:\n"
		*((uint8_t*)dst+state.edi) = state.ah;//"    movb %ah, (%edi)\n"

common8:
		asm_incl (&state, &state.edi);//"    incl %edi\n"
		asm_addl (&state, 4, &state.esi);//"    addl $4, %esi\n"
		asm_cmpl (&state, mixrClipendpX_4, state.edi);//"  cmpl $1234, %edi\nmixrClip8endp1:\n"
		asm_jb (&state, mixrClip8lp);//"  jb mixrClip8lp\n"
		asm_jmp (&state, mixrClip8out);//"  jmp mixrClip8out\n"

mixrClip8low:
		*((uint8_t*)dst+state.edi) = mixrClip8minv_1;//"    movb $12, (%edi)\nmixrClip8minv:\n"
		goto common8;

mixrClip8high:
		*((uint8_t*)dst+state.edi) = mixrClip8maxv_1;//"    movb $12, (%edi)\nmixrClip8maxv:\n"
		goto common8;
	}
}

#endif
