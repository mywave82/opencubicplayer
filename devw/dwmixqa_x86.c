#ifndef NULL
#define NULL (void *)0
#endif

void remap_range2_start(void){}

void noexternal_dwmixq(void)
{
	__asm__ __volatile__
	(
		"playquiet:\n"
		"  ret\n"
	);

/* esi = current source
 * edi = current destination
 *
 * esi:ebx = 64 bit source index
 * ebp:edx = 64 bit pitch
 * ecx = number of target samples
 * ----------------------------------
 * ecx = target buffer end marker
 * ebx,eax = temp buffers
 */

	__asm__ __volatile__
	(
		"playmono:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  xorb %bl, %bl\n"
		"playmono_lp:\n"
		"    movb (%esi), %bh\n"
		"    addl $2, %edi\n"
		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    movw %bx, -2(%edi)\n"
		"  cmpl %ecx, %edi\n"
		"  jb playmono_lp\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmono16:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  xorb %bl, %bl\n"
		"playmono16_lp:\n"
		"  movw (%esi, %esi), %bx\n"
		"  addl $2, %edi\n"
		"  addl %edx, %ebx\n"
		"  adcl %ebp, %esi\n"
		"  movw %bx, -2(%edi)\n"
		"  cmpl %ecx, %edi\n"
		"  jb playmono16_lp\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi_endp-4)\n"
		"  xorl %eax, %eax\n"
		"  movl %ebx, %ecx\n"

		"playmonoi_lp:\n"
		"  shrl $19, %ecx\n"
		"  addl $2, %edi\n"
		"  movl %ecx, %eax\n"
		"  movb 0(%esi), %cl\n"
		"  movb 1(%esi), %al\n"
		"  addl %edx, %ebx\n"
		"  movw %es:1234(,%ecx,4), %bx\n"
		"    playmonoi_intr1:\n"
		"  adcl %ebp, %esi\n"
		"  addw %es:1234(, %eax, 4), %bx\n"
		"    playmonoi_intr2:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi_endp:\n"
		"  jb playmonoi_lp\n"
		"  ret\n"

		"setupmonoi:\n"
		"  movl %ebx, (playmonoi_intr1-4)\n"
		"  addl $2, %ebx\n"
		"  movl %ebx, (playmonoi_intr2-4)\n"
		"  subl $2, %ebx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi16:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi16_endp-4)\n"
		"  movl %eax, %eax\n" /* pentium pipeline */
		"  movl %ebx, %ecx\n"

		"playmonoi16_lp:\n"
		"    shrl $19, %ecx\n"
		"    addl $2, %edi\n"
		"    movl %ecx, %eax\n"
		"    movb 1(%esi,%esi), %cl\n"
		"    movb 3(%esi,%esi), %al\n"
		"    movw %es:1234(,%ecx,4), %bx\n"
		"      playmonoi16_intr1:\n"
		"    movb (%esi, %esi), %cl\n"
		"    addw 1234(,%eax,4), %bx\n"
		"      playmonoi16_intr2:\n"
		"    movb 2(%esi, %esi), %al\n"
		"    addw %es:1234(,%ecx,4), %bx\n"
		"      playmonoi16_intr3:\n"
		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    addw 1234(,%eax,4), %bx\n"
		"      playmonoi16_intr4:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi16_endp:\n"
		"  jb playmonoi16_lp\n"
		"  ret\n"

		"setupmonoi16:\n"
		"  movl %ebx, (playmonoi16_intr1-4)\n"
		"  addl $2, %ebx\n"
		"  movl %ebx, (playmonoi16_intr2-4)\n"
		"  addl $32768, %ebx\n" /* 4*32*256 */
		"  movl %ebx, (playmonoi16_intr4-4)\n"
		"  subl $2, %ebx\n"
		"  movl %ebx, (playmonoi16_intr3-4)\n"
		"  subl $32768, %ebx\n" /* 4*32*256 - 2 */
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi2:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi2_endp-4)\n"
		"  movl %eax, %eax\n" /* pipeline */
		"  movl %ebx, %ecx\n"

		"playmonoi2_lp:\n"
		"    shrl $20, %ecx\n"
		"    addl $2, %edi\n"
		"    movl %ecx, %eax\n"
		"    movb 0(%esi), %cl\n"
		"    movb 1(%esi), %al\n"

		"    movw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi2_intr1:\n"
		"    movb 2(%esi), %cl\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi2_intr2:\n"

		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    addw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi2_intr3:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi2_endp:\n"
		"  jb playmonoi2_lp\n"

		"  ret\n"

		"setupmonoi2:\n"
		"  movl %ecx, (playmonoi2_intr1-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi2_intr2-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi2_intr3-4)\n"
		"  subl $4, %ecx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi216:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi216_endp-4)\n"
		"  movl %ebx, %ecx\n"
		"  movl %eax, %eax\n" /* pipeline*/

		"playmonoi216_lp:\n"
		"    shrl $20, %ecx\n"
		"    addl $2, %edi\n"
		"    movl %ecx, %eax\n"

		"    movb 1(%esi, %esi), %cl\n"
		"    movb 3(%esi, %esi), %al\n"

		"    movw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi216_intr1:\n"

		"    movb 5(%esi, %esi), %cl\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi216_intr2:\n"

		"    movb 0(%esi,%esi), %al\n"
		"    addw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi216_intr3:\n"

		"    movb 2(%esi,%esi), %cl\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi216_intr4:\n"

		"    movb 4(%esi, %esi), %al\n"
		"    addw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi216_intr5:\n"

		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi216_intr6:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi216_endp:\n"
		"  jb playmonoi216_lp\n"
		"  ret\n"

		"setupmonoi216:\n"
		"  movl %ecx, (playmonoi216_intr1-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr2-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr3-4)\n"
		"  addl $32768, %ecx\n" /* 8*16*256 */
		"  movl %ecx, (playmonoi216_intr6-4)\n"
		"  subl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr5-4)\n"
		"  subl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr4-4)\n"
		"  subl $32768, %ecx\n" /* 8*16*256 */
		"  ret\n"
	);
}


#ifndef __PIC__
static void *playrout;
#endif

void mixqPlayChannel(int16_t *buf, uint32_t len, struct channel *ch, int quiet)
{
	int inloop;
	uint32_t filllen;
#ifdef __PIC__
	void *playrout=NULL;
#endif
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl $0, %5\n"                 /*  %5 = fillen */

		"  movl %2, %%edi\n"              /*  %2 = ch */
		"  cmpb $0, %3\n"                 /*  %3 = quiet */
		"  jne mixqPlayChannel_pquiet\n"

		"  testb %16, %6(%%edi)\n"        /* %16 = MIXQ_INTERPOLATE
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_intr\n"
		"    movl $playmono, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmono16, %%eax\n"
		"  jmp mixqPlayChannel_intrfini\n"
		"mixqPlayChannel_intr:\n"

		"  testb %18, %6(%%edi)\n"        /* %18 = MIXQ_INTERPOLATEMAX
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_intrmax\n"
		"    movl $playmonoi, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmonoi16, %%eax\n"
		"  jmp mixqPlayChannel_intrfini\n"
		"mixqPlayChannel_intrmax:\n"

		"    movl $playmonoi2, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmonoi216, %%eax\n"
		" mixqPlayChannel_intrfini:\n"

		"  movl %%eax, %7\n"              /*  %7 = playrout */
		"  jmp mixqPlayChannel_bigloop\n"

		"mixqPlayChannel_pquiet:\n"

		"  movl $playquiet, %7\n"         /*  %7 = playrout */

		"mixqPlayChannel_bigloop:\n"

		"  movl %1, %%ecx\n"              /*  %1 = len */
		"  movl %8(%%edi), %%ebx\n"       /*  %8 = ch->step */
		"  movl %9(%%edi), %%edx\n"       /*  %9 = ch->pos */
		"  movw %10(%%edi), %%si\n"       /* %10 = ch->fpos */
		"  movb $0, %4\n"                 /*  %4 = inloop */
		"  cmpl $0, %%ebx\n"
		"  je mixqPlayChannel_playecx\n"
		"  jg mixqPlayChannel_forward\n"
		"    negl %%ebx\n"
		"    movl %%edx, %%eax\n"
		"    testb %19, %6(%%edi)\n"      /* %19 = MIXQ_LOOPED
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_maxplaylen\n"
		"    cmpl %11(%%edi), %%edx\n"    /* %11 = ch->loopstart */
		"    jb mixqPlayChannel_maxplaylen\n"
		"    subl %11(%%edi), %%eax\n"    /* %11 = ch->loopstart */
		"    movb $1, %4\n"               /*  %4 = inloop */
		"    jmp mixqPlayChannel_maxplaylen\n"
		"mixqPlayChannel_forward:\n"

		"    movl %14(%%edi), %%eax\n"    /* %14 = ch->length */
		"    negw %%si\n"
		"    sbbl %%edx, %%eax\n"
		"    testb %19, %6(%%edi)\n"      /* %19 = MIXQ_LOOPED
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_maxplaylen\n"
		"    cmpl %12(%%edi), %%edx\n"    /* %12 = ch->loopend */
		"    jae mixqPlayChannel_maxplaylen\n"
		"    subl %14(%%edi), %%eax\n"    /* %14 = ch->length */
		"    addl %12(%%edi), %%eax\n"    /* %12 = ch->loopend */
		"    movb $1, %4\n"               /*  %4 = inloop */


		"mixqPlayChannel_maxplaylen:\n"

		"  xorl %%edx, %%edx\n"
		"  shld $16, %%eax, %%edx\n"
		"  shll $16, %%esi\n"
		"  shld $16, %%esi, %%eax\n"
		"  addl %%ebx, %%eax\n"
		"  adcl $0, %%edx\n"
		"  subl $1, %%eax\n"
		"  sbbl $0, %%edx\n"
		"  cmpl %%ebx, %%edx\n"
		"  jae mixqPlayChannel_playecx\n"
		"  divl %%ebx\n"
		"  cmpl %%eax, %%ecx\n"
		"  jb mixqPlayChannel_playecx\n"
		"    movl %%eax, %%ecx\n"
		"    cmpb $0, %4\n"               /*  %4 = inloop */
		"    jnz mixqPlayChannel_playecx\n"
		"      andb %20, %6(%%edi)\n"     /* %20 = MIXQ_PLAYING^255
		                                   *  %6 = ch->status */
		"      movl %1, %%eax\n"          /*  %1 = len */
		"      subl %%ecx, %%eax\n"
		"      addl %%eax, %5\n"          /*  %5 = filllen */
		"      movl %%ecx, %1\n"          /*  %1 = len */

		"mixqPlayChannel_playecx:\n"

		"  pushl %%ebp\n"
		"  pushl %%edi\n"
		"  pushl %%ecx\n"

#ifdef __PIC__
		/* We are going to kill ebp, so this is needed, since playrout is now a local variable */
		"  movl  %7, %%eax\n " /* step 1 */
		"  pushl %%eax\n"      /* step 2 */
#endif
		"  movw %10(%%edi), %%bx\n"       /* %10 = ch->fpos */
		"  shll $16, %%ebx\n"
		"  movl %0, %%eax\n"              /*  %0 = buf */

		"  movl %8(%%edi), %%edx\n"       /*  %8 = ch->step */
		"  shll $16, %%edx\n"

		"  movl %9(%%edi), %%esi\n"       /*  %9 = ch->pos */
		"  movl %8(%%edi), %%ebp\n"       /*  %8 = ch->step */
		"  sarl $16, %%ebp\n"
		"  addl %15(%%edi), %%esi\n"      /* %15 = ch->samp */
		"  movl %%eax, %%edi\n"

#ifdef __PIC__
		"  popl %%eax\n"  /* step 3 */
		"  call *%%eax\n" /* step 4 */
#else
		"  call *%7\n"                    /*  %7 = playrout */
#endif
		"  popl %%ecx\n"
		"  popl %%edi\n"
		"  popl %%ebp\n"

		"  addl %%ecx, %0\n"              /*  %0 = buf */
		"  addl %%ecx, %0\n"              /*  %0 = buf */
		"  subl %%ecx, %1\n"              /*  %1 = len */

		"  movl %8(%%edi), %%eax\n"       /*  %8 = ch->step */
		"  imul %%ecx\n"
		"  shld $16, %%eax, %%edx\n"
		"  addw %%ax, %10(%%edi)\n"       /* %10 = ch->fpos */
		"  adcl %%edx, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  movl %9(%%edi), %%eax\n"       /*  %9 = ch->pos */

		"  cmpb $0, %4\n"                 /*  %4 = inloop */
		"  jz mixqPlayChannel_fill\n"

		"  cmpl $0, %8(%%edi)\n"          /*  %8 = ch->step */
		"  jge mixqPlayChannel_forward2\n"
		"    cmpl %11(%%edi), %%eax\n"    /* %11 = ch->loopstart */
		"    jge mixqPlayChannel_exit\n"

		"    testb %21, %6(%%edi)\n"      /* %21 = MIXQ_PINGPONGLOOP
		                                   *  %6 = ch->status */
		"    jnz mixqPlayChannel_pong\n"
		"      addl %13(%%edi), %%eax\n"  /* %13 = ch->replen */
		"      jmp mixqPlayChannel_loopiflen\n"
		"    mixqPlayChannel_pong:\n"

		"      negl %8(%%edi)\n"          /*  %8 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %11(%%edi), %%eax\n"  /* %11 = ch->loopstart */
		"      addl %11(%%edi), %%eax\n"  /* %11 = ch->loopstart */
		"      jmp mixqPlayChannel_loopiflen\n"
		"mixqPlayChannel_forward2:\n"

		"    cmpl %12(%%edi), %%eax\n"    /* %12 = ch->loopend */

		"    jb mixqPlayChannel_exit\n"

		"    testb %21, %6(%%edi)\n"      /* %21 = MIXQ_PINGPONGLOOP
		                                   *  %6 = ch->status */
		"    jnz mixqPlayChannel_ping\n"

		"      subl %13(%%edi), %%eax\n"  /* %13 = ch->replen */

		"      jmp mixqPlayChannel_loopiflen\n"

		"    mixqPlayChannel_ping:\n"

		"      negl %8(%%edi)\n"          /*  %8 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->fstep */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %12(%%edi), %%eax\n"  /* %12 = ch->loopend */
		"      addl %12(%%edi), %%eax\n"  /* %12 = ch->loopend */

		"mixqPlayChannel_loopiflen:\n"

		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  cmpl $0, %1\n"                 /*  %1 = len */
		"  jne mixqPlayChannel_bigloop\n"

		"mixqPlayChannel_fill:\n"

		"  cmpl $0, %5\n"                 /*  %5 = filllen */
		"  je mixqPlayChannel_exit\n"
		"  movl %14(%%edi), %%eax\n"      /* %14 = ch->length */
		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  addl %15(%%edi), %%eax\n"      /* %15 = ch->samp */
		"  testb %17, %6(%%edi)\n"        /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_fill16\n"
		"    movb (%%eax), %%ah\n"
		"    movb $0, %%al\n"
		"    jmp mixqPlayChannel_filldo\n"
		"mixqPlayChannel_fill16:\n"
		"    movw (%%eax, %%eax), %%ax\n"
		"mixqPlayChannel_filldo:\n"
		"  movl %5, %%ecx\n"              /*  %5 = filllen */
		"  movl %0, %%edi\n"              /*  %0 = buf */
		"  rep stosw\n"
		"mixqPlayChannel_exit:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "m" (buf),                                 /*   0  */
		  "m" (len),                                 /*   1  */
		  "m" (ch),                                  /*   2  */
		  "m" (quiet),                               /*   3  */
		  "m" (inloop),                              /*   4  */
		  "m" (filllen),                             /*   5  */
		  "m" (((struct channel *)NULL)->status),    /*   6  */
		  "m" (playrout),                            /*   7  */
		  "m" (((struct channel *)NULL)->step),      /*   8  */
		  "m" (((struct channel *)NULL)->pos),       /*   9  */
		  "m" (((struct channel *)NULL)->fpos),      /*  10  */
		  "m" (((struct channel *)NULL)->loopstart), /*  11  */
		  "m" (((struct channel *)NULL)->loopend),   /*  12  */
		  "m" (((struct channel *)NULL)->replen),    /*  13  */
		  "m" (((struct channel *)NULL)->length),    /*  14  */
		  "m" (((struct channel *)NULL)->samp),      /*  15  */
		  "n" (MIXQ_INTERPOLATE),                    /*  16  */
		  "n" (MIXQ_PLAY16BIT),                      /*  17  */
		  "n" (MIXQ_INTERPOLATEMAX),                 /*  18  */
		  "n" (MIXQ_LOOPED),                         /*  19  */
		  "n" (255-MIXQ_PLAYING),                    /*  20  */
		  "n" (MIXQ_PINGPONGLOOP)                    /*  21  */
		:
#ifndef __PIC__
		"ebx",
#endif
		"memory", "eax", "ecx", "edx", "edi", "esi"
	);
}

void mixqAmplifyChannel(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	int d0, d1, d2, d3;
	__asm__ __volatile__
	(

#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%eax, %%ebx\n"
#endif
		"  shll $9, %%ebx\n"
		"  movb 1(%%esi), %%bl\n"

		"mixqAmplifyChannel_ploop:\n"
		"    movl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannel_voltab1:\n"
		"    movb (%%esi), %%bl\n"
		"    addl $2, %%esi\n"
		"    addl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannel_voltab2:\n"
		"    movb 1(%%esi), %%bl\n"
		"    movswl  %%ax, %%eax\n"
		"    addl %%eax, (%%edi)\n"
		"    addl %%edx, %%edi\n"
		"  decl %%ecx\n"
		"  jnz mixqAmplifyChannel_ploop\n"
		"  jmp mixqAmplifyChannel_done\n"

		"setupampchan:\n"
		"  movl %%eax, (mixqAmplifyChannel_voltab1-4)\n"
		"  addl $512, %%eax\n"
		"  movl %%eax, (mixqAmplifyChannel_voltab2-4)\n"
		"  subl $512, %%eax\n"
		"  ret\n"
		"mixqAmplifyChannel_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
#ifdef __PIC__
		  "=&a" (d2),
#else
		  "=&b" (d2),
#endif
		  "=&c" (d3)
		: "0" (src),
		  "1" (buf),
		  "2" (vol),
		  "3" (len),
		  "d" (step)
		: "memory"
#ifndef __PIC__
		, "eax"
#endif
	);
}

void mixqAmplifyChannelUp(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	int d0, d1, d2, d3;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%eax, %%ebx\n"
#endif
		"  shll $9, %%ebx\n"
		"  movb 1(%%esi), %%bl\n"

		"mixqAmplifyChannelUp_ploop:\n"
		"    movl 1234(%%ebx,%%ebx), %%eax\n"
		"      mixqAmplifyChannelUp_voltab1:\n"
		"    movb (%%esi), %%bl\n"
		"    addl $2, %%esi\n"
		"    addl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannelUp_voltab2:\n"
		"    addl $512, %%ebx\n"
		"    movswl %%ax, %%eax\n"
		"    movb 1(%%esi), %%bl\n"
		"    addl %%eax, (%%edi)\n"
		"    addl %%edx, %%edi\n"
		"  decl %%ecx\n"
		"  jnz mixqAmplifyChannelUp_ploop\n"
		"  jmp mixqAmplifyChannelUp_done\n"

		"setupampchanup:\n"
		"  movl %%eax, (mixqAmplifyChannelUp_voltab1-4)\n"
		"  addl $512, %%eax\n"
		"  movl %%eax, (mixqAmplifyChannelUp_voltab2-4)\n"
		"  subl $512, %%eax\n"
		"  ret\n"

		"mixqAmplifyChannelUp_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
#ifdef __PIC__
		  "=&a" (d2),
#else
		  "=&b" (d2),
#endif
		  "=&c" (d3)
		: "0" (src),
		  "1" (buf),
		  "2" (vol),
		  "3" (len),
		  "d" (step)
		: "memory"
#ifndef __PIC__
		, "eax"
#endif
	);
}

void mixqAmplifyChannelDown(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	int d0, d1, d2, d3;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%eax, %%ebx\n"
#endif
		"  shll $9, %%ebx\n"
		"  movb 1(%%esi), %%bl\n"

		"mixqAmplifyChannelDown_ploop:\n"
		"    movl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannelDown_voltab1:\n"
		"    movb (%%esi), %%bl\n"
		"    addl $2, %%esi\n"
		"    addl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannelDown_voltab2:\n"
		"    subl $512, %%ebx\n"
		"    movswl %%ax, %%eax\n"
		"    movb 1(%%esi), %%bl\n"
		"    addl %%eax, (%%edi)\n"
		"    addl %%edx, %%edi\n"
		"  decl %%ecx\n"
		"  jnz mixqAmplifyChannelDown_ploop\n"
		"  jmp mixqAmplifyChannelDown_done\n"

		"setupampchandown:\n"
		"  movl %%eax, (mixqAmplifyChannelDown_voltab1-4)\n"
		"  addl $512, %%eax\n"
		"  movl %%eax, (mixqAmplifyChannelDown_voltab2-4)\n"
		"  subl $512, %%eax\n"
		"  ret\n"

		"mixqAmplifyChannelDown_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
#ifdef __PIC__
		  "=&a" (d2),
#else
		  "=&b" (d2),
#endif
		  "=&c" (d3)
		: "0" (src),
		  "1" (buf),
		  "2" (vol),
		  "3" (len),
		  "d" (step)
		: "memory"
#ifndef __PIC__
		, "eax"
#endif

	);
}

void mixqSetupAddresses(int16_t (*voltab)[2][256], int16_t (*intrtab1)[32][256][2], int16_t (*intrtab2)[16][256][4])
{
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl  %%edx, %%ebx\n"
#endif
		"  call setupampchan\n"
		"  call setupampchanup\n"
		"  call setupampchandown\n"
		"  call setupmonoi\n"
		"  call setupmonoi16\n"
		"  call setupmonoi2\n"
		"  call setupmonoi216\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "a" (voltab),
#ifdef __PIC__
		  "d" (intrtab1),
#else
		  "b" (intrtab1),
#endif
		  "c" (intrtab2)
	);
}

void remap_range2_stop(void){}
