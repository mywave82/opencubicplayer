/* OpenCP Module Player
 * copyright (c) 2004-'122 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Unit-test for "dwmixqa.c"
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
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "types.h"
#include "dev/mix.h"
#include "devwmix.h"
#include "dwmix.h"
#include "dwmixqa.h"
#ifdef I386_ASM
#include "stuff/pagesize.inc.c"
#endif


static int initAsm(void)
{
#ifdef I386_ASM

	/* Self-modifying code needs access to modify it self */
	int fd;
	char file[128]="/tmp/ocpXXXXXX";
	char *start1, *stop1;
	int len1;
	fd=mkstemp(file);

	start1=(void *)remap_range2_start;
	stop1=(void *)remap_range2_stop;

	start1=(char *)(((int)start1)&~(pagesize()-1));
	len1=((stop1-start1)+pagesize()-1)& ~(pagesize()-1);
	if (write(fd, start1, len1)!=len1)
	{
		close(fd);
		unlink(file);
		return 0;
	}
	if (mmap(start1, len1, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED, fd, 0)==MAP_FAILED)
	{
		perror("mmap()");
		close(fd);
		unlink(file);
		return 0;
	}

	close(fd);
	unlink(file);
#endif

	return 1;
}

static int16_t (*voltabsq)[2][256];
static int16_t (*interpoltabq)[32][256][2];
static int16_t (*interpoltabq2)[16][256][4];

static void calcvoltabsq(void)
{
	int i,j;
	for (j=0; j<=512; j++)
	{
		long amp=j-256;
		for (i=0; i<256; i++)
		{
			int v=amp*(signed char)i;
			voltabsq[j][0][i]=(v==0x8000)?0x7FFF:v;
			voltabsq[j][1][i]=(amp*i)>>8;
		}
	}
}

static void calcinterpoltabq(void)
{
	int i,j;
	for (i=0; i<32; i++)
		for (j=0; j<256; j++)
		{
			interpoltabq[0][i][j][1]=(i*(signed char)j)<<3;
			interpoltabq[0][i][j][0]=(((signed char)j)<<8)-interpoltabq[0][i][j][1];
			interpoltabq[1][i][j][1]=(i*j)>>5;
			interpoltabq[1][i][j][0]=j-interpoltabq[1][i][j][1];
		}
	for (i=0; i<16; i++)
		for (j=0; j<256; j++)
		{
			interpoltabq2[0][i][j][0]=((16-i)*(16-i)*(signed char)j)>>1;
			interpoltabq2[0][i][j][2]=(i*i*(signed char)j)>>1;
			interpoltabq2[0][i][j][1]=(((signed char)j)<<8)-interpoltabq2[0][i][j][0]-interpoltabq2[0][i][j][2];
			interpoltabq2[1][i][j][0]=((16-i)*(16-i)*j)>>9;
			interpoltabq2[1][i][j][2]=(i*i*j)>>9;
			interpoltabq2[1][i][j][1]=j-interpoltabq2[1][i][j][0]-interpoltabq2[1][i][j][2];
		}
}

int main(int argc, char *argv[])
{
	struct channel tc;
	int c;
	int16_t target[128];

	tc.realsamp.bit8 = malloc(1024);
	for (c=0;c<1024;c++)
		tc.realsamp.bit8[c]=(int8_t)(c&0xff);
	tc.samp = tc.realsamp.bit8;
	tc.length=1024;
	tc.loopstart=3;
	tc.loopend=5;
	tc.replen=2;
	tc.step=0x00018000; /* 1.5 */
	tc.pos=0;
	tc.fpos=0;
	tc.status=MIXRQ_LOOPED;
	tc.curvols[0]=32;
	tc.curvols[1]=32;
	tc.curvols[2]=32;
	tc.curvols[3]=32;
	tc.dstvols[0]=32;
	tc.dstvols[1]=32;
	tc.dstvols[2]=32;
	tc.dstvols[3]=32;
	tc.vol[0]=32;
	tc.vol[1]=32;
	tc.orgvol[0]=32;
	tc.orgvol[1]=32;
	tc.orgrate=0x0001000;
	tc.orgfrq=220;
	tc.orgdiv=0;
	tc.volopt=0;
	tc.orgvolx=0x100;
	tc.orgpan=0x7f;
	tc.samptype=0;
	tc.orgloopstart=0;
	tc.orgloopend=0;
	tc.orgsloopstart=0;
	tc.orgsloopend=0;

	initAsm();

	voltabsq=malloc(sizeof(uint16_t)*513*2*256);
	interpoltabq=malloc(sizeof(uint16_t)*2*32*256*2);
	interpoltabq2=malloc(sizeof(uint16_t)*2*16*256*4);

	calcvoltabsq();
	calcinterpoltabq();

	mixqSetupAddresses(&voltabsq[256], interpoltabq, interpoltabq2);

	mixqPlayChannel(target, 128, &tc, 0);

	for (c=0;c<128;c++)
	{
		fprintf(stderr, "%04x\n", (uint16_t)target[c]);
	}

	fprintf(stderr, "tc.length=%d\n", tc.length);
	fprintf(stderr, "tc.loopstart=%d\n", tc.loopstart);
	fprintf(stderr, "tc.loopend=%d\n", tc.loopend);
	fprintf(stderr, "tc.replen=%d\n", tc.replen);
	fprintf(stderr, "tc.step=%d\n", tc.step);
	fprintf(stderr, "tc.pos=%d\n", tc.pos);
	fprintf(stderr, "tc.fpos=%d\n", tc.fpos);
	fprintf(stderr, "tc.status=%d\n", tc.status);
	fprintf(stderr, "tc.curvols[0]=%d\n", tc.curvols[0]);
	fprintf(stderr, "tc.curvols[1]=%d\n", tc.curvols[1]);
	fprintf(stderr, "tc.curvols[2]=%d\n", tc.curvols[2]);
	fprintf(stderr, "tc.curvols[3]=%d\n", tc.curvols[3]);
	fprintf(stderr, "tc.dstvols[0]=%d\n", tc.dstvols[0]);
	fprintf(stderr, "tc.dstvols[1]=%d\n", tc.dstvols[1]);
	fprintf(stderr, "tc.dstvols[2]=%d\n", tc.dstvols[2]);
	fprintf(stderr, "tc.dstvols[3]=%d\n", tc.dstvols[3]);
	fprintf(stderr, "tc.vol[0]=%d\n", tc.vol[0]);
	fprintf(stderr, "tc.vol[1]=%d\n", tc.vol[1]);
	fprintf(stderr, "tc.orgvol[0]=%d\n", tc.orgvol[0]);
	fprintf(stderr, "tc.orgvol[1]=%d\n", tc.orgvol[1]);
	fprintf(stderr, "tc.orgrate=%d\n", tc.orgrate);
	fprintf(stderr, "tc.orgfrq=%d\n", tc.orgfrq);
	fprintf(stderr, "tc.orgdiv=%d\n", tc.orgdiv);
	fprintf(stderr, "tc.volopt=%d\n", tc.volopt);
	fprintf(stderr, "tc.orgvolx=%d\n", tc.orgvolx);
	fprintf(stderr, "tc.orgpan=%d\n", tc.orgpan);
	fprintf(stderr, "tc.samptype=%d\n", tc.samptype);
	fprintf(stderr, "tc.orgloopstart=%d\n", tc.orgloopstart);
	fprintf(stderr, "tc.orgloopend=%d\n", tc.orgloopend);
	fprintf(stderr, "tc.orgsloopstart=%d\n", tc.orgsloopstart);
	fprintf(stderr, "tc.orgsloopend=%d\n", tc.orgsloopend);

	free(tc.realsamp.bit8);

	return 0;
}


