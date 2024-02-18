/* OpenCP Module Player
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * unit-test of "mchasm.c"
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
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "mchasm.h"


int retval=0;

char *OK10="\x1b[1m\x1b[32mok                \x1b[0m\x1b[37m";
char *FAILED10="\x1b[1m\x1b[31mfailed            \x1b[0m\x1b[37m";

void test4(void)
{
	int16_t samples_zero[10]={0x0000, 0x7F00, 0x0000, 0x00EE, 0x0000, 0x0011, 0x0000, 0x0033, 0x0000, 0x0006};
	int16_t samples_range[10]={0x0001, 0x0001, 0x0000, 0x7999, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0001};
	int16_t samples_positive[10]={0x0002, 0x0000, 0x0002, 0x7aaa, 0x0002, 0x7ddd, 0x0002, 0x0000, 0x0002, 0x1234};
	int16_t samples_negative[10]={-0x0002, 0x0000, -0x0002, 0x0100, -0x0002, -0x1000, -0x0002, -0x0f01, -0x0002, 0x4321};
	uint32_t result;

	fputs("mixAddAbs16SS():", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs16SS(samples_zero, 5))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs16SS(samples_range+2, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs16SS(samples_positive, 5))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs16SS(samples_negative, 5))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}
	fputs("\n", stderr);
}

typedef char pad_t[128];
pad_t masterpad;
void reset_pads(char *pad0, char *pad1, char *pad2)
{
	memset(pad0, 0, 128);
	memset(pad1, 0, 128);
	memset(pad2, 0, 128);
}

int check_pads(char *pad0, char *pad1, char *pad2)
{
	return memcmp(pad0, masterpad, 128)||memcmp(pad1, masterpad, 128)||memcmp(pad2, masterpad, 128);
}

void test23(void)
{
	pad_t pad0;
	int16_t src[20]={-2,0, 0,-4, -3,-3, 1,1, 2,2, 3,3, -1280,-1280, 1270,1270, 10,10, 11,11};
	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
	int16_t wewant1[10]={-1, -2, -3,     1,   2,  3, -1280, 1270, 10, 11};
	int16_t wewant2[10]={-1, -3,  2, -1280,  10,  0,     0,    0,   0, 0};
	int16_t wewant3[10]={-1, -1, -2,    -2,  -3, -3,     1,    1,   2, 2};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSS16M  (16bit, stereo, signed => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSS16M((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x %04x", (uint16_t)src[i*2], (uint16_t)src[i*2+1]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "     %04x ", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "     %04x ", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSS16M((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleSS16M((int16_t *)dst, src, 10, 0x0008000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant3, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}
	fputs("\n", stderr);
}

void test25(void)
{
	pad_t pad0;
	int16_t src[20]={-1,0, 0,-4, -3,-3, 1,1, 2,2, 3,3, -1280,-1280, 1270,1270, 10,10, 11,11};
	pad_t pad1;
	int16_t dst[20];
	pad_t pad2;
	int16_t wewant1[20]={-1,0,  0,-4, -3,-3,     1,    1,   2, 2,  3, 3, -1280,-1280, 1270,1270, 10,10, 11,11};
	int16_t wewant2[20]={-1,0, -3,-3,  2, 2, -1280,-1280,  10,10,  0, 0,     0,    0,    0,   0,  0, 0,  0, 0};
	int16_t wewant3[20]={-1,0, -1, 0,  0,-4,     0,   -4,  -3,-3, -3,-3,     1,    1,    1,   1,  2, 2,  2, 2};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSS16S  (16bit, stereo, signed => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSS16S((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x      ", (uint16_t)src[i*2]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x-%04x ", (uint16_t)dst[i*2], (uint16_t)dst[i*2+1]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x-%04x ", (uint16_t)wewant1[i*2], (uint16_t)wewant1[i*2+1]);
			fprintf(stderr, "\n");
		}
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSS16S((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleSS16S((int16_t *)dst, src, 10, 0x0008000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant3, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}
	fputs("\n", stderr);
}

int main(int argc, char *argv[])
{
	test4();
	memset(masterpad, 0, 128);
	test23();
	test25();

	return retval;
}
