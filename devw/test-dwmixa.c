/* OpenCP Module Player
 * copyright (c) '04-'10 Stian Skjelstad <stian@nixia.no>
 *
 * Unit-test for "dwmixa.c"
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
#include "types.h"
#include <stdio.h>
#include "dwmix.h"
#include "dwmixa.h"
#include <string.h>
#include <stdlib.h>

#if defined(__PIC__) && defined(I386_ASM)
#warning I386_ASM is disabled in non FPU mixers when compiled PIC
#undef I386_ASM
#endif

#ifdef I386_ASM
#include <unistd.h>
#include <sys/mman.h>
#include "stuff/pagesize.inc.c"
#endif


static int16_t (*amptab)[256]; /* signedness is not fixed here */
static int signedout;
static uint32_t clipmax;

static int32_t (*voltabsr)[256];

static uint8_t (*interpoltabr)[256][2];

static void calcamptab(int32_t amp)
	/* Used by SET
	 *         OpenPlayer
	 */
{
	int i;

	amp=3*amp/16;

	for (i=0; i<256; i++)
	{
		amptab[0][i]=(amp*i)>>12;
		amptab[1][i]=(amp*i)>>4;
		amptab[2][i]=(amp*(signed char)i)<<4;
	}

	if(amp)
		clipmax=0x07FFF000/amp;
	else
		clipmax=0x07FFF000;

	if (!signedout)
		for (i=0; i<256; i++)
			amptab[0][i]^=0x8000;
}

static void calcvoltabsr(void)
{
	int i,j;
	for (i=0; i<=512; i++)
		for (j=0; j<256; j++)
			voltabsr[i][j]=(i-256)*(signed char)j;
}

static void calcinterpoltabr(void)
{
	int i,j;
	for (i=0; i<16; i++)
		for (j=0; j<256; j++)
		{
			interpoltabr[i][j][1]=(i*(signed char)j)>>4;
			interpoltabr[i][j][0]=(signed char)j-interpoltabr[i][j][1];
		}
}

static uint8_t  *test_mixrClip_dst8;
static uint16_t *test_mixrClip_dst16;
static int32_t  *test_mixrClip_src;

static const int32_t test_mixrClip_fill_src[32] =
{
	0xfffaf970, 0xffffce5d, 0xffffd792, 0xfffff8cb,
	0xfffffb87, 0xfffecd1d, 0xfffffc89, 0xffffcc8b,
	0xfffffdba, 0xffffd018, 0x00000004, 0xffffdb9b,
	0x0000036d, 0xffffe799, 0x000007f1, 0xffffeddf,
	0x00000d93, 0xffffee4b, 0x000013be, 0xffffe9cc,
	0x0000189e, 0xffffe12e, 0x00001c0f, 0xffffd594,
	0x00001e15, 0xffffd063, 0x00001ed1, 0xffffe258,
	0x00001e50, 0x000106e0, 0x00001d60, 0x000018e3
};

static const uint8_t test_mixrClip_fill_dst8[34] =
{
	0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55
};

static const uint16_t test_mixrClip_fill_dst16[34] =
{
	0x1234,
	0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234,
	0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234,
	0x1324, 0x1234, 0x1234, 0x1324, 0x1234, 0x1234, 0x1234, 0x1234,
	0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234,
	0x1234
};

static const uint16_t test_mixrClip_dst16_test1[34] =
{
	0x1234,
	0x0000, 0x0000, 0x06b7, 0x6a60, 0x7294, 0x0000, 0x759a, 0x0000,
	0x792d, 0x0000, 0x800b, 0x12d2, 0x8a45, 0x36cb, 0x97d1, 0x499d,
	0xa8b7, 0x4ae1, 0xbb37, 0x3d64, 0xc9d7, 0x238a, 0xd42a, 0x00bd,
	0xda3c, 0x0000, 0xdc70, 0x2708, 0xdaed, 0xfffd, 0xd81d, 0xcaa6,
	0x1234
};

static const uint8_t test_mixrClip_dst8_test2[34] =
{
	0x55,
	0x00, 0x00, 0x06, 0x6a, 0x72, 0x00, 0x75, 0x00,
	0x79, 0x00, 0x80, 0x12, 0x8a, 0x36, 0x97, 0x49,
	0xa8, 0x4a, 0xbb, 0x3d, 0xc9, 0x23, 0xd4, 0x00,
	0xda, 0x00, 0xdc, 0x27, 0xda, 0xff, 0xd8, 0xca,
	0x55
};

static const uint16_t test_mixrClip_dst16_test3[34] =
{
	0x1234,
	0x8000, 0x8000, 0x86b7, 0xea60, 0xf294, 0x8000, 0xf59a, 0x8000,
	0xf92d, 0x8000, 0x000b, 0x92d2, 0x0a45, 0xb6cb, 0x17d1, 0xc99d,
	0x28b7, 0xcae1, 0x3b37, 0xbd64, 0x49d7, 0xa38a, 0x542a, 0x80bd,
	0x5a3c, 0x8000, 0x5c70, 0xa708, 0x5aed, 0x7ffd, 0x581d, 0x4aa6,
	0x1234
};

static const uint8_t test_mixrClip_dst8_test4[34] =
{
	0x55,
	0x80, 0x80, 0x86, 0xea, 0xf2, 0x80, 0xf5, 0x80,
	0xf9, 0x80, 0x00, 0x92, 0x0a, 0xb6, 0x17, 0xc9,
	0x28, 0xca, 0x3b, 0xbd, 0x49, 0xa3, 0x54, 0x80,
	0x5a, 0x80, 0x5c, 0xa7, 0x5a, 0x7f, 0x58, 0x4a,
	0x55
};

static void test_mixrClip_fill(void)
{
	memcpy(test_mixrClip_src, test_mixrClip_fill_src, sizeof(test_mixrClip_fill_src));
	memcpy(test_mixrClip_dst8, test_mixrClip_fill_dst8, sizeof(test_mixrClip_fill_dst8));
	memcpy(test_mixrClip_dst16, test_mixrClip_fill_dst16, sizeof(test_mixrClip_fill_dst16));
}

static int test_mixrClip_dump(const uint8_t *t1, const uint16_t *t2)
{
	int retval=0;
	int i;
	if (memcmp(test_mixrClip_src, test_mixrClip_fill_src, sizeof(test_mixrClip_fill_src)))
	{
		retval=1;
		fprintf(stderr, "src failed\n");
		fprintf(stderr, "src_expected[]={");
		for (i=0;i<32;i++)
			fprintf(stderr, "0x%08x%s", (unsigned)test_mixrClip_fill_src[i], i<31?", ":"");
		fprintf(stderr, "};\n");
		fprintf(stderr, "src_result[]  ={");
		for (i=0;i<32;i++)
			fprintf(stderr, "0x%08x%s", (unsigned)test_mixrClip_src[i], i<31?", ":"");
		fprintf(stderr, "};\n");
	}
	if (memcmp(test_mixrClip_dst8, t1, 34 * sizeof (uint8_t)))
	{
		retval=1;
		fprintf(stderr, "dst8 failed\n");
		fprintf(stderr, "dst8_expected[]={");
		for (i=0;i<34;i++)
			fprintf(stderr, "0x%02x%s", t1[i], i<33?", ":"");
		fprintf(stderr, "}\n");
		fprintf(stderr, "dst8_result[]  ={");
		for (i=0;i<34;i++)
			fprintf(stderr, "0x%02x%s", test_mixrClip_dst8[i], i<33?", ":"");
		fprintf(stderr, "}\n");
	}
	if (memcmp(test_mixrClip_dst16, t2, 34 * sizeof (uint16_t)))
	{
		retval=1;
		fprintf(stderr, "dst16 failed\n");
		fprintf(stderr, "dst16_expected[]={");
		for (i=0;i<34;i++)
			fprintf(stderr, "0x%04x%s", t2[i], i<33?", ":"");
		fprintf(stderr, "}\n");
		fprintf(stderr, "dst16_result[]  ={");
		for (i=0;i<34;i++)
			fprintf(stderr, "0x%04x%s", test_mixrClip_dst16[i], i<33?", ":"");
		fprintf(stderr, "}\n");
	}
	return retval;
}

static int test_mixrClip(void)
{
	int retval = 0;

	fprintf(stderr, "mixrClip, unsigned, gain = 1.0, 16 bit\n");
	signedout=0;
	calcamptab(65535); /* gain = 1.0 */
	test_mixrClip_fill();
	mixrClip(test_mixrClip_dst16+1, test_mixrClip_src, 32, amptab, clipmax, 1);
	retval |= test_mixrClip_dump(test_mixrClip_fill_dst8, test_mixrClip_dst16_test1);

	fprintf(stderr, "mixrClip, unsigned, gain = 1.0, 8 bit\n");
	//signedout=0;
	//calcamptab(65535); /* gain = 1.0 */
	test_mixrClip_fill();
	mixrClip(test_mixrClip_dst8+1, test_mixrClip_src, 32, amptab, clipmax, 0);
	retval |= test_mixrClip_dump(test_mixrClip_dst8_test2, test_mixrClip_fill_dst16);

	fprintf(stderr, "mixrClip, signed, gain = 1.0, 16 bit\n");
	signedout=1;
	calcamptab(65535); /* gain = 1.0 */
	test_mixrClip_fill();
	mixrClip(test_mixrClip_dst16+1, test_mixrClip_src, 32, amptab, clipmax, 1);
	retval |= test_mixrClip_dump(test_mixrClip_fill_dst8, test_mixrClip_dst16_test3);

	fprintf(stderr, "mixrClip, signed, gain = 1.0, 8 bit\n");
	//signedout=1;
	//calcamptab(65535); /* gain = 1.0 */
	test_mixrClip_fill();
	mixrClip(test_mixrClip_dst8+1, test_mixrClip_src, 32, amptab, clipmax, 0);
	retval |= test_mixrClip_dump(test_mixrClip_dst8_test4, test_mixrClip_fill_dst16);

	return retval;
}

static int initAsm(void)
{
#ifdef I386_ASM
	/* Self-modifying code needs access to modify it self */
	{
		int fd;
		char *file=strdup("/tmp/ocpXXXXXX");
		char *start1, *stop1/*, *start2, *stop2*/;
		int len1/*, len2*/;
		fd=mkstemp(file);

		start1=(void *)remap_range1_start;
		stop1=(void *)remap_range1_stop;
		/*start2=(void *)remap_range2_start;
		stop2=(void *)remap_range2_stop;*/
#ifdef MIXER_DEBUG
		fprintf(stderr, "range1: %p - %p\n", start1, stop1);
		/*fprintf(stderr, "range2: %p - %p\n", start2, stop2);*/
#endif

		start1=(char *)(((int)start1)&~(pagesize()-1));
		/*start2=(char *)(((int)start2)&~(pagesize()-1));*/
		len1=((stop1-start1)+pagesize()-1)& ~(pagesize()-1);
		/*len2=((stop2-start2)+pagesize-1)& ~(pagesize()-1);*/
#ifdef MIXER_DEBUG
		fprintf(stderr, "mprot: %p + %08x\n", start1, len1);
		/*fprintf(stderr, "mprot: %p + %08x\n", start2, len2);*/
#endif
		if (write(fd, start1, len1)!=len1)
		{
#ifdef MIXER_DEBUG
			fprintf(stderr, "write 1 failed\n");
#endif
			return 1;
		}
		/*
		if (write(fd, start2, len2)!=len2)
		{
#ifdef MIXER_DEBUG
			fprintf(stderr, "write 2 failed\n");
#endif
			return 0;
		}*/

		if (mmap(start1, len1, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED, fd, 0)==MAP_FAILED)
		{
			perror("mmap()");
			return 1;
		}
		/*
		if (mmap(start2, len2, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED, fd, len1)==MAP_FAILED)
		{
			perror("mmap()");
			return 0;
		}
		*/
/*
		if (mprotect((char *)(((int)remap_range1_start)&~(pagesize()-1)), (((char *)remap_range1_stop-(char *)remap_range1_start)+pagesize()-1)& ~(pagesize()-1), PROT_READ|PROT_WRITE|PROT_EXEC) ||
		    mprotect((char *)(((int)remap_range2_start)&~(pagesize()-1)), (((char *)remap_range2_stop-(char *)remap_range2_start)+pagesize()-1)& ~(pagesize()-1), PROT_READ|PROT_WRITE|PROT_EXEC) )
		{
			perror("Couldn't mprotect");
			return 0;
		}
*/
#ifdef MIXER_DEBUG
		fprintf(stderr, "Done ?\n");
#endif
		close(fd);
		unlink(file);
		free(file);
	}
#endif
	return 0;
}

static int test_mixrFadeChannel(void)
{
	int16_t samples16[] =
	{
		0xef01,
		0xf012,
		0x0123,
		0x1234,
		0x2345,
		0x3456,
		0x4567,
		0x5678,
		0x6789,
		0x789a,
		0x89ab,
		0x9abc,
		0xabcd,
		0xbcde,
		0xcdef
	};
	int8_t samples8[] =
	{
		0x4f,
		0xc0,
		0x01,
		0x12,
		0x23,
		0x34,
		0x45,
		0x56,
		0x67,
		0x89,
		0x9a,
		0xab,
		0xbc,
		0xcd,
		0xde,
		0xef
	};
	int32_t fade[2];
	struct channel dummy;

	int retval = 0;

	mixrSetupAddresses(&voltabsr[256], interpoltabr);
	calcinterpoltabr();
	calcvoltabsr();

	fprintf(stderr, "mixrFadeChannel 8 bit 0,0\n");

	fade[0]=0;
	fade[1]=0;
	dummy.curvols[0] = 0;
	dummy.curvols[1] = 0;
	dummy.samp=samples8;
	dummy.realsamp.bit8=samples8;
	dummy.pos=1;
	dummy.status = MIXRQ_PLAYING;
	mixrFadeChannel(fade, &dummy);
	if (dummy.curvols[0])
	{
		fprintf(stderr, "c->curvols[0]!=0 (%d)\n", dummy.curvols[0]);
		retval|=1;
	}
	if (dummy.curvols[1])
	{
		fprintf(stderr, "c->curvols[1]!=0 (%d)\n", dummy.curvols[1]);
		retval|=1;
	}
	if (dummy.pos!=1)
	{
		fprintf(stderr, "c->pos!=1 (%d)\n", dummy.pos);
		retval|=1;
	}
	if (dummy.status!=MIXRQ_PLAYING)
	{
		fprintf(stderr, "c->status!=%d (%d)\n", MIXRQ_PLAYING, dummy.status);
		retval|=1;
	}
	if (fade[0]!=0)
	{
		fprintf(stderr, "fade[0]!=0 (%d)\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=0)
	{
		fprintf(stderr, "fade[1]!=0 (%d)\n", fade[1]);
		retval|=1;
	}
	fprintf(stderr, "mixrFadeChannel 8 bit 126,90\n");

	fade[0]=1;
	fade[1]=1;
	dummy.curvols[0] = 126;
	dummy.curvols[1] = 90;
	dummy.samp=samples8;
	dummy.realsamp.bit8=samples8;
	dummy.pos=1;
	dummy.status = MIXRQ_PLAYING;
	mixrFadeChannel(fade, &dummy);
	if (dummy.curvols[0])
	{
		fprintf(stderr, "c->curvols[0]!=0 (%d)\n", dummy.curvols[0]);
		retval|=1;
	}
	if (dummy.curvols[1])
	{
		fprintf(stderr, "c->curvols[1]!=0 (%d)\n", dummy.curvols[1]);
		retval|=1;
	}
	if (dummy.pos!=1)
	{
		fprintf(stderr, "c->pos!=1 (%d)\n", dummy.pos);
		retval|=1;
	}
	if (dummy.status!=MIXRQ_PLAYING)
	{
		fprintf(stderr, "c->status!=%d (%d)\n", MIXRQ_PLAYING, dummy.status);
		retval|=1;
	}
	if (fade[0]!=-8063)
	{
		fprintf(stderr, "fade[0]!=-8063 (%d)\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=-5759)
	{
		fprintf(stderr, "fade[1]!=-5759 (%d)\n", fade[1]);
		retval|=1;
	}

	fprintf(stderr, "mixrFadeChannel 8 bit -54,45\n");

	fade[0]=1;
	fade[1]=1;
	dummy.curvols[0] = -54;
	dummy.curvols[1] = 45;
	dummy.samp=samples8;
	dummy.realsamp.bit8=samples8;
	dummy.pos=1;
	dummy.status = MIXRQ_PLAYING;
	mixrFadeChannel(fade, &dummy);
	if (dummy.curvols[0])
	{
		fprintf(stderr, "c->curvols[0]!=0 (%d)\n", dummy.curvols[0]);
		retval|=1;
	}
	if (dummy.curvols[1])
	{
		fprintf(stderr, "c->curvols[1]!=0 (%d)\n", dummy.curvols[1]);
		retval|=1;
	}
	if (dummy.pos!=1)
	{
		fprintf(stderr, "c->pos!=1 (%d)\n", dummy.pos);
		retval|=1;
	}
	if (dummy.status!=MIXRQ_PLAYING)
	{
		fprintf(stderr, "c->status!=%d (%d)\n", MIXRQ_PLAYING, dummy.status);
		retval|=1;
	}
	if (fade[0]!=3457)
	{
		fprintf(stderr, "fade[0]!=3457 (%d)\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=-2879)
	{
		fprintf(stderr, "fade[1]!=-2879 (%d)\n", fade[1]);
		retval|=1;
	}

	fprintf(stderr, "mixrFadeChannel 16 bit 0,0\n");

	fade[0]=0;
	fade[1]=0;
	dummy.curvols[0] = 0;
	dummy.curvols[1] = 0;
	dummy.samp=(void*)((unsigned long)samples16>>1);
	dummy.realsamp.bit16=samples16;
	dummy.pos=1;
	dummy.status = MIXRQ_PLAYING | MIXRQ_PLAY16BIT;
	mixrFadeChannel(fade, &dummy);
	if (dummy.curvols[0])
	{
		fprintf(stderr, "c->curvols[0]!=0 (%d)\n", dummy.curvols[0]);
		retval|=1;
	}
	if (dummy.curvols[1])
	{
		fprintf(stderr, "c->curvols[1]!=0 (%d)\n", dummy.curvols[1]);
		retval|=1;
	}
	if (dummy.pos!=1)
	{
		fprintf(stderr, "c->pos!=1 (%d)\n", dummy.pos);
		retval|=1;
	}
	if (dummy.status!=(MIXRQ_PLAYING|MIXRQ_PLAY16BIT))
	{
		fprintf(stderr, "c->status!=%d (%d)\n", MIXRQ_PLAYING|MIXRQ_PLAY16BIT, dummy.status);
		retval|=1;
	}
	if (fade[0]!=0)
	{
		fprintf(stderr, "fade[0]!=0 (%d)\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=0)
	{
		fprintf(stderr, "fade[1]!=0 (%d)\n", fade[1]);
		retval|=1;
	}
	fprintf(stderr, "mixrFadeChannel 16 bit 126,90\n");

	fade[0]=1;
	fade[1]=1;
	dummy.curvols[0] = 126;
	dummy.curvols[1] = 90;
	dummy.samp=(void*)((unsigned long)samples16>>1);
	dummy.realsamp.bit16=samples16;
	dummy.pos=1;
	dummy.status = MIXRQ_PLAYING|MIXRQ_PLAY16BIT;
	mixrFadeChannel(fade, &dummy);
	if (dummy.curvols[0])
	{
		fprintf(stderr, "c->curvols[0]!=0 (%d)\n", dummy.curvols[0]);
		retval|=1;
	}
	if (dummy.curvols[1])
	{
		fprintf(stderr, "c->curvols[1]!=0 (%d)\n", dummy.curvols[1]);
		retval|=1;
	}
	if (dummy.pos!=1)
	{
		fprintf(stderr, "c->pos!=1 (%d)\n", dummy.pos);
		retval|=1;
	}
	if (dummy.status!=(MIXRQ_PLAYING|MIXRQ_PLAY16BIT))
	{
		fprintf(stderr, "c->status!=%d (%d)\n", MIXRQ_PLAYING|MIXRQ_PLAY16BIT, dummy.status);
		retval|=1;
	}
	if (fade[0]!=-2015)
	{
		fprintf(stderr, "fade[0]!=-2015 (%d)\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=-1439)
	{
		fprintf(stderr, "fade[1]!=-1439 (%d)\n", fade[1]);
		retval|=1;
	}

	fprintf(stderr, "mixrFadeChannel 16 bit -54,45\n");

	fade[0]=1;
	fade[1]=1;
	dummy.curvols[0] = -54;
	dummy.curvols[1] = 45;
	dummy.samp=(void*)((unsigned long)samples16>>1);
	dummy.realsamp.bit16=samples16;
	dummy.pos=1;
	dummy.status = MIXRQ_PLAYING|MIXRQ_PLAY16BIT;
	mixrFadeChannel(fade, &dummy);
	if (dummy.curvols[0])
	{
		fprintf(stderr, "c->curvols[0]!=0 (%d)\n", dummy.curvols[0]);
		retval|=1;
	}
	if (dummy.curvols[1])
	{
		fprintf(stderr, "c->curvols[1]!=0 (%d)\n", dummy.curvols[1]);
		retval|=1;
	}
	if (dummy.pos!=1)
	{
		fprintf(stderr, "c->pos!=1 (%d)\n", dummy.pos);
		retval|=1;
	}
	if (dummy.status!=(MIXRQ_PLAYING|MIXRQ_PLAY16BIT))
	{
		fprintf(stderr, "c->status!=%d (%d)\n", MIXRQ_PLAYING|MIXRQ_PLAY16BIT, dummy.status);
		retval|=1;
	}
	if (fade[0]!=865)
	{
		fprintf(stderr, "fade[0]!=865 (%d)\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=-719)
	{
		fprintf(stderr, "fade[1]!=-719 (%d)\n", fade[1]);
		retval|=1;
	}

	return retval;
}

int32_t test_mixrFade_buf[22];
const int32_t test_mixrFade_test1[22] = {0xfffffd31, 0xfffffd36, 0xfffffd3b, 0xfffffd40, 0xfffffd45, 0xfffffd4a, 0xfffffd4f, 0xfffffd54, 0xfffffd59, 0xfffffd5e, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878, 0x78787878};
const int32_t test_mixrFade_test2[22] = {0x00000000, 0x00000029, 0x00000000, 0x00000028, 0x00000000, 0x00000027, 0x00000000, 0x00000026, 0x00000000, 0x00000025, 0x00000000, 0x00000024, 0x00000000, 0x00000023, 0x00000000, 0x00000022, 0x00000000, 0x00000021, 0x00000000, 0x00000020, 0x78787878, 0x78787878};

static void test_mixrFade_fill(void)
{
	memset(test_mixrFade_buf, 0x78, sizeof(test_mixrFade_buf));
}

static int test_mixrFade_dump(const int32_t *should)
{
	int i;
	if (memcmp(should, test_mixrFade_buf, sizeof(test_mixrFade_buf)))
	{
		fprintf(stderr, "buf_expected[] = {\n");
		for (i=0;i<22;i++)
			fprintf(stderr, "0x%08x%s", should[i], i!=21?", ":"");
		fprintf(stderr, "}\n");
		fprintf(stderr, "buf_result[]= {\n");
		for (i=0;i<22;i++)
			fprintf(stderr, "0x%08x%s", test_mixrFade_buf[i], i!=21?", ":"");
		fprintf(stderr, "}\n");
		return 1;
	}
	return 0;
}


static int test_mixrFade(void)
{
	int retval = 0;
	int32_t fade[2];

	fprintf(stderr, "mixrFade, mono\n");
	fade[0]=-719;
	fade[1]=4;
	test_mixrFade_fill();
	mixrFade(test_mixrFade_buf, fade, 10, 0);
	retval|=test_mixrFade_dump(test_mixrFade_test1);
	if (fade[0]!=-669)
	{
		fprintf(stderr, "fade[0]=%d, should have been -669\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=4)
	{
		fprintf(stderr, "fade[1]=%d, should have been 4\n", fade[1]);
		retval|=1;
	}


	fprintf(stderr, "mixrFade, stereo\n");
	fade[0]=0;
	fade[1]=41;
	test_mixrFade_fill();
	mixrFade(test_mixrFade_buf, fade, 10, 1);
	test_mixrFade_dump(test_mixrFade_test2);
	if (fade[0]!=0)
	{
		fprintf(stderr, "fade[0]=%d, should have been 0\n", fade[0]);
		retval|=1;
	}
	if (fade[1]!=31)
	{
		fprintf(stderr, "fade[1]=%d, should have been 31\n", fade[1]);
		retval|=1;
	}
	return retval;
}

static int16_t test_mixrPlayChannel_fill16[] =
{
	0x1010,
	0x5098,
	0x7fff,
	0x49a0,
	0x0303,
	0xe103,
	0xa309,
	0x8000,
	0xb243,
	0xef0c,

	0x0000
};

static int8_t test_mixrPlayChannel_fill8[] =
{
	0x10,
	0x50,
	0x7f,
	0x49,
	0x03,
	0xe1,
	0xa3,
	0x80,
	0xb2,
	0xef,

	0x00
};

static int32_t test_mixrPlayChannel_buf[34];
static int32_t fadebuf[2];

static const int32_t test_mixrPlayChannel_test1[] = {0x010114b1, 0x010112af, 0x0100ead8, 0x0100fd05, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test2[] = {0x010114b1, 0x010114b1, 0x010112af, 0x010112af, 0x0100ead8, 0x0100ead8, 0x0100fd05, 0x0100fd05, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test3[] = {0x010114b1, 0x010112af, 0x0100ead8, 0x0100fd05, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test4[] = {0x010114b1, 0x010114b1, 0x010112af, 0x010112af, 0x0100ead8, 0x0100ead8, 0x0100fd05, 0x0100fd05, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test5[] = {0x0101152f, 0x0101070f, 0x0100e7bf, 0x0100fd7d, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test6[] = {0x0101152f, 0x0101152f, 0x0101070f, 0x0101070f, 0x0100e7bf, 0x0100e7bf, 0x0100fd7d, 0x0100fd7d, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test7[] = {0x0101152f, 0x0101070f, 0x0100e7bf, 0x0100fd7d, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test8[] = {0x0101152f, 0x0101152f, 0x0101070f, 0x0101070f, 0x0100e7bf, 0x0100e7bf, 0x0100fd7d, 0x0100fd7d, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test9[] = {0x010114b1, 0x010112af, 0x0100ead8, 0x01011ec5, 0x010101b2, 0x0100e401, 0x01011142, 0x0100fa39, 0x0100f03f, 0x010101a6, 0x0100ed06, 0x01011c4a, 0x0100fa58, 0x0100e581, 0x010110b0, 0x0100ed06, 0x0100f03f, 0x010101a6, 0x0100e581, 0x01011c4a, 0x0100fa58, 0x0100f03f, 0x010110b0, 0x0100ed06, 0x01011c4a, 0x010101a6, 0x0100e581, 0x010110b0, 0x0100fa58, 0x0100f03f, 0x010101a6, 0x0100ed06, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test10[] = {0x010114b1, 0x010114b1, 0x010112af, 0x010112af, 0x0100ead8, 0x0100ead8, 0x01011ec5, 0x01011ec5, 0x010101b2, 0x010101b2, 0x0100e401, 0x0100e401, 0x01011142, 0x01011142, 0x0100fa39, 0x0100fa39, 0x0100f03f, 0x0100f03f, 0x010101a6, 0x010101a6, 0x0100ed06, 0x0100ed06, 0x01011c4a, 0x01011c4a, 0x0100fa58, 0x0100fa58, 0x0100e581, 0x0100e581, 0x010110b0, 0x010110b0, 0x0100ed06, 0x0100ed06, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test11[] = {0x010114b1, 0x010112af, 0x0100ead8, 0x01011ec5, 0x010101b2, 0x0100e401, 0x01011142, 0x0100fa39, 0x0100f03f, 0x010101a6, 0x0100ed06, 0x01011c4a, 0x0100fa58, 0x0100e581, 0x010110b0, 0x0100ed06, 0x0100f03f, 0x010101a6, 0x0100e581, 0x01011c4a, 0x0100fa58, 0x0100f03f, 0x010110b0, 0x0100ed06, 0x01011c4a, 0x010101a6, 0x0100e581, 0x010110b0, 0x0100fa58, 0x0100f03f, 0x010101a6, 0x0100ed06, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test12[] = {0x010114b1, 0x010114b1, 0x010112af, 0x010112af, 0x0100ead8, 0x0100ead8, 0x01011ec5, 0x01011ec5, 0x010101b2, 0x010101b2, 0x0100e401, 0x0100e401, 0x01011142, 0x01011142, 0x0100fa39, 0x0100fa39, 0x0100f03f, 0x0100f03f, 0x010101a6, 0x010101a6, 0x0100ed06, 0x0100ed06, 0x01011c4a, 0x01011c4a, 0x0100fa58, 0x0100fa58, 0x0100e581, 0x0100e581, 0x010110b0, 0x010110b0, 0x0100ed06, 0x0100ed06, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test13[] = {0x0101152f, 0x0101070f, 0x0100e7bf, 0x01011e11, 0x0100fbb4, 0x0100e815, 0x0101105e, 0x0100f029, 0x0100f60c, 0x01010138, 0x0100e770, 0x01011722, 0x0100f97c, 0x0100ed74, 0x01010a3e, 0x0100ec2a, 0x0100fa21, 0x0100fe6d, 0x0100e6cb, 0x010112d6, 0x0100f48b, 0x0100f1c0, 0x01010471, 0x0100e95f, 0x01011b00, 0x0100fba2, 0x0100eae0, 0x01010de5, 0x0100ef9a, 0x0100f6b1, 0x0101005c, 0x0100e694, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test14[] = {0x0101152f, 0x0101152f, 0x0101070f, 0x0101070f, 0x0100e7bf, 0x0100e7bf, 0x01011e11, 0x01011e11, 0x0100fbb4, 0x0100fbb4, 0x0100e815, 0x0100e815, 0x0101105e, 0x0101105e, 0x0100f029, 0x0100f029, 0x0100f60c, 0x0100f60c, 0x01010138, 0x01010138, 0x0100e770, 0x0100e770, 0x01011722, 0x01011722, 0x0100f97c, 0x0100f97c, 0x0100ed74, 0x0100ed74, 0x01010a3e, 0x01010a3e, 0x0100ec2a, 0x0100ec2a, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test15[] = {0x0101152f, 0x0101070f, 0x0100e7bf, 0x01011e11, 0x0100fbb4, 0x0100e815, 0x0101105e, 0x0100f029, 0x0100f60c, 0x01010138, 0x0100e770, 0x01011722, 0x0100f97c, 0x0100ed74, 0x01010a3e, 0x0100ec2a, 0x0100fa21, 0x0100fe6d, 0x0100e6cb, 0x010112d6, 0x0100f48b, 0x0100f1c0, 0x01010471, 0x0100e95f, 0x01011b00, 0x0100fba2, 0x0100eae0, 0x01010de5, 0x0100ef9a, 0x0100f6b1, 0x0101005c, 0x0100e694, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test16[] = {0x0101152f, 0x0101152f, 0x0101070f, 0x0101070f, 0x0100e7bf, 0x0100e7bf, 0x01011e11, 0x01011e11, 0x0100fbb4, 0x0100fbb4, 0x0100e815, 0x0100e815, 0x0101105e, 0x0101105e, 0x0100f029, 0x0100f029, 0x0100f60c, 0x0100f60c, 0x01010138, 0x01010138, 0x0100e770, 0x0100e770, 0x01011722, 0x01011722, 0x0100f97c, 0x0100f97c, 0x0100ed74, 0x0100ed74, 0x01010a3e, 0x01010a3e, 0x0100ec2a, 0x0100ec2a, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test17[] = {0x010114b1, 0x010112af, 0x0100ead8, 0x0100eeb9, 0x0100eb92, 0x0101118b, 0x01011142, 0x0100fa39, 0x0100f03f, 0x0100ed06, 0x010101a6, 0x01011c4a, 0x0100fa58, 0x0100e581, 0x0100e581, 0x010101a6, 0x01011c4a, 0x010101a6, 0x0100e581, 0x0100f03f, 0x0100fa58, 0x01011c4a, 0x010110b0, 0x0100ed06, 0x0100f03f, 0x0100ed06, 0x010110b0, 0x010110b0, 0x0100fa58, 0x0100f03f, 0x0100ed06, 0x010101a6, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test18[] = {0x010114b1, 0x010114b1, 0x010112af, 0x010112af, 0x0100ead8, 0x0100ead8, 0x0100eeb9, 0x0100eeb9, 0x0100eb92, 0x0100eb92, 0x0101118b, 0x0101118b, 0x01011142, 0x01011142, 0x0100fa39, 0x0100fa39, 0x0100f03f, 0x0100f03f, 0x0100ed06, 0x0100ed06, 0x010101a6, 0x010101a6, 0x01011c4a, 0x01011c4a, 0x0100fa58, 0x0100fa58, 0x0100e581, 0x0100e581, 0x0100e581, 0x0100e581, 0x010101a6, 0x010101a6, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test19[] = {0x010114b1, 0x010112af, 0x0100ead8, 0x0100eeb9, 0x0100eb92, 0x0101118b, 0x01011142, 0x0100fa39, 0x0100f03f, 0x0100ed06, 0x010101a6, 0x01011c4a, 0x0100fa58, 0x0100e581, 0x0100e581, 0x010101a6, 0x01011c4a, 0x010101a6, 0x0100e581, 0x0100f03f, 0x0100fa58, 0x01011c4a, 0x010110b0, 0x0100ed06, 0x0100f03f, 0x0100ed06, 0x010110b0, 0x010110b0, 0x0100fa58, 0x0100f03f, 0x0100ed06, 0x010101a6, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test20[] = {0x010114b1, 0x010114b1, 0x010112af, 0x010112af, 0x0100ead8, 0x0100ead8, 0x0100eeb9, 0x0100eeb9, 0x0100eb92, 0x0100eb92, 0x0101118b, 0x0101118b, 0x01011142, 0x01011142, 0x0100fa39, 0x0100fa39, 0x0100f03f, 0x0100f03f, 0x0100ed06, 0x0100ed06, 0x010101a6, 0x010101a6, 0x01011c4a, 0x01011c4a, 0x0100fa58, 0x0100fa58, 0x0100e581, 0x0100e581, 0x0100e581, 0x0100e581, 0x010101a6, 0x010101a6, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test21[] = {0x0101152f, 0x0101070f, 0x0100e7bf, 0x0100fb61, 0x0100ea30, 0x0101087b, 0x0101105e, 0x0100f029, 0x0100f60c, 0x0100e694, 0x0101005c, 0x01011722, 0x0100f97c, 0x0100ed74, 0x0100eae0, 0x0100fba2, 0x01011a24, 0x0100fe6d, 0x0100e6cb, 0x0100f1c0, 0x0100f3af, 0x010112d6, 0x01010471, 0x0100e95f, 0x0100fafd, 0x0100ec2a, 0x01010a3e, 0x01010de5, 0x0100ef9a, 0x0100f6b1, 0x0100e770, 0x01010138, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test22[] = {0x0101152f, 0x0101152f, 0x0101070f, 0x0101070f, 0x0100e7bf, 0x0100e7bf, 0x0100fb61, 0x0100fb61, 0x0100ea30, 0x0100ea30, 0x0101087b, 0x0101087b, 0x0101105e, 0x0101105e, 0x0100f029, 0x0100f029, 0x0100f60c, 0x0100f60c, 0x0100e694, 0x0100e694, 0x0101005c, 0x0101005c, 0x01011722, 0x01011722, 0x0100f97c, 0x0100f97c, 0x0100ed74, 0x0100ed74, 0x0100eae0, 0x0100eae0, 0x0100fba2, 0x0100fba2, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test23[] = {0x0101152f, 0x0101070f, 0x0100e7bf, 0x0100fb61, 0x0100ea30, 0x0101087b, 0x0101105e, 0x0100f029, 0x0100f60c, 0x0100e694, 0x0101005c, 0x01011722, 0x0100f97c, 0x0100ed74, 0x0100eae0, 0x0100fba2, 0x01011a24, 0x0100fe6d, 0x0100e6cb, 0x0100f1c0, 0x0100f3af, 0x010112d6, 0x01010471, 0x0100e95f, 0x0100fafd, 0x0100ec2a, 0x01010a3e, 0x01010de5, 0x0100ef9a, 0x0100f6b1, 0x0100e770, 0x01010138, 0x01010101, 0x01010101};
static const int32_t test_mixrPlayChannel_test24[] = {0x0101152f, 0x0101152f, 0x0101070f, 0x0101070f, 0x0100e7bf, 0x0100e7bf, 0x0100fb61, 0x0100fb61, 0x0100ea30, 0x0100ea30, 0x0101087b, 0x0101087b, 0x0101105e, 0x0101105e, 0x0100f029, 0x0100f029, 0x0100f60c, 0x0100f60c, 0x0100e694, 0x0100e694, 0x0101005c, 0x0101005c, 0x01011722, 0x01011722, 0x0100f97c, 0x0100f97c, 0x0100ed74, 0x0100ed74, 0x0100eae0, 0x0100eae0, 0x0100fba2, 0x0100fba2, 0x01010101, 0x01010101};

static void test_mixrPlayChannel_fill(int bit16, struct channel *c, int status)
{
	memset(c, 0, sizeof(*c));
	if (bit16)
	{
		c->samp=(void*)((unsigned long)test_mixrPlayChannel_fill16>>1);
		c->realsamp.bit16 = test_mixrPlayChannel_fill16;
		c->length=10;
		status|=MIXRQ_PLAY16BIT;
	} else {
		c->samp=test_mixrPlayChannel_fill8;
		c->realsamp.bit8 = test_mixrPlayChannel_fill8;
		c->length=10;
	}
	fadebuf[0]=100;
	fadebuf[1]=-100;
	c->status=status | MIXRQ_PLAYING;
	c->pos=1;
	c->fpos=0x1234;
	c->step=0x0002abcd;
	c->length=10;
	c->loopstart=2;
	c->loopend=9;
	c->replen=7;
	memset(test_mixrPlayChannel_buf, 1, sizeof(test_mixrPlayChannel_buf));
	c->curvols[0]=63;
	c->curvols[1]=63;
	c->dstvols[0]=55;
	c->dstvols[1]=55;
}

static int test_mixrPlayChannel_dump(struct channel *ch, const int32_t *target, int32_t fadebuf0, int32_t fadebuf1, int status, uint32_t pos, uint32_t fpos, int curvols0, int curvols1)
{
	int i;
	int retval = 0;
	if (memcmp(target, test_mixrPlayChannel_buf, sizeof(test_mixrPlayChannel_buf)))
	{
		fprintf(stderr, "buf_expected[] = {\n");
		for (i=0;i<34;i++)
			fprintf(stderr, "%s0x%08x", i?", ":"", target[i]);
		fprintf(stderr, "\n}\n");

		fprintf(stderr, "buf_result[] = {\n");
		for (i=0;i<34;i++)
			fprintf(stderr, "%s0x%08x", i?", ":"", test_mixrPlayChannel_buf[i]);
		fprintf(stderr, "\n}\n");
		retval |= 1;
	}
	if (fadebuf[0]!=fadebuf0)
	{
		fprintf(stderr, "fadebuf[0]=%d (expected %d)\n", fadebuf[0], fadebuf0);
		retval |= 1;
	}
	if (fadebuf[1]!=fadebuf1)
	{
		fprintf(stderr, "fadebuf[1]=%d (expected %d)\n", fadebuf[1], fadebuf1);
		retval |= 1;
	}

	if (ch->status!=status)
	{
		fprintf(stderr, "ch->status=0x%02x (expected 0x%02x)\n", ch->status, status);
		retval |= 1;
	}
	if ((ch->pos!=pos)|(ch->fpos!=fpos))
	{
		fprintf(stderr, "ch->pos=0x%08x.%04x (expected 0x%08x.%04x)\n", ch->pos, ch->fpos, pos, fpos);
		retval |= 1;
	}
	if (ch->curvols[0]!=curvols0)
	{
		fprintf(stderr, "ch->curvols[0]=0x%02x (expected 0x%02x)\n", ch->curvols[0], curvols0);
		retval |= 1;
	}
	if (ch->curvols[1]!=curvols1)
	{
		fprintf(stderr, "ch->curvols[1]=0x%02x (expected 0x%02x)\n", ch->curvols[1], curvols1);
		retval |= 1;
	}
	return retval;
}

static int test_mixrPlayChannel(void)
{
	int retval = 0;
	struct channel ch;

	fprintf (stderr, "mixrPlayChannel, mono, 8 bit\n");
	test_mixrPlayChannel_fill (0, &ch, 0);
	mixrPlayChannel (test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test1, 100, -100, 0, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, stereo, 8 bit\n");
	test_mixrPlayChannel_fill(0, &ch, 0);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test2, 100, -100, 0, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, mono, 16 bit\n");
	test_mixrPlayChannel_fill(1, &ch, 0);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test3, 100, -100, MIXRQ_PLAY16BIT, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, stereo, 16 bit\n");
	test_mixrPlayChannel_fill(1, &ch, 0);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test4, 100, -100, MIXRQ_PLAY16BIT, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, mono, 8 bit, interpolate\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_INTERPOLATE);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test5, 100, -100, MIXRQ_INTERPOLATE, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, stereo, 8 bit, interpolate\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_INTERPOLATE);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test6, 100, -100, MIXRQ_INTERPOLATE, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, mono, 16 bit, interpolate\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_INTERPOLATE);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test7, 100, -100, MIXRQ_INTERPOLATE|MIXRQ_PLAY16BIT, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, stereo, 16 bit, interpolate\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_INTERPOLATE);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test8, 100, -100, MIXRQ_INTERPOLATE|MIXRQ_PLAY16BIT, 0x0000000a, 0xc168, 0, 0);

	fprintf(stderr, "mixrPlayChannel, mono, 8 bit, looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test9, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 8 bit, looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test10, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED, 0x00000008, 0xcf04, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, mono, 16 bit, looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test11, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PLAY16BIT, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 16 bit, looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test12, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PLAY16BIT, 0x00000008, 0xcf04, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, mono, 8 bit, interpolate, looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test13, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_INTERPOLATE, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 8 bit, interpolate, looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test14, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_INTERPOLATE, 0x00000008, 0xcf04, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, mono, 16 bit, interpolate, looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test15, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PLAY16BIT|MIXRQ_INTERPOLATE, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 16 bit, interpolate, looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test16, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PLAY16BIT|MIXRQ_INTERPOLATE, 0x00000008, 0xcf04, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, mono, 8 bit, ping-pong looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test17, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 8 bit, ping-pong looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test18, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP, 0x00000002, 0x30fc, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, mono, 16 bit, ping-pong looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test19, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PLAY16BIT|MIXRQ_PINGPONGLOOP, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 16 bit, ping-pong looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test20, 100, -100, MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PLAY16BIT|MIXRQ_PINGPONGLOOP, 0x00000002, 0x30fc, 0x37, 0x37);


	fprintf(stderr, "mixrPlayChannel, mono, 8 bit, interpolate, ping-pong looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test21, 100, -100, MIXRQ_PLAYING|MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 8 bit, interpolate, ping-pong looped\n");
	test_mixrPlayChannel_fill(0, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test22, 100, -100, MIXRQ_PLAYING|MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP, 0x00000002, 0x30fc, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, mono, 16 bit, interpolate, ping-pong looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 32, &ch, 0);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test23, 100, -100, MIXRQ_PLAYING|MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PLAY16BIT|MIXRQ_PINGPONGLOOP, 0x00000002, 0x8bd4, 0x37, 0x37);

	fprintf(stderr, "mixrPlayChannel, stereo, 16 bit, interpolate, ping-pong looped\n");
	test_mixrPlayChannel_fill(1, &ch, MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
	mixrPlayChannel(test_mixrPlayChannel_buf, fadebuf, 16, &ch, 1);
	retval |= test_mixrPlayChannel_dump (&ch, test_mixrPlayChannel_test24, 100, -100, MIXRQ_PLAYING|MIXRQ_INTERPOLATE|MIXRQ_LOOPED|MIXRQ_PLAY16BIT|MIXRQ_PINGPONGLOOP, 0x00000002, 0x30fc, 0x37, 0x37);

	return retval;
}

int main(int argc, char *argv[])
{
	int retval=0;

	if (initAsm())
		return 1;

	test_mixrClip_dst8  = malloc (34 * sizeof (uint8_t));
	test_mixrClip_dst16 = malloc (34 * sizeof (uint16_t));
	test_mixrClip_src   = malloc (32 * sizeof (uint32_t));

	amptab=malloc(sizeof(int16_t)*3*256+sizeof(int32_t)); /* PADDING since assembler indexes some bytes beyond tab and ignores upper bits */ /*new short [3][256];*/

	voltabsr=malloc(sizeof(uint32_t)*513*256);

	interpoltabr=malloc(sizeof(uint8_t)*16*256*2);

	retval |= test_mixrClip();

	retval |= test_mixrFadeChannel();

	retval |= test_mixrFade();

	retval |= test_mixrPlayChannel();

	free (test_mixrClip_dst8);
	free (test_mixrClip_dst16);
	free (test_mixrClip_src);


	free(amptab);

	return retval;
}
