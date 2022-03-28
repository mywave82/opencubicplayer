#include <stddef.h>

void remap_range1_start(void){}


void nonepublic_dwmixa3(void)
{
	__asm__ __volatile__
	(
		".cfi_endproc\n"

		".type mixrFadeChannel_, @function\n"
		"mixrFadeChannel_:\n"
		".cfi_startproc\n"

		"  movl %c0(%%edi), %%ebx\n"      /*  %0 = curvol[0] */
		"  movl %c1(%%edi), %%ecx\n"      /*  %1 = curvol[1] */
		"  shll $8, %%ebx\n"
		"  shll $8, %%ecx\n"
		"  movl %c2(%%edi),%%eax\n"       /*  %2 = ch->samp */
		"  addl %c3(%%edi),%%eax\n"       /*  %3 = ch->pos */
		"  testb %5, %c4(%%edi)\n"        /*  %5 = MIXRQ_PLAY16BIT */
		                                  /*  %4 = ch->status, */
		"  jnz mixrFadeChannel16\n"
		"    movb (%%eax), %%bl\n"
		"  jmp mixrFadeChanneldo\n"
		"mixrFadeChannel16:\n"
		"    movb 1(,%%eax,2),%%bl\n"
		"mixrFadeChanneldo:\n"
		"  movb %%bl, %%cl\n"
		"  movl 1234(,%%ebx,4),%%ebx\n"
		"mixrFadeChannelvoltab1:\n"
		"  movl 1234(,%%ecx,4),%%ecx\n"
		"mixrFadeChannelvoltab2:\n"
		"  addl %%ebx, (%%esi)\n"
		"  addl %%ecx, 4(%%esi)\n"
		"  movl $0, %c0(%%edi)\n"         /* %0 = curvol[0] */
		"  movl $0, %c1(%%edi)\n"         /* %1 = curvol[1] */
		"  ret\n"
		".cfi_endproc\n"
		".size mixrFadeChannel_, .-mixrFadeChannel_\n"

		".type setupfade, @function\n"
		"setupfade:\n" /* CALLED FROM EXTERNAL */
		".cfi_startproc\n"
		"  movl %%eax, (mixrFadeChannelvoltab1-4)\n"
		"  movl %%eax, (mixrFadeChannelvoltab2-4)\n"
		"  ret\n"
		".cfi_endproc\n"
		".size setupfade, .-setupfade\n"

		".cfi_startproc\n"
		:
		: "n" (offsetof(struct channel, curvols[0])), /*  0  */
		  "n" (offsetof(struct channel, curvols[1])), /*  1  */
		  "n" (offsetof(struct channel, samp)),       /*  2  */
		  "n" (offsetof(struct channel, pos)),        /*  3  */
		  "n" (offsetof(struct channel, status)),     /*  4  */
		  "n" (MIXRQ_PLAY16BIT)                       /*  5  */
	);
}

__attribute__((optimize("-fno-omit-frame-pointer"))) /* we use the stack, so we need all access to go via EBP, not ESP */
void mixrFadeChannel(int32_t *fade, struct channel *ch)
{
	int d0, d1;

	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"call mixrFadeChannel_\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S"(d0),
		  "=&D"(d1)
		: "0"(fade),
		  "1"(ch)
#ifdef __PIC__
		: "memory", "eax", "ecx", "edx"
#else
		: "memory", "eax", "ebx", "ecx", "edx"
#endif
	);
}

void nonepublic_dwmixa1(void)
{
	__asm__ __volatile__
	(
		".cfi_endproc\n"
	);

	__asm__ __volatile__
	(
		".type playquiet, @function\n"
		"playquiet:\n"
		".cfi_startproc\n"

		"  ret\n"

		".cfi_endproc\n"
		".size playquiet, .-playquiet\n"
	);

	__asm__ __volatile__
	(
		".type playmono, @function\n"
		"playmono:\n"
		".cfi_startproc\n"

		"playmonolp:\n"
		"    movb (%esi), %bl\n"        // esi = pos   bl=sample-data
		"    addl $1234,%edx\n"         // edx = fpos << 16
		"monostepl:\n"
		"    movl 1234(,%ebx,4), %eax\n" // ebx=sample-data, 1234=mixrFadeChannelvoltab[vol0], eax=result of lookup
		"playmonomonosteplvol1:\n"
		"    adcl $1234, %esi\n"       // 1234 = step real part
		"monosteph:\n"
		"    addl %eax, (%edi)\n"      // edi = destination
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"       // 1234 = vol0add
		"monoramp:\n"
		"  cmpl $1234, %edi\n"         // 1234 = destination eof (based on len)
		"monoendp:\n"
		"  jb playmonolp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playmono, .-playmono\n"

		".type setupmono, @function\n"
		"setupmono:\n" /* CALLED FROM EXTERNAL */
		".cfi_startproc\n"

		"  movl %eax, (playmonomonosteplvol1-4)\n"
		"  ret\n"

		".cfi_endproc\n"
		".size setupmono, .-setupmono\n"
	);

	__asm__ __volatile__
	(
		".type playmono16, @function\n"
		"playmono16:\n"
		".cfi_startproc\n"

		"playmono16lp:\n"
		"    movb 1(%esi,%esi), %bl\n"
		"    addl $1234, %edx\n"
		"mono16stepl:\n"
		"    movl 1234(,%ebx,4),%eax\n"
		"playmono16vol1:\n"
		"    adcl $1234, %esi\n"
		"mono16steph:\n"
		"    addl %eax, (%edi)\n"
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"
		"mono16ramp:\n"
		"    cmpl $1234, %edi\n"
		"mono16endp:\n"
		"  jb playmono16lp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playmono16, .-playmono16\n"

		".type setupmono16, @function\n"
		"setupmono16:\n" /* usual CALLED from EXTERNAL crap*/
		".cfi_startproc\n"

		"  movl %eax, (playmono16vol1-4)\n"
		"  ret\n"

		".cfi_endproc\n"
		".size setupmono16, .-setupmono16\n"
	);

	__asm__ __volatile__
	(
		".type playmonoi, @function\n"
		"playmonoi:\n"
		".cfi_startproc\n"

		"playmonoilp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb (%esi), %al\n"
		"    movb 1234(%eax,%eax), %bl\n"
		"playmonoiint0:\n"
		"    movb 1(%esi), %al\n"
		"    addb 1234(%eax,%eax), %bl\n"
		"playmonoiint1:\n"

		"    addl $1234, %edx\n"
		"monoistepl:\n"
		"    movl 1234(,%ebx,4),%eax\n"
		"playmonoivol1:\n"
		"    adcl $1234, %esi\n"
		"monoisteph:\n"
		"    addl %eax, (%edi)\n"
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"
		"monoiramp:\n"
		"  cmpl $1234, %edi\n"
		"monoiendp:\n"
		"  jb playmonoilp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playmonoi, .-playmonoi\n"

		".type setupmonoi, @function\n"
		"setupmonoi:\n" /* need to comment??? external */
		".cfi_startproc\n"

		"  movl %eax, (playmonoivol1-4)\n"
		"  movl %ebx, (playmonoiint0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playmonoiint1-4)\n"
		"  decl %ebx\n"
		"  ret\n"

		".cfi_endproc\n"
		".size setupmonoi, .-setupmonoi\n"
	);

	__asm__ __volatile__
	(
		".type playmonoi16, @function\n"
		"playmonoi16:\n"
		".cfi_startproc\n"

		"playmonoi16lp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb 1(%esi,%esi), %al\n"
		"    movb 1234(%eax,%eax), %bl\n"
		"playmonoi16int0:\n"
		"    movb 3(%esi,%esi), %al\n"
		"    addb 1234(%eax,%eax), %bl\n"
		"playmonoi16int1:\n"

		"    addl $1234, %edx\n"
		"monoi16stepl:\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playmonoi16vol1:\n"
		"    adcl $1234, %esi\n"
		"monoi16steph:\n"
		"    addl %eax, (%edi)\n"
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"
		"monoi16ramp:\n"
		"  cmpl $1234, %edi\n"
		"monoi16endp:\n"
		"  jb playmonoi16lp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playmonoi16, .-playmonoi16\n"

		".type setupmonoi16, @function\n"
		"setupmonoi16:\n" /* WE ARE NOT SHOCKED ABOUT EXTERNAL STUFF? */
		".cfi_startproc\n"

		"  movl %eax, (playmonoi16vol1-4)\n"
		"  movl %ebx, (playmonoi16int0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playmonoi16int1-4)\n"
		"  decl %ebx\n"
		"  ret\n"

		".cfi_endproc\n"
		".size setupmonoi16, .-setupmonoi16\n"
	);

	__asm__ __volatile__
	(
		".type playstereo, @function\n"
		"playstereo:\n"
		".cfi_startproc\n"

		"playstereolp:\n"
		"    movb (%esi), %bl\n"
		"    addl $1234, %edx\n"
		"stereostepl:\n"
		"    movb (%esi), %cl\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playstereovol1:\n"
		"    adcl $1234, %esi\n"
		"stereosteph:\n"
		"    addl %eax, (%edi)\n"
		"    movl 1234(,%ecx,4), %eax\n"
		"playstereovol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234, %ebx\n"
		"stereoramp0:\n"
		"    addl $1234, %ecx\n"
		"stereoramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereoendp:\n"
		"  jb playstereolp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playstereo, .-playstereo\n"

		".type setupstereo, @function\n"
		"setupstereo:\n" /* TAKE A WILD GUESS */
		".cfi_startproc\n"

		"  movl %eax, (playstereovol1-4)\n"
		"  movl %eax, (playstereovol2-4)\n"
		"  ret\n"

		".cfi_endproc\n"
		".size setupstereo, .-setupstereo\n"
	);

	__asm__ __volatile__
	(
		".type playstereo16, @function\n"
		"playstereo16:\n"
		".cfi_startproc\n"

		"playstereo16lp:\n"
		"    movb 1(%esi,%esi), %bl\n"
		"    addl $1234, %edx\n"
		"stereo16stepl:\n"
		"    movb 1(%esi,%esi), %cl\n"
		"    movl 1234(,%ebx,4),%eax\n"
		"playstereo16vol1:\n"
		"    adcl $1234, %esi\n"
		"stereo16steph:\n"
		"    addl %eax,(%edi)\n"
		"    movl 1234(,%ecx,4), %eax\n"
		"playstereo16vol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234,%ebx\n"
		"stereo16ramp0:\n"
		"    addl $1234,%ecx\n"
		"stereo16ramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereo16endp:\n"
		"  jb playstereo16lp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playstereo16, .-playstereo16\n"

		".type setupstereo16, @function\n"
		"setupstereo16:\n" /* GUESS TWO TIMES? */
		".cfi_startproc\n"

		"  movl %eax, (playstereo16vol1-4)\n"
		"  movl %eax, (playstereo16vol2-4)\n"
		"  ret\n"

		".cfi_endproc\n"
		".size setupstereo16, .-setupstereo16\n"
	);

	__asm__ __volatile__
	(
		".type playstereoi, @function\n"
		"playstereoi:\n"
		".cfi_startproc\n"

		"playstereoilp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb (%esi), %al\n"
		"    movb 1234(%eax,%eax), %bl\n"
		"playstereoiint0:\n"
		"    movb 1(%esi), %al\n"
		"    addb 1234(%eax,%eax), %bl\n"
		"playstereoiint1:\n"

		"    addl $1234, %edx\n"
		"stereoistepl:\n"
		"    movb %bl, %cl\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playstereoivol1:\n"
		"    adcl $1234, %esi\n"
		"stereoisteph:\n"
		"    addl %eax, (%edi)\n"
		"    movl 1234(,%ecx,4),%eax\n"
		"playstereoivol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234, %ebx\n"
		"stereoiramp0:\n"
		"    addl $1234, %ecx\n"
		"stereoiramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereoiendp:\n"
		"  jb playstereoilp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playstereoi, .-playstereoi\n"

		".type setupstereoi, @function\n"
		"setupstereoi:\n" /* THESE ARE STARTING TO BECOME A HABIT NOW*/
		".cfi_startproc\n"

		"  movl %eax, (playstereoivol1-4)\n"
		"  movl %eax, (playstereoivol2-4)\n"
		"  movl %ebx, (playstereoiint0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playstereoiint1-4)\n"
		"  decl %ebx\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playstereoi, .-playstereoi\n"
	);

	__asm__ __volatile__
	(
		".type playstereoi16, @function\n"
		"playstereoi16:\n"
		".cfi_startproc\n"

		"playstereoi16lp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb 1(%esi,%esi), %al\n"
		"    movb 1234(%eax, %eax), %bl\n"
		"playstereoi16int0:\n"
		"    movb 3(%esi, %esi), %al\n"
		"    addb 1234(%eax, %eax), %bl\n"
		"playstereoi16int1:\n"

		"    addl $1234, %edx\n"
		"stereoi16stepl:\n"
		"    movb %bl, %cl\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playstereoi16vol1:\n"
		"    adcl $1234, %esi\n"
		"stereoi16steph:\n"
		"    addl %eax, (%edi)\n"
		"    movl 1234(,%ecx,4), %eax\n"
		"playstereoi16vol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234, %ebx\n"
		"stereoi16ramp0:\n"
		"    addl $1234, %ecx\n"
		"stereoi16ramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereoi16endp:\n"
		"  jb playstereoi16lp\n"
		"  ret\n"

		".cfi_endproc\n"
		".size playstereoi16, .-playstereoi16\n"

		".type setupstereoi16, @function\n"
		"setupstereoi16:" /* THIS IS THE LAST ONE!!!!!!!!!! */
		".cfi_startproc\n"

		"  movl %eax, (playstereoi16vol1-4)\n"
		"  movl %eax, (playstereoi16vol2-4)\n"
		"  movl %ebx, (playstereoi16int0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playstereoi16int1-4)\n"
		"  decl %ebx\n"
		"  ret\n"

		".cfi_endproc\n"
		".size setupstereoi16, .-setupstereoi16\n"
	);

	__asm__ __volatile__
	(
		".cfi_startproc\n"

		".section .data\n"
		"dummydd: .long 0\n"

		"routq:\n"
		".long   playquiet,       dummydd,         dummydd,         dummydd,         dummydd,         dummydd,         0,0\n"
		"routtab:\n"
		".long   playmono,        monostepl-4,     monosteph-4,     monoramp-4,      dummydd,         monoendp-4,      0,0\n"
		".long   playmono16,      mono16stepl-4,   mono16steph-4,   mono16ramp-4,    dummydd,         mono16endp-4,    0,0\n"
		".long   playmonoi,       monoistepl-4,    monoisteph-4,    monoiramp-4,     dummydd,         monoiendp-4,     0,0\n"
		".long   playmonoi16,     monoi16stepl-4,  monoi16steph-4,  monoi16ramp-4,   dummydd,         monoi16endp-4,   0,0\n"
		".long   playstereo,      stereostepl-4,   stereosteph-4,   stereoramp0-4,   stereoramp1-4,   stereoendp-4,    0,0\n"
		".long   playstereo16,    stereo16stepl-4, stereo16steph-4, stereo16ramp0-4, stereo16ramp1-4, stereo16endp-4,  0,0\n"
		".long   playstereoi,     stereoistepl-4,  stereoisteph-4,  stereoiramp0-4,  stereoiramp1-4,  stereoiendp-4,   0,0\n"
		".long   playstereoi16,   stereoi16stepl-4,stereoi16steph-4,stereoi16ramp0-4,stereoi16ramp1-4,stereoi16endp-4, 0,0\n"

		".previous\n"
	);
}

__attribute__((optimize("-fno-omit-frame-pointer"))) /* we use the stack, so we need all access to go via EBP, not ESP */
void mixrPlayChannel(int32_t *buf, int32_t *fadebuf, uint32_t len, struct channel *chan, int stereo)
{
	void *routptr;
	uint32_t filllen,
	         ramping[2];
	int inloop;
	int ramploop;
	int dofade;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl %3, %%edi\n"              /*  %3 = chan */
		"  testb %25, %c12(%%edi)\n"      /* %25 = MIXRQ_PLAYING */
		                                  /* %12 = status */
		"  jz mixrPlayChannelexit\n"

		"  movl $0, %6\n"                 /*  %6 = fillen */
		"  movl $0, %11\n"                /* %11 = dofade */

		"  xorl %%eax, %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelnostereo\n"
		"    addl $4, %%eax\n"
		"mixrPlayChannelnostereo:\n"
		"  testb %27, %c12(%%edi)\n"      /* %27 = MIXRQ_INTERPOLATE */
		                                  /* %12 = ch->status */
		"  jz mixrPlayChannelnointr\n"
		"    addl $2, %%eax\n"
		"mixrPlayChannelnointr:\n"
		"  testb %26, %c12(%%edi)\n"      /* %26 = MIXRQ_PLAY16BIT */
		                                  /* %12 = ch->status */
		"  jz mixrPlayChannelpsetrtn\n"
		"    incl %%eax\n"
		"mixrPlayChannelpsetrtn:\n"
		"  shll $5, %%eax\n"
		"  addl $routtab, %%eax\n"
		"  movl %%eax, %5\n"              /*  %5 = routeptr*/

		"mixrPlayChannelbigloop:\n"
		"  movl %2, %%ecx\n"              /*  %2 = len */
		"  movl %c13(%%edi), %%ebx\n"     /* %13 = ch->step */
		"  movl %c14(%%edi), %%edx\n"     /* %14 = ch->pos */
		"  movw %c15(%%edi), %%si\n"      /* %15 = ch->fpos */
		"  movb $0, %9\n"                 /*  %9 = inloop */
		"  cmpl $0, %%ebx\n"

		"  je mixrPlayChannelplayecx\n"
		"  jg mixrPlayChannelforward\n"
		"    negl %%ebx\n"
		"    movl %%edx, %%eax\n"
		"    testb %28, %c12(%%edi)\n"    /* %28 = MIXRQ_LOOPED */
		                                  /* %12 = ch->status */
		"    jz mixrPlayChannelmaxplaylen\n"
		"    cmpl %c16(%%edi), %%edx\n"   /* %16 = ch->loopstart */
		"    jb mixrPlayChannelmaxplaylen\n"
		"    subl %c16(%%edi), %%eax\n"   /* %16 = ch->loopstart */
		"    movb $1, %9\n"               /*  %9 = inloop */
		"    jmp mixrPlayChannelmaxplaylen\n"
		"mixrPlayChannelforward:\n"
		"    movl %c18(%%edi), %%eax\n"   /* %18 = length */
		"    negw %%si\n"
		"    sbbl %%edx, %%eax\n"
		"    testb %28, %c12(%%edi)\n"    /* %28 = MIXRQ_LOOPED */
		                                  /* %12 = ch->status */
		"    jz mixrPlayChannelmaxplaylen\n"
		"    cmpl %c17(%%edi), %%edx\n"   /* %17 = ch->loopend */
		"    jae mixrPlayChannelmaxplaylen\n"
		"    subl %c18(%%edi), %%eax\n"   /* %18 = ch->length */
		"    addl %c17(%%edi), %%eax\n"   /* %17 = ch->loopend*/
		"    movb $1, %9\n"               /*  %9 = inloop */

		"mixrPlayChannelmaxplaylen:\n"
		"  xorl %%edx, %%edx\n"
		"  shld $16, %%eax, %%edx\n"
		"  shll $16, %%esi\n"
		"  shld $16, %%esi, %%eax\n"
		"  addl %%ebx, %%eax\n"
		"  adcl $0, %%edx\n"
		"  subl $1, %%eax\n"
		"  sbbl $0, %%edx\n"
		"  cmpl %%ebx, %%edx\n"
		"  jae mixrPlayChannelplayecx\n"
		"  divl %%ebx\n"
		"  cmpl %%eax, %%ecx\n"
		"  jb mixrPlayChannelplayecx\n"
		"    movl %%eax, %%ecx\n"
		"    cmpb $0, %9\n"               /*  %9 = inloop */
		"    jnz mixrPlayChannelplayecx\n"
#if MIXRQ_PLAYING != 1
#error This line bellow depends on MIXRQ_PLAYING = 1
#endif
		"      andb $254, %c12(%%edi)\n"  /* 254 = 255-MIXRQ_PLAYING */
		                                  /* %12 = ch->status */
		"      movl $1, %11\n"            /* %11 = dofade */
		"      movl %2, %%eax\n"          /*  %2 = len */
		"      subl %%ecx, %%eax\n"
		"      addl %%eax, %6\n"          /*  %6 = filllen */
		"      movl %%ecx, %2\n"          /*  %2 = len */

		"mixrPlayChannelplayecx:\n"
		"  movb $0, %10\n"                /* %10 = ramploop */
		"  movl $0, %7\n"                 /*  %7 = ramping[0] */
		"  movl $0, %8\n"                 /*  %8 = ramping[1] */

		"  cmpl $0, %%ecx\n"
		"  je mixrPlayChannelnoplay\n"

		"  movl %c21(%%edi), %%edx\n"     /* %21 = ch->dstvols[0] */
		"  subl %c19(%%edi), %%edx\n"     /* %19 = ch->curvols[0] */
		"  je mixrPlayChannelnoramp0\n"
		"  jl mixrPlayChannelramp0down\n"
		"    movl $1, %7\n"               /*  %7 = ramping[0] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp0\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"      jmp mixrPlayChannelnoramp0\n"
		"mixrPlayChannelramp0down:\n"
		"    negl %%edx\n"
		"    movl $-1, %7\n"              /*  %7 = ramping[0] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp0\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"mixrPlayChannelnoramp0:\n"

		"  movl %c22(%%edi), %%edx\n"     /* %22 = ch->dstvols[1] */
		"  subl %c20(%%edi), %%edx\n"     /* %20 = ch->curvols[1] */
		"  je mixrPlayChannelnoramp1\n"
		"  jl mixrPlayChannelramp1down\n"
		"    movl $1, %8\n"               /*  %8 = ramping[1] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp1\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"      jmp mixrPlayChannelnoramp1\n"
		"mixrPlayChannelramp1down:\n"
		"    negl %%edx\n"
		"    movl $-1, %8\n"              /*  %8 = ramping[1] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp1\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"mixrPlayChannelnoramp1:\n"

		"  movl %5, %%edx\n"              /*  %5 = routptr */
		"  cmpl $0, %7\n"                 /*  %7 = ramping[0] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %8\n"                 /*  %8 = ramping[1] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %c19(%%edi)\n"        /* %19 = ch->curvols[0] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %c20(%%edi)\n"        /* %20 = ch->curvols[1] */
		"  jne mixrPlayChannelnotquiet\n"
		"    movl $routq, %%edx\n"

		"mixrPlayChannelnotquiet:\n"
		"  movl 4(%%edx), %%ebx\n"
		"  movl %c13(%%edi), %%eax\n"     /* %13 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 8(%%edx), %%ebx\n"
		"  movl %c13(%%edi), %%eax\n"     /* %13 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 12(%%edx), %%ebx\n"
		"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
		"  shll $8, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 16(%%edx), %%ebx\n"
		"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
		"  shll $8, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 20(%%edx), %%ebx\n"
		"  leal (,%%ecx,4), %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelm1\n"
		"    shll $1, %%eax\n"
		"mixrPlayChannelm1:\n"
		"  addl %0, %%eax\n"              /*  %0 = buf */
		"  movl %%eax, (%%ebx)\n"
		"  pushl %%ecx\n"
		"  movl (%%edx), %%eax\n"

		"  movl %c19(%%edi), %%ebx\n"     /* %19 = ch->curvols[0] */
		"  shll $8, %%ebx\n"
		"  movl %c20(%%edi), %%ecx\n"     /* %20 = ch->curvols[1] */
		"  shll $8, %%ecx\n"
		"  movw %c15(%%edi), %%dx\n"      /* %15 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %c14(%%edi), %%esi\n"     /* %14 = ch->chpos */
		"  addl %c23(%%edi), %%esi\n"     /* %23 = ch->samp */
		"  movl %0, %%edi\n"              /*  %0 = buf */

		"  call *%%eax\n"

		"  popl %%ecx\n"
		"  movl %3, %%edi\n"              /*  %3 = chan */

		"mixrPlayChannelnoplay:\n"
		"  movl %%ecx, %%eax\n"
		"  shll $2, %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelm2\n"
		"    shll $1, %%eax\n"
		"mixrPlayChannelm2:\n"
		"  addl %%eax, %0\n"              /*  %0 = buf */
		"  subl %%ecx, %2\n"              /*  %2 = len */

		"  movl %c13(%%edi), %%eax\n"     /* %13 = ch->step */
		"  imul %%ecx\n"
		"  shld $16, %%eax, %%edx\n"
		"  addw %%ax, %c15(%%edi)\n"      /* %15 = ch->fpos */
		"  adcl %%edx, %c14(%%edi)\n"     /* %14 = ch->pos */

		"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
		"  imul %%ecx, %%eax\n"
		"  addl %%eax, %c19(%%edi)\n"     /* %19 = ch->curvols[0] */
		"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
		"  imul %%ecx, %%eax\n"
		"  addl %%eax, %c20(%%edi)\n"     /* %20 = ch->curvols[1] */

		"  cmpb $0, %10\n"                /* %10 = ramploop */
		"  jnz mixrPlayChannelbigloop\n"

		"  cmpb $0, %9\n"                 /*  %9 = inloop */
		"  jz mixrPlayChannelfill\n"

		"  movl %c14(%%edi), %%eax\n"     /* %14 = ch->pos */
		"  cmpl $0, %c13(%%edi)\n"        /* %13 = ch->step */
		"  jge mixrPlayChannelforward2\n"
		"    cmpl %c16(%%edi), %%eax\n"   /* %16 = ch->loopstart */
		"    jge mixrPlayChannelexit\n"
		"    testb %29, %c12(%%edi)\n"    /* %29 = MIXRQ_PINGPONGLOOP */
		                                  /* %12 = ch->status */
		"    jnz mixrPlayChannelpong\n"
		"      addl %c24(%%edi), %%eax\n" /* %24 = ch->replen */
		"      jmp mixrPlayChannelloopiflen\n"
		"mixrPlayChannelpong:\n"
		"      negl %c13(%%edi)\n"        /* %13 = ch->step */
		"      negw %c15(%%edi)\n"        /* %15 = ch->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %c16(%%edi), %%eax\n" /* %16 = ch->loopstart */
		"      addl %c16(%%edi), %%eax\n" /* %16 = ch->loopstart */
		"      jmp mixrPlayChannelloopiflen\n"
		"mixrPlayChannelforward2:\n"
		"    cmpl %c17(%%edi), %%eax\n"   /* %17 = ch->loopend */
		"    jb mixrPlayChannelexit\n"
		"    testb %29, %c12(%%edi)\n"    /* %29 = MIXRQ_PINGPONGLOOP */
		                                  /* %12 = ch->status */
		"    jnz mixrPlayChannelping\n"
		"      subl %c24(%%edi), %%eax\n" /* %24 = ch->replen */
		"      jmp mixrPlayChannelloopiflen\n"
		"mixrPlayChannelping:\n"
		"      negl %c13(%%edi)\n"        /* %13 = ch->step */
		"      negw %c15(%%edi)\n"        /* %15 = ch->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %c17(%%edi), %%eax\n" /* %17 = ch->loopend */
		"      addl %c17(%%edi), %%eax\n" /* %17 = ch->loopend */

		"mixrPlayChannelloopiflen:\n"
		"  movl %%eax, %c14(%%edi)\n"     /* %14 = ch->pos */
		"  cmpl $0, %2\n"                 /*  %2 = len */
		"  jne mixrPlayChannelbigloop\n"
		"  jmp mixrPlayChannelexit\n"

		"mixrPlayChannelfill:\n"
		"  cmpl $0, %6\n"                 /*  %6 = filllen */
		"  je mixrPlayChannelfadechk\n"
		"  movl %c18(%%edi), %%eax\n"     /* %18 = ch->length */
		"  movl %%eax, %c14(%%edi)\n"     /* %14 = ch->pos */
		"  addl %c23(%%edi), %%eax\n"     /* %23 = ch->samp */
		"  movl %c19(%%edi), %%ebx\n"     /* %19 = ch->curvols[0] */
		"  movl %c20(%%edi), %%ecx\n"     /* %20 = ch->curvols[1] */
		"  shll $8, %%ebx\n"
		"  shll $8, %%ecx\n"
		"  testb %26, %c12(%%edi)\n"      /* %26 = MIXRQ_PLAY16BIT */
		                                  /* %12 = ch->status */
		"  jnz mixrPlayChannelfill16\n"
		"    movb (%%eax), %%bl\n"
		"    jmp mixrPlayChannelfilldo\n"
		"mixrPlayChannelfill16:\n"
		"    movb 1(%%eax, %%eax), %%bl\n"
		"mixrPlayChannelfilldo:\n"
		"  movb %%bl, %%cl\n"
		"  movl 1234(,%%ebx,4), %%ebx\n"
		"mixrPlayChannelvoltab1:\n"
		"  movl 1234(,%%ecx,4), %%ecx\n"
		"mixrPlayChannelvoltab2:\n"
		"  movl %6, %%eax\n"              /*  %6 = filllen */
		"  movl %0, %%edi\n"              /*  %0 = buf */
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  jne mixrPlayChannelfillstereo\n"
		"mixrPlayChannelfillmono:\n"
		"    addl %%ebx,(%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%eax\n"
		"  jnz mixrPlayChannelfillmono\n"
		"  jmp mixrPlayChannelfade\n"
		"mixrPlayChannelfillstereo:\n"
		"    addl %%ebx, (%%edi)\n"
		"    addl %%ecx, 4(%%edi)\n"
		"    addl $8, %%edi\n"
		"  decl %%eax\n"
		"  jnz mixrPlayChannelfillstereo\n"
		"  jmp mixrPlayChannelfade\n"

		"mixrPlayChannelfadechk:\n"
		"  cmpl $0, %11\n"                /* %11 = dofade */
		"  je mixrPlayChannelexit\n"
		"mixrPlayChannelfade:\n"
		"  movl %3, %%edi\n"              /* %3 = chan */
		"  movl %1, %%esi\n"              /* %1 = fadebuf */
		"  call mixrFadeChannel_\n"
		"  jmp mixrPlayChannelexit\n"

		"setupplay:\n"
		"  movl %%eax, (mixrPlayChannelvoltab1-4)\n"
		"  movl %%eax, (mixrPlayChannelvoltab2-4)\n"
		"  ret\n"

		"mixrPlayChannelexit:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "m" (buf),                                  /*   0  */
		  "m" (fadebuf),                              /*   1  */
		  "m" (len),                                  /*   2  */
		  "m" (chan),                                 /*   3  */
		  "m" (stereo),                               /*   4  */
		  "m" (routptr),                              /*   5  */
		  "m" (filllen),                              /*   6  */
		  "m" (ramping[0]),                           /*   7  */
		  "m" (ramping[1]),                           /*   8  */
		  "m" (inloop),                               /*   9  */
		  "m" (ramploop),                             /*  10  */
		  "m" (dofade),                               /*  11  */
		  "n" (offsetof(struct channel, status)),     /*  12  */
		  "n" (offsetof(struct channel, step)),       /*  13  */
		  "n" (offsetof(struct channel, pos)),        /*  14  */
		  "n" (offsetof(struct channel, fpos)),       /*  15  */
		  "n" (offsetof(struct channel, loopstart)),  /*  16  */
		  "n" (offsetof(struct channel, loopend)),    /*  17  */
		  "n" (offsetof(struct channel, length)),     /*  18  */
		  "n" (offsetof(struct channel, curvols[0])), /*  19  */
		  "n" (offsetof(struct channel, curvols[1])), /*  20  */
		  "n" (offsetof(struct channel, dstvols[0])), /*  21  */
		  "n" (offsetof(struct channel, dstvols[1])), /*  22  */
		  "n" (offsetof(struct channel, samp)),       /*  23  */
		  "n" (offsetof(struct channel, replen)),     /*  24  */
		  "n" (MIXRQ_PLAYING),                        /*  25  */
		  "n" (MIXRQ_PLAY16BIT),                      /*  26  */
		  "n" (MIXRQ_INTERPOLATE),                    /*  27  */
		  "n" (MIXRQ_LOOPED),                         /*  28  */
		  "n" (MIXRQ_PINGPONGLOOP)                    /*  29  */
#ifdef __PIC__
		: "memory", "eax", "ecx", "edx", "edi", "esi"
#else
		: "memory", "eax", "ebx", "ecx", "edx", "edi", "esi"
#endif
	);
}

void mixrSetupAddresses(int32_t (*vol)[256], uint8_t (*intr)[256][2])
{
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl  %%ecx, %%ebx\n"
#endif
		"  call setupfade\n"
		"  call setupplay\n"
		"  call setupmono\n"
		"  call setupmono16\n"
		"  call setupmonoi\n"
		"  call setupmonoi16\n"
		"  call setupstereo\n"
		"  call setupstereo16\n"
		"  call setupstereoi\n"
		"  call setupstereoi16\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "a" (vol),
#ifdef __PIC__
		  "c" (intr)
#else
		  "b" (intr)
#endif
		/* no registers should change, and .data/.bss is not touched */
	);
}

void mixrFade(int32_t *buf, int32_t *fade, int len, int stereo)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl (%%esi), %%eax\n"
		"  movl 4(%%esi), %%ebx\n"
		"  cmpl $0, %%edx\n"
		"  jnz mixrFadestereo\n"
		"mixrFadelpm:\n"
		"      movl %%eax, (%%edi)\n"
		"      movl %%eax, %%edx\n"
		"      shll $7, %%eax\n"
		"      subl %%edx, %%eax\n"
		"      sarl $7, %%eax\n"
		"      addl $4, %%edi\n"
		"    decl %%ecx\n"
		"    jnz mixrFadelpm\n"
		"  jmp mixrFadedone\n"
		"mixrFadestereo:\n"
		"mixrFadelps:\n"
		"      movl %%eax, (%%edi)\n"
		"      movl %%ebx, 4(%%edi)\n"
		"      movl %%eax, %%edx\n"
		"      shll $7, %%eax\n"
		"      subl %%edx, %%eax\n"
		"      sarl $7, %%eax\n"
		"      movl %%ebx, %%edx\n"
		"      shll $7, %%ebx\n"
		"      subl %%edx, %%ebx\n"
		"      sarl $7, %%ebx\n"
		"      addl $8, %%edi\n"
		"    decl %%ecx\n"
		"    jnz mixrFadelps\n"
		"mixrFadedone:\n"
		"  movl %%eax, (%%esi)\n"
		"  movl %%ebx, 4(%%esi)\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&D"(d0),
		  "=&c"(d1),
		  "=&d"(d2)
		: "S" (fade),
		  "0" (buf),
		  "1" (len),
		  "2" (stereo)
#ifdef __PIC__
		: "memory", "eax"
#else
		: "memory", "eax", "ebx"
#endif
	);
}

/******************************************************************************/
void nonepublic_dwmixa2(void)
{
	__asm__ __volatile__
	(
		".cfi_endproc\n"

		".type mixrClip8_, @function\n"
		"mixrClip8_:\n"
		".cfi_startproc\n"

		"  movl %ebx, (mixrClip8amp1-4)\n"
		"  addl $512, %ebx\n"
		"  movl %ebx, (mixrClip8amp2-4)\n"
		"  addl $512, %ebx\n"
		"  movl %ebx, (mixrClip8amp3-4)\n"
		"  subl $1024, %ebx\n"
		"  movl %edx, (mixrClip8max-4)\n"
		"  negl %edx\n"
		"  movl %edx, (mixrClip8min-4)\n"

		"  xorl %edx, %edx\n"
		"  movb (mixrClip8min-4), %dl\n"
		"  movl (%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8min-3), %dl\n"
		"  addl 512(%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8min-2), %dl\n"
		"  addl 1024(%ebx, %edx, 2), %eax\n"
		"  movb %ah, (mixrClip8minv-1)\n"
		"  movb (mixrClip8max-4), %dl\n"
		"  movl (%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8max-3), %dl\n"
		"  addl 512(%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8max-2), %dl\n"
		"  addl 1024(%ebx, %edx, 2), %eax\n"
		"  movb %ah, (mixrClip8maxv-1)\n"
		"  leal (%ecx, %edi), %ecx\n"
		"  movl %ecx, (mixrClip8endp1-4)\n"
		"  movl %ecx, (mixrClip8endp2-4)\n"
		"  movl %ecx, (mixrClip8endp3-4)\n"
		"  xorl %ebx, %ebx\n"
		"  xorl %ecx, %ecx\n"
		"  xorl %edx, %edx\n"

		"mixrClip8lp:\n"
		"  cmpl $1234, (%esi)\n"
		"    mixrClip8min:\n"
		"  jl mixrClip8low\n"
		"  cmpl $1234, (%esi)\n"
		"    mixrClip8max:\n"
		"  jg mixrClip8high\n"

		"    movb (%esi), %bl\n"
		"    movb 1(%esi), %cl\n"
		"    movb 2(%esi), %dl\n"
		"    movl 1234(,%ebx,2), %eax\n"
		"      mixrClip8amp1:\n"
		"    addl 1234(,%ecx,2), %eax\n"
		"      mixrClip8amp2:\n"
		"    addl 1234(,%edx,2), %eax\n"
		"      mixrClip8amp3:\n"
		"    movb %ah, (%edi)\n"
		"    incl %edi\n"
		"    addl $4, %esi\n"
		"  cmpl $1234, %edi\n"
		"mixrClip8endp1:\n"
		"  jb mixrClip8lp\n"
		"mixrClip8done:\n"
		"  jmp mixrClip8out\n"

		"mixrClip8low:\n"
		"    movb $12, (%edi)\n"
		"      mixrClip8minv:\n"
		"    incl %edi\n"
		"    addl $4, %esi\n"
		"    cmpl $1234, %edi\n"
		"mixrClip8endp2:\n"
		"  jb mixrClip8lp\n"
		"  jmp mixrClip8done\n"

		"mixrClip8high:\n"
		"    movb $12, (%edi)\n"
		"      mixrClip8maxv:\n"
		"    incl %edi\n"
		"    addl $4, %esi\n"
		"    cmpl $1234, %edi\n"
		"mixrClip8endp3:\n"
		"  jb mixrClip8lp\n"
		"  jmp mixrClip8done\n"

		".cfi_endproc\n"
		".size mixrClip8_, .-mixrClip8_\n"

		".cfi_startproc\n"
	);
}

void mixrClip(void *dst, int32_t *src, int len, void *tab, int32_t max, int b16)
{
	int d0, d1, d2, d3, d4, d5;
#ifdef __PIC__
	d2=(int)tab;
#endif
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %10, %%ebx\n"
#endif
		"  cmpl $0, %%eax\n"
		"  je mixrClip8_\n"

		"  movl %%ebx, (mixrClipamp1-4)\n"
		"  addl $512, %%ebx\n"
		"  movl %%ebx, (mixrClipamp2-4)\n"
		"  addl $512, %%ebx\n"
		"  movl %%ebx, (mixrClipamp3-4)\n"
		"  subl $1024, %%ebx\n"
		"  movl %%edx, (mixrClipmax-4)\n"
		"  negl %%edx\n"
		"  movl %%edx, (mixrClipmin-4)\n"

		"  xorl %%edx, %%edx\n"
		"  movb (mixrClipmin-4), %%dl\n"
		"  movl (%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmin-3), %%dl\n"
		"  addl 512(%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmin-2), %%dl\n"
		"  addl 1024(%%ebx, %%edx, 2), %%eax\n"
		"  movw %%ax, (mixrClipminv-2)\n"
		"  movb (mixrClipmax-4), %%dl\n"
		"  movl (%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmax-3), %%dl\n"
		"  addl 512(%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmax-2), %%dl\n"
		"  addl 1024(%%ebx, %%edx, 2), %%eax\n"
		"  movw %%ax, (mixrClipmaxv-2)\n"
		"  leal (%%edi, %%ecx, 2), %%ecx\n"
		"  movl %%ecx, (mixrClipendp1-4)\n"
		"  movl %%ecx, (mixrClipendp2-4)\n"
		"  movl %%ecx, (mixrClipendp3-4)\n"
		"  xorl %%ebx, %%ebx\n"
		"  xorl %%ecx, %%ecx\n"
		"  xorl %%edx, %%edx\n"

		"mixrCliplp:\n"
		"    cmpl $1234, (%%esi)\n"
		"      mixrClipmin:\n"
		"    jl mixrCliplow\n"
		"    cmpl $1234, (%%esi)\n"
		"      mixrClipmax:\n"
		"    jg mixrCliphigh\n"

		"    movb (%%esi), %%bl\n"
		"    movb 1(%%esi), %%cl\n"
		"    movb 2(%%esi), %%dl\n"
		"    movl 1234(,%%ebx,2), %%eax\n"
		"      mixrClipamp1:\n"
		"    addl 1234(,%%ecx,2), %%eax\n"
		"      mixrClipamp2:\n"
		"    addl 1234(,%%edx,2), %%eax\n"
		"      mixrClipamp3:\n"
		"    movw %%ax, (%%edi)\n"
		"    addl $2, %%edi\n"
		"    addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"    mixrClipendp1:\n"
		"  jb mixrCliplp\n"
		"  jmp mixrClipdone\n"

		"mixrCliplow:\n"
		"    movw $1234, (%%edi)\n"
		"      mixrClipminv:\n"
		"    addl $2, %%edi\n"
		"    addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"    mixrClipendp2:\n"
		"  jb mixrCliplp\n"
		"  jmp mixrClipdone\n"
		"mixrCliphigh:\n"
		"    movw $1234, (%%edi)\n"
		"      mixrClipmaxv:\n"
		"    addl $2, %%edi\n"
		"    addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"    mixrClipendp3:\n"
		"  jb mixrCliplp\n"
		/* jmp mixrClipdone\n" */
		"mixrClipdone:"
		"mixrClip8out:"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
		  "=&c" (d3),
		  "=&d" (d4),
#ifdef __PIC__
		  "=&a" (d5)
#else
		  "=&a" (d5),
		  "=&b" (d2)
#endif
		: "0" (src),
		  "1" (dst),
		  "2" (len),
		  "3" (max),
		  "4" (b16),
#ifdef __PIC__
		  "m" (tab)
#else
		  "5" (tab)
#endif
		: "memory"
	);
}

void remap_range1_stop(void){}

