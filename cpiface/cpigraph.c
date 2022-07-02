/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface graphic spectrum analysers
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
 *  -doj980928  Dirk Jagdmann <doj@cubic.org>
 *    -changed code with fftanalyse to meet dependencies from changes in fft.cpp
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 *  -fd981220   Felix Domke    <tmbinc@gmx.net>
 *    -changes for LFB (and other faked banked-modes)
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "stuff/poutput.h"
#include "cpiface.h"
#include "cpiface-private.h"
#include "cpipic.h"
#include "dev/mcp.h"
#include "fft.h"

static unsigned char plStripePal1;
static unsigned char plStripePal2;

static uint32_t plAnalRate;
static uint16_t plAnalScale;

static int plStripeSpeed;
static int plAnalChan;
static int plStripePos;
static int plStripeBig;

static int16_t plSampBuf[2048];
static uint16_t ana[1024];

static void plSetStripePals(int a, int b)
{
	short i;
	int index=64;

	plStripePal1=(a+8)%8;
	plStripePal2=(b+4)%4;

	switch(plStripePal2)
	{
		case 0:
			for (i=0; i<32; i++)
				gupdatepal(index++, 2*i, 63, 0);
			for (i=0; i<32; i++)
				gupdatepal(index++, 63, 63-2*i, 0);
			break;
		case 1:
			for (i=0; i<32; i++)
				gupdatepal(index++, 0, 63, 2*i);
			for (i=0; i<32; i++)
				gupdatepal(index++, 0, 63-2*i, 63);
			break;
		case 2:
			for (i=0; i<64; i++)
				gupdatepal(index++, 63-i/2, 63-i/2, 63-i/2);
			break;
		case 3:
			for (i=0; i<60; i++)
				gupdatepal(index++, 63-i/2, 63-i/2, 63-i/2);
			for (i=0; i<4; i++)
				gupdatepal(index++, 63,0, 0);
			break;
	}

	switch(plStripePal1)
	{
		case 0:
			for (i=0; i<32; i++)
				gupdatepal(index++, 0, 0, i);
			for (i=0; i<64; i++)
				gupdatepal(index++, i, 0, 31-i/2);
			for (i=0; i<32; i++)
				gupdatepal(index++, 63, 2*i, 0);
			break;
		case 1:
			for (i=0; i<32; i++)
				gupdatepal(index++, 0, 0, i);
			for (i=0; i<80; i++)
				gupdatepal(index++, 4*i/5, 0, 31-2*i/5);
			for (i=0; i<16; i++)
				gupdatepal(index++, 63, i*4, 0);
			break;
		case 2:
			for (i=0; i<64; i++)
				gupdatepal(index++, 0, 0, i/2);
			for (i=0; i<48; i++)
				gupdatepal(index++, 4*i/3, 0, 31-2*i/3);
			for (i=0; i<16; i++)
				gupdatepal(index++, 63, i*4, 0);
			break;
		case 3:
			for (i=0; i<32; i++)
				gupdatepal(index++, 0, 0, i);
			for (i=0; i<64; i++)
				gupdatepal(index++, 0, i, 31-i/2);
			for (i=0; i<32; i++)
				gupdatepal(index++, 2*i, 63, 2*i);
			break;
		case 4:
			for (i=0; i<128; i++)
				gupdatepal(index++, i/2, i/2, i/2);
			break;
		case 5:
			for (i=0; i<120; i++)
				gupdatepal(index++, i/2, i/2, i/2);
			for (i=0; i<8; i++)
				gupdatepal(index++, 63, 0, 0);
			break;
		case 6:
			for (i=0; i<128; i++)
				gupdatepal(index++, 63-i/2, 63-i/2, 63-i/2);
			break;
		case 7:
			for (i=0; i<120; i++)
				gupdatepal(index++, 63-i/2, 63-i/2, 63-i/2);
			for (i=0; i<8; i++)
				gupdatepal(index++, 63, 0, 0);
			break;
	}
	gflushpal();
}

static void plPrepareStripes(void)
{
	plStripePos=0;

	plSetStripePals(plStripePal1, plStripePal2);

	if (plStripeBig)
	{
		int i;

		memset(plVidMem+32*1024, 128, 32*1024);
		memset(plVidMem + 128 * 1024, 128, 512 * 1024 );
		gdrawstr(42, 1, 0x09, "scale: ", 7);
		for (i=0; i<256; i++)
		{
			int j;
			for (j=0; j<16; j++)
				plVidMem[672*1024 +j*plScrLineBytes+64+i]=(i>>1)+128;
		}
		for (i=0; i<128; i++)
		{
			int j;
			for (j=0; j<16; j++)
				plVidMem[672*1024+j*plScrLineBytes+352+i]=(i>>1)+64;

		}
	} else {
		int i;
		memset(plVidMem+96*640, 128, 170 * 1024);
		gdrawstr(24, 1, 0x09, "scale: ", 7);
		for (i=0; i<128; i++)
		{
			int j;
			for (j=0; j<16; j++)
				plVidMem[384*640+64+i+j*640]=i+128;
		}
		for (i=0; i<64; i++)
		{
			int j;
			for (j=0; j<16; j++)
				plVidMem[384*640+232+i+j*640]=i+64;
		}
	}
}

static void plPrepareStripeScr (struct cpifaceSessionAPI_t *cpifaceSession)
{
	char str[49];

	if ((plAnalChan==2)&&!plGetLChanSample)
		plAnalChan=0;
	if (((plAnalChan==0)||(plAnalChan==1)) && (!cpifaceSession->GetMasterSample))
		plAnalChan=2;
	if ((plAnalChan==2)&&!plGetLChanSample)
		plAnalChan=0;

	strcpy(str, "   ");
	if (plStripeBig)
		strcat(str, "big ");
	strcat(str, "graphic spectrum analyser");
	gdrawstr(4, 0, 0x09, str, 48);

	strcpy(str, "max: ");
	convnum(plAnalRate>>1, str+strlen(str), 10, 5, 1);
	strcat(str, "Hz  (");
	strcat(str, plStripeSpeed?"fast, ":"fine, ");
	strcat(str, (plAnalChan==0)?"both":(plAnalChan==1)?"mid":"chan");
	strcat(str, ")");

	if (plStripeBig)
		gdrawstr(42, 96, 0x09, str, 32);
	else
		gdrawstr(24, 48, 0x09, str, 32);
}

static void reduceana(unsigned short *a, short len)
{
	int max=(1<<22)/plAnalScale;
	int i;
	for (i=0; i<len; i++)
	if (*a>=max)
		*a++=255;
	else {
		*a=((*a*plAnalScale)>>15)+128;
		a++;
	}
}

#if 0
static char *vmx;

void drawgbar(unsigned long x, unsigned char h);
#pragma aux drawgbar parm [edi] [ecx] modify [eax] = \
  "mov eax,plVidMem" \
  "mov vmx,eax" \
  "add vmx,0x1000" \
  "add edi,eax" \
  "add edi,0xAD80" \
  "mov ax,0x4040" \
  "test ecx,ecx" \
  "jmp lp1e" \
"lp1:" \
    "mov word ptr [edi],ax" \
    "add ax,0x0101" \
    "sub edi,640" \
  "dec ecx" \
"lp1e:" \
  "jnz lp1" \
  \
  "jmp lp2e" \
"lp2:" \
    "mov word ptr [edi],0" \
    "sub edi,640" \
"lp2e:" \
  "cmp edi, vmx" \
  "jnb lp2"

#endif

void drawgbar(unsigned long x, unsigned char h)
{
	uint8_t *vmx, *pos;
	short value;

	vmx=plVidMem+(479-0x40)*plScrLineBytes;
	pos=plVidMem+x+479*plScrLineBytes;
	value=0x4040;

	while(h)
	{
		((short *)pos)[0]=value;
		value+=0x0101;
		pos-=plScrLineBytes; /* 640 */
		h--;
	}

	while (pos>vmx)
	{
		((short *)pos)[0]=0;
		pos-=plScrLineBytes; /* 640 */
	}
}

#if 0
void drawgbarb(unsigned long x, unsigned char h);
#pragma aux drawgbarb parm [edi] [ecx] modify [eax] = \
  "add edi,plVidMem" \
  "add edi,0xFC00" \
  "mov al,0x40" \
  "test ecx,ecx" \
  "jmp lp1e" \
"lp1:" \
    "mov byte ptr [edi],al" \
    "inc al" \
    "sub edi,1024" \
  "dec ecx" \
"lp1e:" \
  "jnz lp1" \
  \
  "jmp lp2e" \
"lp2:" \
    "mov byte ptr [edi],0" \
    "sub edi,1024" \
"lp2e:" \
  "cmp edi, plVidMem" \
  "jnb lp2"
#endif

void drawgbarb(unsigned long x, unsigned char h)
{
	uint8_t *vmx, *pos;
	uint8_t value=0x40;

	vmx=plVidMem+(768-0x40)*plScrLineBytes;
	pos=plVidMem+x+767*plScrLineBytes;

	while(h)
	{
		*pos=value;
		value++;
		pos-=plScrLineBytes; /* 1024 */
		h--;
	}

	while (pos>vmx)
	{
		*pos=0;
		pos-=plScrLineBytes; /* 1024 */
	}

}

static void plDrawStripes (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i, j;
	unsigned char *sp;
	static unsigned char linebuf[1088];

	if (plPause)
		return;
	if (plStripeBig)
	{
		memset(linebuf, 128, 1088);
		if (!plAnalChan)
		{
			cpifaceSession->GetMasterSample(plSampBuf, 1024>>plStripeSpeed, plAnalRate, mcpGetSampleStereo);

			if (plStripeSpeed)
			{
				fftanalyseall(ana, plSampBuf, 2, 9);
				reduceana(ana, 256);
				sp=linebuf+511;
				for (i=0; i<256; i++)
				{
					*sp=ana[i];
					sp--;
					*sp=ana[i];
					sp--;
				}

				fftanalyseall(ana, plSampBuf+1, 2, 9);
				reduceana(ana, 256);
				sp=linebuf+1087;
				for (i=0; i<256; i++)
				{
					*sp=ana[i];
					sp--;
					*sp=ana[i];
					sp--;
				}
			} else {
				fftanalyseall(ana, plSampBuf, 2, 10);
				reduceana(ana, 512);
				sp=linebuf+511;
				for (i=0; i<512; i++)
					*sp--=ana[i];
				fftanalyseall(ana, plSampBuf+1, 2, 10);
				reduceana(ana, 512);
				sp=linebuf+1087;
				for (i=0; i<512; i++)
					*sp--=ana[i];
			}
		} else {
			if (plAnalChan!=2)
				cpifaceSession->GetMasterSample(plSampBuf, 2048>>plStripeSpeed, plAnalRate, 0);
			else
				plGetLChanSample (cpifaceSession->SelectedChannel, plSampBuf, 2048>>plStripeSpeed, plAnalRate, 0);
			if (plStripeSpeed)
			{
				fftanalyseall(ana, plSampBuf, 1, 10);
				reduceana(ana, 512);
				sp=linebuf+1055;
				for (i=0; i<512; i++)
				{
					*sp=ana[i];
					sp--;
					*sp=ana[i];
					sp--;
				}
			} else {
				fftanalyseall(ana, plSampBuf, 1, 11);
				reduceana(ana, 1024);
				sp=linebuf+1055;
				for (i=0; i<1024; i++)
					*sp--=ana[i];
			}
		}

		sp=plVidMem+96*plScrLineBytes;
		for (i=0; i<544; i++, sp+=plScrLineBytes)
		{
			sp[plStripePos]=(linebuf[2*i]+linebuf[2*i+1])>>1;
			for (j=1;j<32;j++)
			{
				int p = (plStripePos+j)%1024;
				unsigned char b = sp[p];
				if (b<133)
					b=128;
				else
					b-=4;
				sp[p] = b;
			}
		}
		if (!plAnalChan)
		{
			for (i=0; i<504; i++)
				drawgbarb(i, (linebuf[511-i]-128)>>1);
			for (i=0; i<16; i++)
				drawgbarb(512-8+i, 0);
			for (i=0; i<504; i++)
				drawgbarb(512+8+i, (linebuf[1087-i]-128)>>1);
		} else
			for (i=0; i<1024; i++)
				drawgbarb(i, (linebuf[1055-i]-128)>>1);
		plStripePos=(plStripePos+1)%1024;
	} else {
		memset(linebuf, 128, 272);
		if (!plAnalChan)
		{
			cpifaceSession->GetMasterSample(plSampBuf, 256>>plStripeSpeed, plAnalRate, mcpGetSampleStereo);
			if (plStripeSpeed)
			{
				fftanalyseall(ana, plSampBuf, 2, 7);
				reduceana(ana, 64);
				sp=linebuf+127;
				for (i=0; i<64; i++)
				{
					*sp=ana[i];
					sp--;
					*sp=ana[i];
					sp--;
				}
				fftanalyseall(ana, plSampBuf+1, 2, 7);
				reduceana(ana, 64);
				sp=linebuf+271;
				for (i=0; i<64; i++)
				{
					*sp=ana[i];
					sp--;
					*sp=ana[i];
					sp--;
				}
			} else {
				fftanalyseall(ana, plSampBuf, 2, 8);
				reduceana(ana, 128);
				sp=linebuf+127;
				for (i=0; i<128; i++)
					*sp--=ana[i];
				fftanalyseall(ana, plSampBuf+1, 2, 8);
				reduceana(ana, 128);
				sp=linebuf+271;
				for (i=0; i<128; i++)
					*sp--=ana[i];
			}
		} else {
			if (plAnalChan!=2)
				cpifaceSession->GetMasterSample(plSampBuf, 512>>plStripeSpeed, plAnalRate, 0);
			else
				plGetLChanSample (cpifaceSession->SelectedChannel, plSampBuf, 512>>plStripeSpeed, plAnalRate, 0);
			if (plStripeSpeed)
			{
				fftanalyseall(ana, plSampBuf, 1, 8);
				reduceana(ana, 128);
				sp=linebuf+263;
				for (i=0; i<128; i++)
				{
					*sp=ana[i];
					sp--;
					*sp=ana[i];
					sp--;
				}
			} else {
				fftanalyseall(ana, plSampBuf, 1, 9);
				reduceana(ana, 256);
				sp=linebuf+263;
				for (i=0; i<256; i++)
					*sp--=ana[i];
			}
		}

		sp=plVidMem+96*plScrLineBytes;
		for (i=0; i<272; i++, sp+=plScrLineBytes)
		{
			sp[plStripePos]=linebuf[i];
			for (j=1;j<32;j++)
			{
				int p = (plStripePos+j)%640;
				unsigned char b = sp[p];
				if (b<133)
					b=128;
				else
					b-=4;
				sp[p] = b;
			}
		}

		if (!plAnalChan)
		{
			for (i=0; i<128; i++)
				drawgbar(48+2*i, (linebuf[127-i]-128)>>1);
			for (i=0; i<16; i++)
				drawgbar(48+256+2*i, 0);
			for (i=0; i<128; i++)
				drawgbar(48+288+2*i, (linebuf[271-i]-128)>>1);
		} else
			for (i=0; i<272; i++)
				drawgbar(48+2*i, (linebuf[271-i]-128)>>1);
					plStripePos=(plStripePos+1)%plScrLineBytes;
	}
}

static void strSetMode (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpiSetGraphMode(plStripeBig);
	plPrepareStripes();
	plPrepareStripeScr (cpifaceSession);
}

static int plStripeKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(KEY_PPAGE, "Reduce frequency space for graphical spectrum analyzer");
			cpiKeyHelp(KEY_PPAGE, "Increase frequency space for graphical spectrum analyzer");
			cpiKeyHelp(KEY_CTRL_PGUP, "Adjust scale down");
			cpiKeyHelp(KEY_CTRL_PGDN, "Adjust scale up");
			cpiKeyHelp(KEY_HOME, "Reset settings for graphical spectrum analyzer");
			cpiKeyHelp(KEY_TAB, "Cycle palette for graphical spectrum analyzer");
			cpiKeyHelp(KEY_SHIFT_TAB, "Cycle palette for mini graphical spectrum analyzer");
			cpiKeyHelp(KEY_ALT_G, "Toggle stripe speed");
			cpiKeyHelp('g', "Toggle which channel to analyze");
			cpiKeyHelp('G', "Toggle resolution");
			return 0;
		case KEY_PPAGE:
			plAnalRate=plAnalRate*30/32;
			plAnalRate=(plAnalRate>=64000)?64000:(plAnalRate<1024)?1024:plAnalRate;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
			plAnalScale=(plAnalScale+1)*32/31;
			plAnalScale=(plAnalScale>=4096)?4096:(plAnalScale<256)?256:plAnalScale;
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			plAnalRate=plAnalRate*32/30;
			plAnalRate=(plAnalRate>=64000)?64000:(plAnalRate<1024)?1024:plAnalRate;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
			plAnalScale=plAnalScale*31/32;
			plAnalScale=(plAnalScale>=4096)?4096:(plAnalScale<256)?256:plAnalScale;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plAnalRate=5512;
			plAnalScale=2048;
			plAnalChan=0;
			break;
		case KEY_TAB:
			plSetStripePals(plStripePal1+1, plStripePal2);
			break;
/* TODO-keys
		case 0xA500: // alt-tab
			plSetStripePals(plStripePal1-1, plStripePal2);
			break; */
		case KEY_SHIFT_TAB:
			plSetStripePals(plStripePal1, plStripePal2+1);
			break;
		case KEY_ALT_G: /*0x2200*/
			plStripeSpeed=!plStripeSpeed;
			break;
		case 'g':
			plAnalChan=(plAnalChan+1)%3;
			break;
		case 'G':
			plStripeBig=!plStripeBig;
			strSetMode (cpifaceSession);
			break;
		default:
			return 0;
	}
	plPrepareStripeScr (cpifaceSession);
	return 1;
}

static int plStripeInit(void)
{
	if (plVidType==vidNorm)
		return 0;
	plAnalRate=5512;
	plAnalScale=2048;
	plAnalChan=0;
	plStripeSpeed=0;
	return 1;
}

static void strDraw (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpiDrawGStrings (cpifaceSession);
	plDrawStripes (cpifaceSession);
}

static int strCan (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if ((!plGetLChanSample) && (!cpifaceSession->GetMasterSample))
		return 0;
	return 1;
}

static int strIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('g', "Enable graphical analyzer in low-res");
			cpiKeyHelp('G', "Enable graphical analyzer in high-res");
			return 0;
		case 'g': case 'G':
			plStripeBig=(key=='G');
			cpiSetMode("graph");
			break;
		default:
		return 0;
	}
	return 1;
}

static int strEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInit:
			return strCan (cpifaceSession);
		case cpievInitAll:
			return plStripeInit();
	}
	return 1;
}

static struct cpimoderegstruct cpiModeGraph = {"graph", strSetMode, strDraw, strIProcessKey, plStripeKey, strEvent CPIMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiRegisterDefMode(&cpiModeGraph);
}

static void __attribute__((destructor))done(void)
{
	cpiUnregisterDefMode(&cpiModeGraph);
}
