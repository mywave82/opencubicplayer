/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIFace phase graphs mode
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
 *  -ss040916   Stian Skjelstad <stian@nixia.no>
 *    -Optimizebug in GCC 3.3.3 when using -O2 or higher
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "cpiface/cpiface-private.h"
#include "cpiface/cpipic.h"
#include "dev/mcp.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"

#define HIGHLIGHT 11
#define MAXDOTS (64*128   *10)
#define MAXSAMPLEN 1024

static uint32_t plOszRate;
static int plOszMono;
static int plOszChan;
static int16_t plSampBuf[MAXSAMPLEN+2];
static int plScopesAmp;
static int plScopesAmp2;
static int plScopesRatio;

static uint32_t replacebuf[MAXDOTS*2];
static uint32_t sorttemp[MAXDOTS*2];
static uint32_t *replacebufpos;
static uint32_t dotbuf[MAXDOTS];
static uint32_t *dotbufpos;

static int scopenx,scopeny;
static int scopedx,scopedy;
static int scopefx,scopefy;
static int samples;


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

	replacebufpos=replacebuf;
	dotbufpos=dotbuf;
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
		scopenx = sqrt (cpifaceSession->LogicalChannelCount * 2);
		scopeny = (cpifaceSession->LogicalChannelCount + scopenx - 1)/scopenx;
		scopedx = 640/scopenx;
		scopedy = 384/scopeny;
		scopefx = ((int)(plScopesAmp*sqrt(cpifaceSession->LogicalChannelCount << 4)))>>2;
		scopefy = (scopefx*plScopesRatio)>>5;
		samples = 64 * 128 / cpifaceSession->LogicalChannelCount;
		if (samples>1024)
			samples=1024;
	} else if (plOszChan==1)
	{
		scopenx = sqrt (cpifaceSession->PhysicalChannelCount * 2);
		scopeny = (cpifaceSession->PhysicalChannelCount + scopenx - 1)/scopenx;
		scopedx = 640/scopenx;
		scopedy = 384/scopeny;
		scopefx = ((int)(plScopesAmp*sqrt (cpifaceSession->PhysicalChannelCount << 4)))>>2;
		scopefy = (scopefx*plScopesRatio)>>5;
		samples = 64 * 128 / cpifaceSession->PhysicalChannelCount;
		if (samples>1024)
			samples=1024;
	} else if (plOszChan==2)
	{
		scopenx=plOszMono?1:2;
		scopeny=1;
		scopedx=640/scopenx;
		scopedy=384;
		scopefx=plScopesAmp2;
		scopefy=(scopefx*plScopesRatio)>>5;
		samples=1024/scopenx;
	} else {
		scopenx=1;
		scopeny=1;
		scopedx=640;
		scopedy=384;
		scopefx = plScopesAmp * cpifaceSession->LogicalChannelCount;
		scopefy=(scopefx*plScopesRatio)>>5;
		samples=1024;
	}

	strcpy(str, "   phase graphs: ");
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
	}
	gdrawstr(4, 0, 0x09, str, 48);
}

static int plScopesKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('b', "Toggle phase viewer types");
			cpiKeyHelp('B', "Toggle phase viewer types");
			cpiKeyHelp(KEY_PPAGE, "Increase the frequency space for the phase viewer");
			cpiKeyHelp(KEY_NPAGE, "Decrease the frequency space for the phase viewer");
			cpiKeyHelp(KEY_HOME, "Reset the settings for the phase viewer");
			cpiKeyHelp(KEY_TAB, "Toggle phase viewer channel-mode");
			cpiKeyHelp(KEY_SHIFT_TAB, "Toggle phase viewer channel-mode");
			cpiKeyHelp(KEY_ALT_O, "Toggle phase viewer channel-mode");
			cpiKeyHelp(KEY_CTRL_PGUP, "Adjust scale up");
			cpiKeyHelp(KEY_CTRL_PGDN, "Adjust scale down");
			return 0;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
		/*      plOszRate=plOszRate*31/32; */
		/*      plOszRate=(plOszRate>=512000)?256000:(plOszRate<2048)?2048:plOszRate; */
			plScopesRatio=(plScopesRatio+1)*32/31;
			plScopesRatio=(plScopesRatio>=1024)?1024:(plScopesRatio<64)?64:plScopesRatio;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
		/*      plOszRate=plOszRate*32/31; */
		/*      plOszRate=(plOszRate>=256000)?256000:(plOszRate<2048)?2048:plOszRate; */
			plScopesRatio=plScopesRatio*31/32;
			plScopesRatio=(plScopesRatio>=1024)?1024:(plScopesRatio<64)?64:plScopesRatio;
			break;
		/*case 0x4900: //pgup*/
		case KEY_PPAGE:
			if (plOszChan==2)
			{
				plScopesAmp2=plScopesAmp2*32/31;
				plScopesAmp2=(plScopesAmp2>=4096)?4096:(plScopesAmp2<64)?64:plScopesAmp2;
			} else {
				plScopesAmp=plScopesAmp*32/31;
				plScopesAmp=(plScopesAmp>=4096)?4096:(plScopesAmp<64)?64:plScopesAmp;
			}
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			if (plOszChan==2)
			{
				plScopesAmp2=plScopesAmp2*31/32;
				plScopesAmp2=(plScopesAmp2>=4096)?4096:(plScopesAmp2<64)?64:plScopesAmp2;
			} else {
				plScopesAmp=plScopesAmp*31/32;
				plScopesAmp=(plScopesAmp>=4096)?4096:(plScopesAmp<64)?64:plScopesAmp;
			}
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plScopesAmp=512;
			plScopesAmp2=512;
			plScopesRatio=256;
			plOszRate=44100;
			break;
		case KEY_TAB: /* tab */
		case KEY_SHIFT_TAB:
		/* case 0x0F00: // shift-tab */
/* TODO -keys
		case 0xA500: // alt-tab */
		case KEY_ALT_O:
		/*case 0x1800: // alt-o*/
			if (plOszChan==2)
			{
				plOszMono=!plOszMono;
				plPrepareScopes();
			}
			break;
		case 'b': case 'B':
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
	plScopesAmp=512;
	plScopesAmp2=512;
	plScopesRatio=256;
	plOszMono=0;
	return 1;
}

static void drawscope(int x, int y, const int16_t *in, int num, uint8_t col, int step)
{
	uint32_t colmask=col<<24;
	int i;

	y+=96;

	for (i=0; i<num; i++)
	{
		int x1=x+((*in*scopefx)>>16);
		int y1=y+(((in[step]-*in)*scopefy)>>16);
		if ((x1>=0)&&(x1<640)&&(y1>=96)&&(y1<480))
			*dotbufpos++=(y1*640+x1)|colmask;
		in+=step;
	}
}

static void radix(uint32_t *dest, uint32_t *source, long n, int byte)
{
	uint32_t count[256];
	/* Optimizebug in GCC 3.3.3 when using -O2 or higher, not reported yet
	 *
	 * For now I'll always let the workaround stay, since we are not that
	 * tight on stack, and streaming/unrolling are nice to have, and pointers
	 * are not always long big
	 * */
/*#if ( __GNUC__ == 3 ) && ( __GNUC_MINOR__ == 3 ) && (__GNUC_PATCHLEVEL__ >= 3) && (__GNUC_PATCHLEVEL__ <= 5)*/
	uint32_t *index[256];
/*#else
	unsigned long **index=(unsigned long**)count;
#endif*/
	int i;

	uint32_t *dp;

	memset (count, 0, sizeof (count));
	for (i=0; i<n; i++)
		count[((uint8_t*)&source[i])[byte]]++;

	dp=dest;
	if (byte==3)
	{
		uint32_t *ndp;
		for (i=48; i<256; i++)
		{
			ndp=dp+count[i];
			index[i]=dp;
			dp=ndp;
		}
		for (i=0; i<48; i++)
			if (i!=HIGHLIGHT)
			{
				ndp=dp+count[i];
				index[i]=dp;
				dp=ndp;
			}
		ndp=dp+count[HIGHLIGHT];
		index[HIGHLIGHT]=dp;
		dp=ndp;
	} else
		for (i=0; i<256; i++)
		{
			uint32_t *ndp=dp+count[i];
			index[i]=dp;
			dp=ndp;
		}
	for (i=0; i<n; i++)
		*index[((uint8_t*)&source[i])[byte]]++=source[i];
}



static void drawframe(void)
{
	int n;

	uint32_t *bp;
	uint32_t *b2;

	memcpy(replacebufpos,dotbuf,(dotbufpos-dotbuf)*4);
	replacebufpos+=dotbufpos-dotbuf;

	n=replacebufpos-replacebuf;
#ifndef WORDS_BIGENDIAN
	radix(sorttemp, replacebuf, n, 3);
	radix(replacebuf, sorttemp, n, 0);
	radix(sorttemp, replacebuf, n, 1);
	radix(replacebuf, sorttemp, n, 2);
#else
	radix(sorttemp, replacebuf, n, 0);
	radix(replacebuf, sorttemp, n, 3);
	radix(sorttemp, replacebuf, n, 2);
	radix(replacebuf, sorttemp, n, 1);
#endif
	for (b2 = replacebuf; b2<replacebufpos; b2++)
	{
		plVidMem[((*b2)&0xffffff)]=(*b2)>>24;
	}

	memcpy(replacebuf,dotbuf,(dotbufpos-dotbuf)*4);
	replacebufpos=replacebuf+(dotbufpos-dotbuf);
	if (plOpenCPPict)
		for (bp=replacebuf; bp<replacebufpos; bp++)
		{
			*bp&=0x00ffffff;
			*bp|=plOpenCPPict[(*bp&0xFFFFFF)-96*640]<<24;
		} else
			for (bp=replacebuf; bp<replacebufpos; bp++)
				*bp&=0x00ffffff;

	dotbufpos=dotbuf;
}

static void plDrawScopes (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;
	if (plOszChan==2)
	{
		cpifaceSession->GetMasterSample(plSampBuf, samples+1, plOszRate, (plOszMono?mcpGetSampleMono:mcpGetSampleStereo)|mcpGetSampleHQ);
		for (i=0; i<scopenx; i++)
			drawscope(scopedx/2+scopedx*i, scopedy/2, plSampBuf+i, samples, 15, scopenx);
	} else if (plOszChan==1)
	{
		int i;
		for (i=0; i < cpifaceSession->PhysicalChannelCount; i++)
		{
			int paus = cpifaceSession->GetPChanSample (cpifaceSession, i, plSampBuf, samples+1, plOszRate, mcpGetSampleHQ);
			drawscope((i%scopenx)*scopedx+scopedx/2, scopedy*(i/scopenx)+scopedy/2, plSampBuf, samples, paus?8:15, 1);
		}
	} else if (plOszChan==3)
	{
		cpifaceSession->GetLChanSample (cpifaceSession, cpifaceSession->SelectedChannel, plSampBuf, samples+1, plOszRate, mcpGetSampleHQ);
		drawscope(scopedx/2, scopedy/2, plSampBuf, samples, cpifaceSession->MuteChannel[cpifaceSession->SelectedChannel]?7:15, 1);
	} else if (plOszChan==0)
	{
		int i;
		for (i=0; i < cpifaceSession->LogicalChannelCount; i++)
		{
			cpifaceSession->GetLChanSample (cpifaceSession, i, plSampBuf, samples+1, plOszRate, mcpGetSampleHQ);
			drawscope((i%scopenx)*scopedx+scopedx/2, scopedy*(i/scopenx)+scopedy/2, plSampBuf, samples, (cpifaceSession->SelectedChannel==i)?cpifaceSession->MuteChannel[i]?(HIGHLIGHT&7):HIGHLIGHT:cpifaceSession->MuteChannel[i]?8:15, 1);
		}
	}
	drawframe();
}

static void scoDraw (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpiDrawGStrings (cpifaceSession);
	plDrawScopes (cpifaceSession);
}

static void scoSetMode (struct cpifaceSessionAPI_t *cpifaceSession)
{
	plReadOpenCPPic (cpifaceSession->configAPI, cpifaceSession->dirdb);
	cpiSetGraphMode(0);
	plPrepareScopes();
	plPrepareScopeScr (cpifaceSession);
}

static int scoCan (struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (cpifaceSession->console->VidType == vidNorm)
	{
		return 0;
	}
	if (!cpifaceSession->GetLChanSample && !cpifaceSession->GetPChanSample && (!cpifaceSession->GetMasterSample))
	{
		return 0;
	}
	return 1;
}

static int scoIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('b', "Enable phase mode");
			cpiKeyHelp('B', "Enable phase mode");
			return 0;
		case 'b': case 'B':
			cpiSetMode ("phase");
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

static struct cpimoderegstruct cpiModePhase = {"phase", scoSetMode, scoDraw, scoIProcessKey, plScopesKey, scoEvent CPIMODEREGSTRUCT_TAIL};

OCP_INTERNAL void cpiPhaseInit (void)
{
	cpiRegisterDefMode(&cpiModePhase);
}

OCP_INTERNAL void cpiPhaseDone (void)
{
	cpiUnregisterDefMode(&cpiModePhase);
}
