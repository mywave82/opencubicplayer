/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIFace oscilloscope mode
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
 *  -doj980928  Dirk Jagdmann  <doj@cubic.org>
 *    -added cpipic.h and isqrt.cpp to the #include list
 *  -fd981119   Felix Domke    <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 *  -fd981220   Felix Domke    <tmbinc@gmx.net>
 *    -changes for LFB (and other faked banked-modes)
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include "cpiface.h"
#include "cpiface-private.h"
#include "cpipic.h"
#include "dev/mcp.h"
#include "stuff/poutput.h"

#define MAXDOTS 16384
#define MAXSAMPLEN 1280
#define MAXVIEWCHAN2 16

static uint32_t plOszRate;
static uint8_t plOszTrigger;
static uint8_t plOszMono;
static uint8_t plOszChan;
static int16_t plSampBuf[MAXSAMPLEN];
static int16_t scopes[MAXDOTS];
static int plScopesAmp;
static int plScopesAmp2;

static uint32_t replacbuf[640*2];

static int scopenx,scopeny;
static int scopesx,scopesy,scopetlen;
static int scopedx,scopedy;

static char scaleshift=0;
static int16_t scaledmax;
static int scalemax;
static int16_t scaletab[1024];

static void makescaletab(int amp, int max)
{
	int i;

	for (scaleshift=0; scaleshift<6; scaleshift++)
		if ((amp>>(7-scaleshift))>max)
			break;
	scaledmax=max*80;
	scalemax=512<<scaleshift;
	for (i=-512; i<512; i++)
	{
		int r=(i*amp)>>(16-scaleshift);
		if (r<-max)
			r=-max;
		if (r>max)
			r=max;
		scaletab[512+i]=r*80;
	}
}

static void doscale(int16_t *buf, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		if (*buf<-scalemax)
			*buf=-scaledmax;
		else if (*buf>=scalemax)
			*buf=scaledmax;
		else
			*buf=scaletab[512+(*buf>>scaleshift)];
		buf++;
	}
}

static void plPrepareScopes(void)
{
	if (plOpenCPPict)
	{
		int i;
		for (i=16; i<256; i++)
			gupdatepal(i, plOpenCPPal[i*3], plOpenCPPal[i*3+1], plOpenCPPal[i*3+2]);
		gflushpal();
		memcpy(plVidMem+96*640, plOpenCPPict, (480-96)*640);
	} else {
		memset(plVidMem+96*640, 0, (480-96)*640);
	}

	memset(scopes, 0, MAXDOTS*2);
}

static void plPrepareScopeScr (struct cpifaceSessionAPI_t *cpifaceSession)
{
	char str[49];

	if ((plOszChan==2) && (!cpifaceSession->GetMasterSample))
		plOszChan=3;
	if (((plOszChan==3) || (plOszChan==0)) && !cpifaceSession->GetLChanSample)
		plOszChan=1;
	if ((plOszChan==1) && !cpifaceSession->GetPChanSample)
		plOszChan=2;
	if ((plOszChan==2) && (!cpifaceSession->GetMasterSample))
		plOszChan=3;

	if (plOszChan==0)
	{
		int chann = cpifaceSession->LogicalChannelCount;
		if (chann>(MAXVIEWCHAN2*2))
			chann=(MAXVIEWCHAN2*2);
		scopenx=2;
		scopeny=(chann+scopenx-1)/scopenx;
		scopedx=640/scopenx;
		scopedy=384/scopeny;
		scopesx=512/scopenx;
		scopesy=336/scopeny;
		scopetlen=scopesx/2;
		makescaletab (plScopesAmp * cpifaceSession->PhysicalChannelCount / scopeny, scopesy/2);
	} else if (plOszChan==1)
	{
		scopenx=sqrt( (cpifaceSession->PhysicalChannelCount + 2)/3);
		scopeny = (cpifaceSession->PhysicalChannelCount + scopenx - 1)/scopenx;
		scopedx=640/scopenx;
		scopedy=384/scopeny;
		scopesx=512/scopenx;
		scopesy=336/scopeny;
		scopetlen=scopesx/2;
		makescaletab (plScopesAmp * cpifaceSession->PhysicalChannelCount / scopeny, scopesy/2);
	} else if (plOszChan==2)
	{
		scopenx=1;
		scopeny=plOszMono?1:2;
		scopedx=640;
		scopedy=384/scopeny;
		scopesx=640;
		scopesy=382/scopeny;
		scopetlen=scopesx/2;
		makescaletab(plScopesAmp2/scopeny, scopesy/2);
	} else {
		scopenx=1;
		scopeny=1;
		scopedx=640;
		scopedy=384;
		scopesx=640;
		scopesy=382;
		scopetlen=scopesx;
		makescaletab (plScopesAmp * cpifaceSession->PhysicalChannelCount, scopesy/2);
	}

	strcpy(str, "   scopes: ");
	convnum(plOszRate/scopenx, str+strlen(str), 10, 6, 1);
	strcat(str, " pix/s");
	strcat(str, ", ");
	if (plOszChan==2)
	{
		strcat(str, "master");
		if (plOszMono)
			strcat(str, ", mono");
		else
			strcat(str, ", stereo");
	} else {
		if (plOszChan==0)
			strcat(str, "logical");
		else
			if (plOszChan==1)
				strcat(str, "physical");
			else
				strcat(str, "solo");
		if (plOszTrigger)
			strcat(str, ", triggered");
	}
	gdrawstr(4, 0, 0x09, str, 48);
}

static void plotbuf(uint32_t *buf, int len)
{
	int i;
	uint8_t *b=(uint8_t*)buf;

	for (i=0; i<len; i++, b+=4)
		plVidMem[(buf[i]&0x00ffffff)]=(buf[i])>>24;
}

static void drawscope(int x, int y, const int16_t *in, int16_t *out, int num, uint8_t col, int step)
{
	uint32_t *buf=replacbuf;
	uint32_t scrpos=(y+96)*640+x;
	uint8_t *pic=plOpenCPPict-96*640;
	uint32_t colmask=col<<24;

	int i;
	if (plOpenCPPict)
		for (i=0; i<num; i++)
		{
			*buf++=scrpos+(*out<<3);
			((uint8_t*)buf)[-1]=pic[buf[-1]];
			*buf++=(scrpos+(*in<<3))|colmask;
			*out=*in;
			out+=step;
			in+=step;
			scrpos++;
		} else for (i=0; i<num; i++)
		{
			*buf++=scrpos+(*out<<3);
			*buf++=(scrpos+(*in<<3))|colmask;
			*out=*in;
			out+=step;
			in+=step;
			scrpos++;
		}
	plotbuf(replacbuf, buf-replacbuf);
}

static void removescope(int x, int y, int16_t *out, int num)
{
	uint32_t *buf=replacbuf;
	uint32_t scrpos=(y+96)*640+x;
	uint8_t *pic=plOpenCPPict-96*640;

	int i;
	if (plOpenCPPict)
		for (i=0; i<num; i++)
		{
			*buf++=scrpos+(*out<<3);
			((uint8_t*)buf)[-1]=pic[buf[-1]];
			*out++=0;
			scrpos++;
		} else for (i=0; i<num; i++)
		{
			*buf++=scrpos+(*out<<3);
			*out++=0;
			scrpos++;
		}
	plotbuf(replacbuf, buf-replacbuf);
}

static int plScopesKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('o', "Toggle scope viewer types");
			cpiKeyHelp('O', "Toggle scope viewer types");
			cpiKeyHelp(KEY_PPAGE, "Increase the scope viewer frequency range");
			cpiKeyHelp(KEY_NPAGE, "Decrease the scope viewer frequency range");
			cpiKeyHelp(KEY_HOME, "Reset the scope viewer settings");
			cpiKeyHelp(KEY_TAB, "Toggle scope viewer channel");
			cpiKeyHelp(KEY_SHIFT_TAB, "Toggle scope viewer channel");
			cpiKeyHelp(KEY_ALT_O, "Toggle scope viewer channel");
			cpiKeyHelp(KEY_CTRL_PGUP, "Adjust scale up");
			cpiKeyHelp(KEY_CTRL_PGDN, "Adjust scale down");
			return 0;
		/*case 0x4900: //pgup*/
		case KEY_PPAGE:
			plOszRate=plOszRate*31/32;
			plOszRate=(plOszRate>=512000)?256000:(plOszRate<2048)?2048:plOszRate;
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			plOszRate=plOszRate*32/31;
			plOszRate=(plOszRate>=256000)?256000:(plOszRate<2048)?2048:plOszRate;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
			if (plOszChan==2)
			{
				plScopesAmp2=(plScopesAmp2+1)*32/31;
				plScopesAmp2=(plScopesAmp2>=2000)?2000:(plScopesAmp2<100)?100:plScopesAmp2;
			} else {
				plScopesAmp=(plScopesAmp+1)*32/31;
				plScopesAmp=(plScopesAmp>=1000)?1000:(plScopesAmp<50)?50:plScopesAmp;
			}
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
			if (plOszChan==2)
			{
				plScopesAmp2=plScopesAmp2*31/32;
				plScopesAmp2=(plScopesAmp2>=2000)?2000:(plScopesAmp2<100)?100:plScopesAmp2;
			} else {
				plScopesAmp=plScopesAmp*31/32;
				plScopesAmp=(plScopesAmp>=1000)?1000:(plScopesAmp<50)?50:plScopesAmp;
			}
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plScopesAmp=320;
			plScopesAmp2=640;
			plOszRate=44100;
			break;
		case KEY_TAB: /* tab */
		case KEY_ALT_O:
		case KEY_SHIFT_TAB:
		/* case 0xA500 : // alt-tab TODO KEYS */
			if (plOszChan==2)
			{
				plOszMono=!plOszMono;
				plPrepareScopes();
			} else
				plOszTrigger=!plOszTrigger;
			break;
		case 'o': case 'O':
			plOszChan=(plOszChan+1)%4;
			plPrepareScopes();
			cpifaceSession->SelectedChannelChanged = 1;
			break;
		default:
			return 0;
	}
	plPrepareScopeScr (cpifaceSession);
	return 1;
}

static int plScopesInit(void)
{
	if (plVidType==vidNorm)
		return 0;
	plOszRate=44100;
	plOszTrigger=1;
	plScopesAmp=320;
	plScopesAmp2=640;
	plOszMono=0;
	return 1;
}

static void plDrawScopes (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (plOszChan==0)
	{
		int chann = (cpifaceSession->LogicalChannelCount + 1) / 2;
		int chan0;
		int i;

		if (chann>MAXVIEWCHAN2)
			chann=MAXVIEWCHAN2;
		chan0 = (cpifaceSession->SelectedChannel / 2) - (chann / 2);
		if ((chan0+chann) >= ((cpifaceSession->LogicalChannelCount+1)/2))
			chan0 = ((cpifaceSession->LogicalChannelCount+1)/2)-chann;
		if (chan0<0)
			chan0 = 0;
		chan0*=2;
		chann*=2;

		for (i=0; i<chann; i++)
		{
			int x = cpifaceSession->PanType ? (((i+i+i+chan0)&2)>>1) : (i&1);
			int paus;
			int16_t *bp;
			if ((i+chan0)==cpifaceSession->LogicalChannelCount)
			{
				if (cpifaceSession->SelectedChannelChanged)
			        {
					gdrawchar8p(x?616: 8, 96+scopedy*(i>>1)+scopedy/2-3, ' ', 0, plOpenCPPict?(plOpenCPPict-96*640):0);
					gdrawchar8p(x?624:16, 96+scopedy*(i>>1)+scopedy/2-3, ' ', 0, plOpenCPPict?(plOpenCPPict-96*640):0);
				}
				removescope((scopedx-scopesx)/2+x*scopedx, scopedy*(i/scopenx)+scopedy/2, scopes+((i&~1)|x)*scopesx, scopesx);
				break;
			}
			cpifaceSession->GetLChanSample (cpifaceSession, i+chan0, plSampBuf, scopesx+(plOszTrigger?scopetlen:0), plOszRate/scopenx, 0);
			paus = cpifaceSession->MuteChannel[i];
			if (cpifaceSession->SelectedChannelChanged)
			{
				gdrawchar8p(x?616: 8, 96+scopedy*(i>>1)+scopedy/2-3, '0'+(i+1+chan0)/10, ((i+chan0)==cpifaceSession->SelectedChannel)?15:paus?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
				gdrawchar8p(x?624:16, 96+scopedy*(i>>1)+scopedy/2-3, '0'+(i+1+chan0)%10, ((i+chan0)==cpifaceSession->SelectedChannel)?15:paus?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
			}

			bp=plSampBuf;
			if (plOszTrigger)
			{
				int j;
				for (j=0; j<scopetlen; j++)
					if ((bp[0]>0)&&(bp[1]<=0)&&(bp[2]<=0))
						break;
					else
						bp++;
				if (j==scopetlen)
					bp=plSampBuf;
				else
					bp++;
			}

			doscale(bp, scopesx);

			drawscope((scopedx-scopesx)/2+x*scopedx, scopedy*(i/scopenx)+scopedy/2, bp, scopes+((i&~1)|x)*scopesx, scopesx, paus?8:15, 1);
		}
	} else if (plOszChan==1)
	{
		int i;
		int16_t *bp;

		for (i=0; i < cpifaceSession->PhysicalChannelCount; i++)
		{
			int paus = cpifaceSession->GetPChanSample (cpifaceSession, i, plSampBuf, scopesx+(plOszTrigger?scopetlen:0), plOszRate/scopenx, 0);
			if (paus==3)
			{
				removescope((scopedx-scopesx)/2+(i%scopenx)*scopedx, scopedy*(i/scopenx)+scopedy/2, scopes+i*scopesx, scopesx);
				continue;
			}

			bp=plSampBuf;
			if (plOszTrigger)
			{
				int j;
				for (j=0; j<scopetlen; j++)
					if ((bp[0]>0)&&(bp[1]<=0)&&(bp[2]<=0))
						break;
					else
						bp++;
				if (j==scopetlen)
					bp=plSampBuf;
				else
					bp++;
			}

			doscale(bp, scopesx);

			drawscope((scopedx-scopesx)/2+(i%scopenx)*scopedx, scopedy*(i/scopenx)+scopedy/2, bp, scopes+i*scopesx, scopesx, paus?8:15, 1);
		}
	} else if (plOszChan==2)
	{
		int i;

		cpifaceSession->GetMasterSample(plSampBuf, scopesx, plOszRate/scopenx, plOszMono?mcpGetSampleMono:mcpGetSampleStereo);

		doscale(plSampBuf, scopesx*scopeny);

		for (i=0; i<scopeny; i++)
			drawscope((scopedx-scopesx)/2, scopedy/2+scopedy*i, plSampBuf+i, scopes+i, scopesx, 15, scopeny);
	} else {
		char col;
		int16_t *bp;
		cpifaceSession->GetLChanSample (cpifaceSession, cpifaceSession->SelectedChannel, plSampBuf, scopesx+(plOszTrigger?scopetlen:0), plOszRate/scopenx, 0);
		col = cpifaceSession->MuteChannel[cpifaceSession->SelectedChannel]?7:15;
		bp=plSampBuf;
		if (plOszTrigger)
		{
			int j;
			for (j=0; j<scopetlen; j++)
				if ((bp[0]>0)&&(bp[1]<=0)&&(bp[2]<=0))
					break;
				else
					bp++;
			if (j==scopetlen)
				bp=plSampBuf;
			else
				bp++;
		}

		doscale(bp, scopesx);

		drawscope((scopedx-scopesx)/2, scopedy/2, bp, scopes, scopesx, col, 1);
	}
}

static void scoDraw (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpiDrawGStrings (cpifaceSession);
	plDrawScopes (cpifaceSession);
}

static void scoSetMode (struct cpifaceSessionAPI_t *cpifaceSession)
{
	plReadOpenCPPic();
	cpiSetGraphMode(0);
	plPrepareScopes();
	plPrepareScopeScr (cpifaceSession);
}

static int scoCan (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if ((!cpifaceSession->GetLChanSample) && (!cpifaceSession->GetPChanSample) && (!cpifaceSession->GetMasterSample))
		return 0;
	return 1;
}

static int scoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('o', "Enable scope mode");
			cpiKeyHelp('O', "Enable scope");
			return 0;
		case 'o': case 'O':
			cpiSetMode("scope");
			break;
		default:
			return 0;
	}
	return 1;
}

static int scoEvent (struct cpifaceSessionAPI_t *cpifaceSession, int ev)
{
	switch (ev)
	{
		case cpievInit:
			return scoCan (cpifaceSession);
		case cpievInitAll:
			return plScopesInit();
	}
	return 1;
}

static struct cpimoderegstruct cpiModeScope = {"scope", scoSetMode, scoDraw, scoIProcessKey, plScopesKey, scoEvent CPIMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiRegisterDefMode(&cpiModeScope);
}

static void __attribute__((destructor))done(void)
{
	cpiUnregisterDefMode(&cpiModeScope);
}
