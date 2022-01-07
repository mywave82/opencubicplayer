/* OpenCP Module Player
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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

void test1(void)
{
	uint16_t samples_zero[10]={0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000};
	uint16_t samples_range[10]={0x8001, 0x8000, 0x8010, 0x8011, 0x8011, 0x8011, 0x8011, 0x8011, 0x8011, 0x8001};
	uint16_t samples_positive[10]={0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001};
	uint16_t samples_negative[10]={0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff};
	uint32_t result;

	fputs("mixAddAbs16M(): ", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs16M(samples_zero, 10))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs16M(samples_range+1, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs16M(samples_positive, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs16M(samples_negative, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}
	fputs("\n", stderr);
}

void test2(void)
{
	int16_t samples_zero[10]={0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
	int16_t samples_range[10]={0x0001, 0x0000, 0x0010, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0001};
	int16_t samples_positive[10]={0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001};
	int16_t samples_negative[10]={-0x0001, -0x0001, -0x0001, -0x0001, -0x0001, -0x0001, -0x0001, -0x0001, -0x0001, -0x0001};
	uint32_t result;

	fputs("mixAddAbs16MS():", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs16MS(samples_zero, 10))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs16MS(samples_range+1, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs16MS(samples_positive, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs16MS(samples_negative, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}
	fputs("\n", stderr);
}

void test3(void)
{
	uint16_t samples_zero[10]={0x8000, 0x80ff, 0x8000, 0x80ff, 0x8000, 0x8fff, 0x8000, 0x9000, 0x8000, 0x1000};
	uint16_t samples_range[10]={0x8001, 0x8001, 0x8000, 0x8999, 0x8011, 0x8011, 0x8011, 0x8011, 0x8011, 0x8001};
	uint16_t samples_positive[10]={0x8002, 0x8000, 0x8002, 0x8000, 0x8002, 0x8000, 0x8002, 0x8000, 0x8002, 0x8001};
	uint16_t samples_negative[10]={0x7ffe, 0x7000, 0x7ffe, 0x7000, 0x7ffe, 0x7000, 0x7ffe, 0xffff, 0x7ffe, 0x1000};
	uint32_t result;

	fputs("mixAddAbs16S(): ", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs16S(samples_zero, 5))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs16S(samples_range+2, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs16S(samples_positive, 5))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs16S(samples_negative, 5))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}
	fputs("\n", stderr);
}

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

void test5(void)
{
	uint8_t samples_zero[10]={0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
	uint8_t samples_range[10]={0x81, 0x80, 0x81, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x81};
	uint8_t samples_positive[10]={0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81};
	uint8_t samples_negative[10]={0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f};
	uint32_t result;

	fputs("mixAddAbs8M():  ", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs8M(samples_zero, 10))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs8M(samples_range+1, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs8M(samples_positive, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs8M(samples_negative, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}
	fputs("\n", stderr);
}

void test6(void)
{
	int8_t samples_zero[10]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int8_t samples_range[10]={0x01, 0x00, 0x02, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x01};
	int8_t samples_positive[10]={0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
	int8_t samples_negative[10]={-0x01, -0x01, -0x01, -0x01, -0x01, -0x01, -0x01, -0x01, -0x01, -0x01};
	uint32_t result;

	fputs("mixAddAbs8MS(): ", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs8MS(samples_zero, 10))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs8MS(samples_range+1, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs8MS(samples_positive, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs8MS(samples_negative, 10))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}
	fputs("\n", stderr);
}

void test7(void)
{
	uint8_t samples_zero[10]={0x80, 0x70, 0x80, 0x60, 0x80, 0x90, 0x80, 0x33, 0x80, 0xef};
	uint8_t samples_range[10]={0x81, 0x81, 0x80, 0x99, 0x82, 0x82, 0x82, 0x82, 0x82, 0x81};
	uint8_t samples_positive[10]={0x82, 0x23, 0x82, 0x43, 0x82, 0x34, 0x82, 0x33, 0x82, 0x00};
	uint8_t samples_negative[10]={0x7e, 0x2f, 0x7e, 0x3f, 0x7e, 0xaf, 0x7e, 0x9f, 0x7e, 0x8f};
	uint32_t result;

	fputs("mixAddAbs8S():  ", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs8S(samples_zero, 5))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs8S(samples_range+2, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs8S(samples_positive, 5))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs8S(samples_negative, 5))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}
	fputs("\n", stderr);
}

void test8(void)
{
	int8_t samples_zero[10]={0x00, 0x7f, 0x00, 0x6f, 0x00, 0x41, 0x00, 0x1b, 0x00, 0x55};
	int8_t samples_range[10]={0x01, 0x01, 0x00, 0x6f, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01};
	int8_t samples_positive[10]={0x02, 0x76, 0x02, 0x43, 0x02, 0x77, 0x02, 0x03, 0x02, 0x00};
	int8_t samples_negative[10]={-0x02, 0x13, -0x02, -0x58, -0x02, 0x6a, -0x02, -0x20, -0x02, 99};
	uint32_t result;

	fputs("mixAddAbs8SS(): ", stderr);

	fputs("  (zero: ", stderr);
	if ((result=mixAddAbs8SS(samples_zero, 5))!=0)
	{
		fprintf(stderr, "failed, got 0x%08x)", (int)result);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (range: ", stderr);
	if ((result=mixAddAbs8SS(samples_range+2, 1))!=0)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (positive: ", stderr);
	if ((result=mixAddAbs8SS(samples_positive, 5))!=10)
	{
		fputs("failed)", stderr);
		retval=1;
	} else {
		fputs("ok)", stderr);
	}

	fputs("  (negative: ", stderr);
	if ((result=mixAddAbs8SS(samples_negative, 5))!=10)
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

void test9(void)
{
	pad_t pad0;
	int8_t src[10]={-1, -2, -3, 1, 2, 3, -128, 127, 10, 11};
	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
	int16_t wewant1[10]={-1*256, -2*256, -3*256,    1*256,  2*256,  3*256, -128*256, 127*256, 10*256, 11*256};
	int16_t wewant2[10]={-1*256, -3*256,  2*256, -128*256, 10*256,      0,        0,       0,      0,      0};
	int16_t wewant3[10]={-1*256, -1*256, -2*256,   -2*256, -3*256, -3*256,    1*256,   1*256,  2*256,  2*256};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMS8M   (8bit, mono, signed => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMS8M((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMS8M((int16_t *)dst, src, 5, 0x0020000);
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
	mixGetMasterSampleMS8M((int16_t *)dst, src, 10, 0x0008000);
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

void test10(void)
{
	pad_t pad0;
/*
	int8_t src[10]={-1, -2, -3, 1, 2, 3, -128, 127, 10, 11};*/
	uint8_t src[10]={0xff, 0xfe, 0xfd, 1, 2, 3, 128, 127, 10, 11};

	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
/*
	uint16_t wewant1[10]={0x7f00, 0x7e00, 0x7d00, 0x8100, 0x8200, 0x8300, 0x0000, 0xff00, 0x8a00, 0x8b00};
	uint16_t wewant2[10]={0x7f00, 0x7d00, 0x8200, 0x0000, 0x8a00,      0,      0,      0,      0,      0};
	uint16_t wewant3[10]={0x7f00, 0x7f00, 0x7e00, 0x7e00, 0x7d00, 0x7d00, 0x8100, 0x8100, 0x8200, 0x8200};*/
	int16_t wewant1[10]={0x7f00, 0x7e00,  0x7d00, -0x7f00, -0x7e00, -0x7d00, 0x0000, -0x0100, -0x7600, -0x7500};
	int16_t wewant2[10]={0x7f00, 0x7d00, -0x7e00,  0x0000, -0x7600,       0,       0,       0,       0,       0};
	int16_t wewant3[10]={0x7f00, 0x7f00,  0x7e00,  0x7e00,  0x7d00,  0x7d00, -0x7f00, -0x7f00, -0x7e00, -0x7e00};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMU8M   (8bit, mono, unsigned => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMU8M((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);

	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMU8M((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fputs("\n", stderr);
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant2[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleMU8M((int16_t *)dst, src, 10, 0x0008000);
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

void test11(void)
{
	pad_t pad0;
	int8_t src[10]={-1, -2, -3, 1, 2, 3, -128, 127, 10, 11};
	pad_t pad1;
	int16_t dst[20];
	pad_t pad2;
	int16_t wewant1[20]={-1*256,-1*256,  -2*256,-2*256,  -3*256,-3*256,     1*256,   1*256,   2*256, 2*256,   3*256, 3*256,  -128*256,-128*256,  127*256,127*256,  10*256,10*256, 11*256,11*256};
	int16_t wewant2[20]={-1*256,-1*256,  -3*256,-3*256,   2*256, 2*256,  -128*256,-128*256,  10*256,10*256,       0,     0,         0,       0,        0,      0,       0,     0,      0,     0};
	int16_t wewant3[20]={-1*256,-1*256,  -1*256,-1*256,  -2*256,-2*256,    -2*256,  -2*256,  -3*256,-3*256,  -3*256,-3*256,     1*256,   1*256,    1*256,  1*256,   2*256, 2*256,  2*256, 2*256};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMS8S   (8bit, mono, signed => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMS8S((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMS8S((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleMS8S((int16_t *)dst, src, 10, 0x0008000);
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

void test12(void)
{
	pad_t pad0;
/*
	int8_t src[10]={-1, -2, -3, 1, 2, 3, -128, 127, 10, 11};*/
	uint8_t src[10]={0xff, 0xfe, 0xfd, 1, 2, 3, 128, 127, 10, 11};

	pad_t pad1;
	int16_t dst[20];
	pad_t pad2;
/*
	uint16_t wewant1[20]={0x7f00,0x7f00, 0x7e00,0x7e00, 0x7d00,0x7d00, 0x8100,0x8100, 0x8200,0x8200, 0x8300,0x8300, 0x0000,0x0000, 0xff00,0xff00, 0x8a00,0x8a00, 0x8b00,0x8b00};
	uint16_t wewant2[20]={0x7f00,0x7f00, 0x7d00,0x7d00, 0x8200,0x8200, 0x0000,0x0000, 0x8a00,0x8a00,    0,0,   0,0,    0,0,   0,0,    0,0};
	uint16_t wewant3[20]={0x7f00,0x7f00, 0x7f00,0x7f00, 0x7e00,0x7e00, 0x7e00,0x7e00, 0x7d00,0x7d00, 0x7d00,0x7d00, 0x8100,0x8100, 0x8100,0x8100, 0x8200,0x8200, 0x8200,0x8200};*/
	int16_t wewant1[20]={0x7f00,0x7f00, 0x7e00,0x7e00,  0x7d00, 0x7d00, -0x7f00,-0x7f00, -0x7e00,-0x7e00, -0x7d00,-0x7d00, 0x0000,0x0000, -0x0100,-0x0100, -0x7600,-0x7600, -0x7500,-0x7500};
	int16_t wewant2[20]={0x7f00,0x7f00, 0x7d00,0x7d00, -0x7e00,-0x7e00,  0x0000, 0x0000, -0x7600,-0x7600,    0,0,   0,0,    0,0,   0,0,    0,0};
	int16_t wewant3[20]={0x7f00,0x7f00, 0x7f00,0x7f00,  0x7e00, 0x7e00,  0x7e00, 0x7e00,  0x7d00, 0x7d00, 0x7d00,0x7d00, -0x7f00,-0x7f00, -0x7f00,-0x7f00, -0x7e00,-0x7e00, 0-0x7e00,-0x7e00};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMU8S   (8bit, mono, unsigned => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMU8S((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "    %02x   ", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "| %04x %04x", (uint16_t)dst[i*2], (uint16_t)dst[i*2+1]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "| %04x %04x", (uint16_t)wewant1[i*2], (uint16_t)wewant1[i*2+1]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMU8S((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fputs("\n", stderr);
			for (i=0;i<10;i++)
				fprintf(stderr, "    %02x   ", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "| %04x %04x", (uint16_t)dst[i*2], (uint16_t)dst[i*2+1]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "| %04x %04x", (uint16_t)wewant2[i*2], (uint16_t)wewant2[i*2+1]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleMU8S((int16_t *)dst, src, 10, 0x0008000);
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

void test13(void)
{
	pad_t pad0;
	int8_t src[20]={-2,0, 0,-4, -3,-3, 1,1, 2,2, 3,3, -128,-128, 127,127, 10,10, 11,11};
	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
	int16_t wewant1[10]={-1*256, -2*256, -3*256,    1*256,  2*256,  3*256, -128*256, 127*256, 10*256, 11*256};
	int16_t wewant2[10]={-1*256, -3*256,  2*256, -128*256, 10*256,      0,        0,       0,      0,      0};
	int16_t wewant3[10]={-1*256, -1*256, -2*256,   -2*256, -3*256, -3*256,    1*256,   1*256,  2*256,  2*256};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSS8M   (8bit, stereo, signed => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSS8M((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSS8M((int16_t *)dst, src, 5, 0x0020000);
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
	mixGetMasterSampleSS8M((int16_t *)dst, src, 10, 0x0008000);
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

void test14(void)
{
	pad_t pad0;
	int8_t src[20]={-2,0, 0,-4, -3,-3, 1,1, 2,2, 3,3, -128,-128, 127,127, 10,10, 11,11};
	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
	uint16_t wewant1[10]={0x7f00, 0x7e00, 0x7d00,  0x8100, 0x8200, 0x8300, 0x0000, 0xff00, 0x8a00, 0x8b00};
	uint16_t wewant2[10]={0x7f00, 0x7d00, 0x8200,  0x0000, 0x8a00,      0,      0,      0,      0,      0};
	uint16_t wewant3[10]={0x7f00, 0x7f00, 0x7e00,  0x7e00, 0x7d00, 0x7d00, 0x8100, 0x8100, 0x8200, 0x8200};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSU8M   (8bit, stereo, unsigned => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSU8M((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSU8M((int16_t *)dst, src, 5, 0x0020000);
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
	mixGetMasterSampleSU8M((int16_t *)dst, src, 10, 0x0008000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant3, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "  %02x %02x ", (uint8_t)src[i*2], (uint8_t)src[i*2+1]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %04x ", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %04x ", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}
	fputs("\n", stderr);
}

void test15(void)
{
	pad_t pad0;
	int8_t src[10]={-1, -2,   -3, 1,   2, 3,   -128, 127, 10,11};
	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
	int16_t wewant1[10]={-1*256, -2*256, -3*256, 1*256,  2*256,  3*256, -128*256, 127*256, 10*256, 11*256};
	int16_t wewant2[10]={-1*256, -2*256,  2*256, 3*256,      0,      0,        0,       0,      0,      0};
	int16_t wewant3[10]={-1*256, -2*256, -1*256,-2*256, -3*256,  1*256,   -3*256,   1*256,  2*256,  3*256};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSS8S   (8bit, stereo, signed => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSS8S((int16_t *)dst, src, 5, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSS8S((int16_t *)dst, src, 2, 0x0020000);
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
	mixGetMasterSampleSS8S((int16_t *)dst, src, 5, 0x0008000);
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

void test16(void)
{
	pad_t pad0;
	int8_t src[10]={-1, -2,   -3, 1,   2, 3,   -128, 127, 10,11};
	pad_t pad1;
	uint16_t dst[10];
	pad_t pad2;
	uint16_t wewant1[10]={0x7f00, 0x7e00, 0x7d00,0x8100, 0x8200, 0x8300, 0x0000,  0xff00, 0x8a00, 0x8b00};
	uint16_t wewant2[10]={0x7f00, 0x7e00, 0x8200,0x8300,      0,      0,      0,       0,      0,      0};
	uint16_t wewant3[10]={0x7f00, 0x7e00, 0x7f00,0x7e00, 0x7d00, 0x8100, 0x7d00,  0x8100, 0x8200, 0x8300};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSU8S   (8bit, stereo, unsigned => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSU8S((int16_t *)dst, src, 5, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
	retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSU8S((int16_t *)dst, src, 2, 0x0020000);
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
	mixGetMasterSampleSU8S((int16_t *)dst, src, 5, 0x0008000);
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

void test17(void)
{
	pad_t pad0;
	int8_t src[10]={-2, -1, 1,-3,   3,2,   127,-128, 11,10};
	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
	int16_t wewant1[10]={-1*256, -2*256, -3*256, 1*256,  2*256,  3*256, -128*256, 127*256, 10*256, 11*256};
	int16_t wewant2[10]={-1*256, -2*256,  2*256, 3*256,      0,      0,        0,       0,      0,      0};
	int16_t wewant3[10]={-1*256, -2*256, -1*256,-2*256, -3*256,  1*256,   -3*256,   1*256,  2*256,  3*256};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSS8SR  (8bit, stereo, signed => 16bit, rev-stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSS8SR((int16_t *)dst, src, 5, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSS8SR((int16_t *)dst, src, 2, 0x0020000);
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
	mixGetMasterSampleSS8SR((int16_t *)dst, src, 5, 0x0008000);
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

void test18(void)
{
	pad_t pad0;
	int8_t src[10]={-2, -1, 1,-3,   3,2,   127,-128, 11,10};
	pad_t pad1;
	uint16_t dst[10];
	pad_t pad2;
	uint16_t wewant1[10]={0x7f00, 0x7e00, 0x7d00,0x8100, 0x8200, 0x8300, 0x0000,  0xff00, 0x8a00, 0x8b00};
	uint16_t wewant2[10]={0x7f00, 0x7e00, 0x8200,0x8300,      0,      0,      0,       0,      0,      0};
	uint16_t wewant3[10]={0x7f00, 0x7e00, 0x7f00,0x7e00, 0x7d00, 0x8100, 0x7d00,  0x8100, 0x8200, 0x8300};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSU8SR  (8bit, stereo, unsigned => 16bit, rev-stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSU8SR((int16_t *)dst, src, 5, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "   %02x", (uint8_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSU8SR((int16_t *)dst, src, 2, 0x0020000);
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
	mixGetMasterSampleSU8SR((int16_t *)dst, src, 5, 0x0008000);
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

void test19(void)
{
	pad_t pad0;
	int16_t src[10]={-233, -234, -345, 123, 234, 356, -32000, 32000, 10, 11};
	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
	int16_t wewant1[10]={-233, -234, -345, 123, 234, 356, -32000, 32000, 10, 11};
	int16_t wewant2[10]={-233, -345,  234, -32000, 10, 0, 0, 0, 0, 0};
	int16_t wewant3[10]={-233, -233, -234, -234, -345, -345, 123, 123, 234, 234};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMS16M  (16bit, mono, signed => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMS16M((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMS16M((int16_t *)dst, src, 5, 0x0020000);
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
	mixGetMasterSampleMS16M((int16_t *)dst, src, 10, 0x0008000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant3, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant3[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}
	fputs("\n", stderr);
}

void test20(void)
{
	pad_t pad0;
	int16_t src[10]={-1, -2, -3,  1, 2, 3, -0x8000, 0x7fff, 10, 11};
	pad_t pad1;
	uint16_t dst[10];
	pad_t pad2;
	uint16_t wewant1[10]={0x7fff, 0x7ffe, 0x7ffd, 0x8001, 0x8002, 0x8003, 0x0000, 0xffff, 0x800a, 0x800b};
	uint16_t wewant2[10]={0x7fff, 0x7ffd, 0x8002, 0x0000, 0x800a,      0,      0,      0,      0,      0};
	uint16_t wewant3[10]={0x7fff, 0x7fff, 0x7ffe, 0x7ffe, 0x7ffd, 0x7ffd, 0x8001, 0x8001, 0x8002, 0x8002};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMU16M  (16bit, mono, unsigned => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMU16M((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMU16M((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant2[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleMU16M((int16_t *)dst, src, 10, 0x0008000);
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

void test21(void)
{
	pad_t pad0;
	int16_t src[10]={-1, -2, -3,  1, 2, 3, -0x8000, 0x7fff, 10, 11};
	pad_t pad1;
	int16_t dst[20];
	pad_t pad2;
	int16_t wewant1[20]={-1,-1,  -2,-2,  -3,-3,  1,1,  2,2,  3,3,  -0x8000,-0x8000,  0x7fff,0x7fff,  10,10,  11,11};
	int16_t wewant2[20]={-1,-1,  -3,-3,  2,2,  -0x8000,-0x8000,  10,10,  0,0,  0,0,  0,0,  0,0,  0,0};
	int16_t wewant3[20]={-1,-1,  -1,-1,  -2,-2,  -2,-2,  -3,-3,  -3,-3,  1,1,  1,1,  2,2,  2,2};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMS16S  (16bit, mono, signed => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMS16S((int16_t *)dst, src, 10, 0x0010000);
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
				fprintf(stderr, " %04x", (uint16_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMS16S((int16_t *)dst, src, 5, 0x0020000);
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
	mixGetMasterSampleMS16S((int16_t *)dst, src, 10, 0x0008000);
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

void test22(void)
{
	pad_t pad0;
	int16_t src[10]={-1, -2, -3, 1, 2, 3, -0x8000, 0x7fff, 10, 11};
	pad_t pad1;
	int16_t dst[20];
	pad_t pad2;
	uint16_t wewant1[20]={0x7fff,0x7fff, 0x7ffe,0x7ffe, 0x7ffd,0x7ffd, 0x8001,0x8001, 0x8002,0x8002, 0x8003,0x8003, 0x0000,0x0000, 0xffff,0xffff, 0x800a,0x800a, 0x800b,0x800b};
	uint16_t wewant2[20]={0x7fff,0x7fff, 0x7ffd,0x7ffd, 0x8002,0x8002, 0x0000,0x0000, 0x800a,0x800a,      0,     0,      0,     0,      0,     0,      0,     0,      0,     0};
	uint16_t wewant3[20]={0x7fff,0x7fff, 0x7fff,0x7fff, 0x7ffe,0x7ffe, 0x7ffe,0x7ffe, 0x7ffd,0x7ffd, 0x7ffd,0x7ffd, 0x8001,0x8001, 0x8001,0x8001, 0x8002,0x8002, 0x8002,0x8002};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleMU16S  (16bit, mono, unsigned => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleMU16S((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleMU16S((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "    %04x  ", (uint16_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x %04x", (uint16_t)dst[i*2], (uint16_t)dst[i*2+1]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x %04x", (uint16_t)wewant2[i*2], (uint16_t)wewant2[i*2+1]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleMU16S((int16_t *)dst, src, 10, 0x0008000);
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

void test24(void)
{
	pad_t pad0;
/*
	int16_t src[20]={-2,0, 0,-4, -3,-3, 1,1, 2,2, 3,3, -0x8000,-0x8000, 0x7fff,0x7fff, 10,10, 11,11};*/
	uint16_t src[20]={0xfffe,0, 0,0xfffc, 0xffff,0xffff, 1,1, 2,2, 3,3, 0xffff,0x0001, 0x7fff,0x7fff, 0x800a,0x800a, 0x800b,0x800b};

	pad_t pad1;
	int16_t dst[10];
	pad_t pad2;
/*
	uint16_t wewant1[10]={0x7fff, 0x7ffe, 0x7ffd,  0x8001, 0x8002, 0x8003, 0x0000, 0xffff, 0x800a, 0x800b};
	uint16_t wewant2[10]={0x7fff, 0x7ffd, 0x8002,  0x0000, 0x8a00,      0,      0,      0,      0,      0};
	uint16_t wewant3[10]={0x7fff, 0x7fff, 0x7ffe,  0x7ffe, 0x7ffd, 0x7ffd, 0x8001, 0x8001, 0x8002, 0x8002};*/
	int16_t wewant1[10]={-1, -2, 0x7fff,  -0x7fff, -0x7ffe, -0x7ffd, 0x0000, -1, 10, 11};
	int16_t wewant2[10]={-1, 0x7fff, -0x7ffe, -0x0000, 10,      0,      0,      0,      0,      0};
	int16_t wewant3[10]={-1, -1, -2, -2, 0x7fff, 0x7fff, -0x7fff, -0x7fff, -0x7ffe, -0x7ffe};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSU16M  (16bit, stereo, unsigned => 16bit, mono, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSU16M((int16_t *)dst, src, 10, 0x0010000);
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
				fprintf(stderr, "  %04x %04x ", (uint16_t)src[i*2], (uint16_t)src[i*2+1]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "      %04x  ", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, "      %04x  ", (uint16_t)wewant1[i]);
			fprintf(stderr, "\n");
		}

	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSU16M((int16_t *)dst, src, 5, 0x0020000);
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
	mixGetMasterSampleSU16M((int16_t *)dst, src, 10, 0x0008000);
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

void test26(void)
{
	pad_t pad0;
	int16_t src[20]={-1,0, 0,-2, -3,-3,  1,1, 2,2, 3,3, -0x8000,-0x8000, 0x7fff,0x7fff, 10,10, 11,11};
	pad_t pad1;
	uint16_t dst[20];
	pad_t pad2;
	uint16_t wewant1[20]={0x7fff,0x8000, 0x8000,0x7ffe, 0x7ffd,0x7ffd, 0x8001,0x8001, 0x8002,0x8002, 0x8003,0x8003, 0x0000,0x0000, 0xffff,0xffff, 0x800a,0x800a, 0x800b,0x800b};
	uint16_t wewant2[20]={0x7fff,0x8000, 0x7ffd,0x7ffd, 0x8002,0x8002, 0x0000,0x0000, 0x800a,0x800a,      0,     0,      0,     0,      0,     0,      0,     0,      0,     0};
	uint16_t wewant3[20]={0x7fff,0x8000, 0x7fff,0x8000, 0x8000,0x7ffe, 0x8000,0x7ffe, 0x7ffd,0x7ffd, 0x7ffd,0x7ffd, 0x8001,0x8001, 0x8001,0x8001, 0x8002,0x8002, 0x8002,0x8002};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSU16S  (16bit, stereo, unsigned => 16bit, stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSU16S((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSU16S((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant2[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleSU16S((int16_t *)dst, src, 10, 0x0008000);
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

void test27(void)
{
	pad_t pad0;
	int16_t src[20]={0,-1, -4,0, -3,-3, 1,1, 2,2, 3,3, -1280,-1280, 1270,1270, 10,10, 11,11};
	pad_t pad1;
	int16_t dst[20];
	pad_t pad2;
	int16_t wewant1[20]={-1,0,  0,-4, -3,-3,     1,    1,   2, 2,  3, 3, -1280,-1280, 1270,1270, 10,10, 11,11};
	int16_t wewant2[20]={-1,0, -3,-3,  2, 2, -1280,-1280,  10,10,  0, 0,     0,    0,    0,   0,  0, 0,  0, 0};
	int16_t wewant3[20]={-1,0, -1, 0,  0,-4,     0,   -4,  -3,-3, -3,-3,     1,    1,    1,   1,  2, 2,  2, 2};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSS16SR  (16bit, stereo, signed => 16bit, rev-stereo, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSS16SR((int16_t *)dst, src, 10, 0x0010000);
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
	mixGetMasterSampleSS16SR((int16_t *)dst, src, 5, 0x0020000);
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
	mixGetMasterSampleSS16SR((int16_t *)dst, src, 10, 0x0008000);
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

void test28(void)
{
	pad_t pad0;
	int16_t src[20]={0,-1, -2,0, -3,-3,  1,1, 2,2, 3,3, -0x8000,-0x8000, 0x7fff,0x7fff, 10,10, 11,11};
	pad_t pad1;
	uint16_t dst[20];
	pad_t pad2;
	uint16_t wewant1[20]={0x7fff,0x8000, 0x8000,0x7ffe, 0x7ffd,0x7ffd, 0x8001,0x8001, 0x8002,0x8002, 0x8003,0x8003, 0x0000,0x0000, 0xffff,0xffff, 0x800a,0x800a, 0x800b,0x800b};
	uint16_t wewant2[20]={0x7fff,0x8000, 0x7ffd,0x7ffd, 0x8002,0x8002, 0x0000,0x0000, 0x800a,0x800a,      0,     0,      0,     0,      0,     0,      0,     0,      0,     0};
	uint16_t wewant3[20]={0x7fff,0x8000, 0x7fff,0x8000, 0x8000,0x7ffe, 0x8000,0x7ffe, 0x7ffd,0x7ffd, 0x7ffd,0x7ffd, 0x8001,0x8001, 0x8001,0x8001, 0x8002,0x8002, 0x8002,0x8002};

	reset_pads(pad0, pad1, pad2);
	fputs("mixGetMasterSampleSU16SR  (16bit, stereo, unsigned => 16bit, stereo-rev, signed) :\n", stderr);

	fputs("  1x: ", stderr);
	mixGetMasterSampleSU16SR((int16_t *)dst, src, 10, 0x0010000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant1, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
	} else {
		fputs(OK10, stderr);
	}

	fputs("  2x: ", stderr);
	memset(dst, 0, sizeof(dst));
	mixGetMasterSampleSU16SR((int16_t *)dst, src, 5, 0x0020000);
	if (check_pads(pad0, pad1, pad2))
	{
		retval=1;
		fputs("overflow/underflow", stderr);
		reset_pads(pad0, pad1, pad2);
	} else if (memcmp(dst, wewant2, sizeof(dst)))
	{
		retval=1;
		fputs(FAILED10, stderr);
/*
		{
			int i;
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)src[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)dst[i]);
			fprintf(stderr, "\n");
			for (i=0;i<10;i++)
				fprintf(stderr, " %04x", (uint16_t)wewant2[i]);
			fprintf(stderr, "\n");
		}*/
	} else {
		fputs(OK10, stderr);
	}

	fputs("  0.5x: ", stderr);
	mixGetMasterSampleSU16SR((int16_t *)dst, src, 10, 0x0008000);
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
	test1();
	test2();
	test3();
	test4();
	test5();
	test6();
	test7();
	test8();
	memset(masterpad, 0, 128);
	test9();
	test10();
	test11();
	test12();
	test13();
	test14();
	test15();
	test16();
	test17();
	test18();

	test19();
	test20();
	test21();
	test22();
	test23();
	test24();
	test25();
	test26();
	test27();
	test28();

	return retval;
}
