/* OpenCP Module Player
 * copyright (c) 2009-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Unit-test for "dwmixfa.c"
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
#include "dwmixfa.h"
#include "dev/mcp.h"
#include <string.h>
#include <stdlib.h>

static int channelnum;

static int initAsm(void)
{
	return 1;
}

static void calcinterpoltab(void)
{
	int i;
	for (i=0; i<256; i++)
	{
		float x1=i/256.0;
		float x2=x1*x1;
		float x3=x1*x1*x1;
		dwmixfa_state.ct0[i]=-0.5*x3+x2-0.5*x1;
		dwmixfa_state.ct1[i]=1.5*x3-2.5*x2+1;
		dwmixfa_state.ct2[i]=-1.5*x3+2*x2+0.5*x1;
		dwmixfa_state.ct3[i]=0.5*x3-0.5*x2;
	};
}

static void OpenPlayer(int chan)
{
	/*
	uint32_t currentrate;
	uint16_t mixfate;
	*/
	int i;

	/* playsamps=pausesamps=0; not needed, since we don't need to track time */

	/*
	currentrate=mcpMixProcRate/chan;
	mixfate=(currentrate>mcpMixMaxRate)?mcpMixMaxRate:currentrate;
	plrSetOptions(mixfate, mcpMixOpt);

	We don't care about tracking output rate
	*/

	if (!(dwmixfa_state.tempbuf=malloc(sizeof(float)*(MIXF_MIXBUFLEN<<1))))
		exit(1);
	/*
	Don't think I need this
	if (!mixInit(GetMixChannel, 0, chan, amplify))
		return 0;
	*/

	for (i=0; i<chan; i++)
	{
		dwmixfa_state.voiceflags[i]=0;
	}

	dwmixfa_state.samprate=/*plrRate*/44100;
/*
	bufpos=0;
	dopause=0;
	orgspeed=12800;
*/
	channelnum=chan;
/*
	mcpNChan=chan;
	mcpIdle=Idle;

	isstereo=stereo;
	outfmt=(bit16<<1)|(!signedout);*/
	dwmixfa_state.nvoices=channelnum;

	prepare_mixer();

	/*calcspeed();*/
	   /*/  playerproc();*/  /* some timing is wrong here! */
	/* tickwidth=newtickwidth; */
	/* tickplayed=0; */
	/* cmdtimerpos=0; */
/*
	if (!pollInit(timerproc))
	{
		mcpNChan=0;
		mcpIdle=0;
		plrClosePlayer();
		mixClose();
		return 0;
	}
	{
		struct mixfpostprocregstruct *mode;

		for (mode=postprocs; mode; mode=mode->next)
			if (mode->Init) mode->Init(samprate, stereo);
	}
*/
}

static void ClosePlayer()
{
/*
	  struct mixfpostprocregstruct *mode;

	  mcpNChan=0;
	  mcpIdle=0;

	  pollClose();

	  plrClosePlayer();

	  channelnum=0;

	  mixClose();

	  for (mode=postprocs; mode; mode=mode->next)
		  if (mode->Close) mode->Close();
*/
	  free(dwmixfa_state.tempbuf);
}

int main(int argc, char *argv[])
{
	float sample_1[] = {12345.0f, 23451.1234f, 30000.543f, 32767.0f, 1023.09f, -5435.05f, -32768.0f, -16000.02f}; /* normalized around 32767 and -32768 */
	int16_t output[2048];
/* INIT START */
	initAsm();

	/* volramp=!!(dev->opt&MIXF_VOLRAMP); */
	/* declick=!!(dev->opt&MIXF_DECLICK); */

	calcinterpoltab();

	/* amplify=65535; */
	/* relspeed=256; */
	/* relpitch=256; */
	/* interpolation=0; */
	/* mastervol=64; */
	/* masterbal=0; */
	/* masterpan=0; */
	/* mastersrnd=0; */
	/* channelnum=0; */
/* INIT DONE */

/* LOAD */
	/* LoadSamples (sampleinfo_1, 1); */
	/*
		samples=sampleinfo_1; */
	/*
		samplenum=1; */

	prepare_mixer();

	OpenPlayer(1);

	memset(output, 1, sizeof(output));
	dwmixfa_state.outbuf=output+2;
	dwmixfa_state.nsamples=308;//508;

	dwmixfa_state.voiceflags[0] = MIXF_PLAYING|MIXF_LOOPED; /* this is so broken! */

	dwmixfa_state.freqf[0]=0x3a987654; /* pitch */
	dwmixfa_state.freqw[0]=0x00000000; /* pitch */

	dwmixfa_state.fl1[0]=0; /* reset filter */
	dwmixfa_state.fb1[0]=0; /* reset feilter */

	dwmixfa_state.ffreq[0] = 1;      /* filter frequency (0<=x<=1) TODO, needs testing / study */
	dwmixfa_state.freso[0] = 0;      /* filter resonance (0<=x<1)  TODO, needs testing / study*/

	dwmixfa_state.smpposf[0]=0;
	dwmixfa_state.smpposw[0]=sample_1;

	dwmixfa_state.looplen[0]=4;
	dwmixfa_state.loopend[0]=&sample_1[7];

	dwmixfa_state.volleft[0]=0.125f;
	dwmixfa_state.volright[0]=0.125f;
	dwmixfa_state.rampleft[0]=0.0f;
	dwmixfa_state.rampright[0]=0.0f;

	dwmixfa_state.fadeleft=0.5f;
	dwmixfa_state.faderight=-0.5f;

	mixer();

	{
		int i;
		fprintf(stderr, "output: ");
		for (i=0;i<512;i++)
			fprintf(stderr, "[%d %d]", output[i*2], output[i*2+1]);
		fprintf(stderr, "\n");
	}

	fprintf(stderr, "smppos: %u.%u\n", (unsigned int)(dwmixfa_state.smpposw[0]-sample_1), dwmixfa_state.smpposf[0]);

	ClosePlayer();

	return 0;
}
