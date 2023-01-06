/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay - Module player for XM/MOD and affiliate formats
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 *    -MOD: added module flag "ismod" to handle some protracker "features"
 *          (YEZ! Finally this player is chiptune capable!)
 *    -fixed envelope handling (sustain point was ignored when loopstart,
 *     loopend and sustain point were the same)
 *    -added plenty of effect status variables for screen output
 *    -fixed "always loop the last pattern" bug
 *    -MOD: fixed "offset greater than samplelength" bug
 *    -MOD: rewrote PlayNote() to achieve perfect PQE
 *          (Protracker Quirk Emulation ;)
 *    -MOD: enabled tick0 effects while pattern delay
 *    -MOD: added second "set speed" command for vblank timed modules
 *    -added "set finetune" command (E5x) (thanks to jt_letgo.xm ;)
 *    -added panpos array to xmodule for MOD/MXM channel panning
 *    -made vibratos weaker (yes, i didnt recognize that sooner, just
 *     blame me)
 *    -fixed playnote() again a bit
 *  -kb981210   Tammo Hinrichs <opencp@gmx.net>
 *    -set max channels to 256 to play modplug 64chn XMs and such
 *    -again many fixes in playnote() (  when...  WHEN...  )
 *  -kb990401   Tammo Hinrichs <opencp@gmx.net>
 *    -Note Retrig fixed
 *  -ryg990426  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -^^^ put this fix into cvs because kb was too lazy and i was stupid
 *     enuff to say i would do the job :)
 *  -doj20020901 Dirk Jagdmann <doj@cubic.org>
 *    -enable/disable pattern looping
 */

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "xmplay.h"
#include "stuff/err.h"

struct channel
{
	int chVol;
	int chFinalVol;
	int chPan;
	int chFinalPan;
	int32_t chPitch;
	int32_t chFinalPitch;
	int curnote;

	uint8_t chCurIns;
	uint8_t chLastIns;
	int chCurNormNote;
	uint8_t chSustain;
	uint16_t chFadeVol;
	uint16_t chAVibPos;
	uint32_t chAVibSwpPos;
	uint32_t chVolEnvPos;
	uint32_t chPanEnvPos;

	uint8_t chDefVol;
	int chDefPan;
	uint8_t chCommand;
	uint8_t chVCommand;
	int32_t chPortaToPitch;
	int32_t chPortaToVal;
	uint8_t chVolSlideVal;
	uint8_t chGVolSlideVal;
	uint8_t chVVolPanSlideVal;
	uint8_t chPanSlideVal;
	uint8_t chFineVolSlideUVal;
	uint8_t chFineVolSlideDVal;
	int32_t chPortaUVal;
	int32_t chPortaDVal;
	uint8_t chFinePortaUVal;
	uint8_t chFinePortaDVal;
	uint8_t chXFinePortaUVal;
	uint8_t chXFinePortaDVal;
	uint8_t chVibRate;
	uint8_t chVibPos;
	uint8_t chVibType;
	uint8_t chVibDep;
	uint8_t chTremRate;
	uint8_t chTremPos;
	uint8_t chTremType;
	uint8_t chTremDep;
	uint8_t chPatLoopCount;
	uint8_t chPatLoopStart;
	uint8_t chArpPos;
	uint8_t chArpNotes[3];
	uint8_t chActionTick;
	uint8_t chMRetrigPos;
	uint8_t chMRetrigLen;
	uint8_t chMRetrigAct;
	uint8_t chDelayNote;
	uint8_t chDelayIns;
	uint8_t chDelayVol;
	uint8_t chOffset;
	uint8_t chGlissando;
	uint8_t chTremorPos;
	uint8_t chTremorLen;
	uint8_t chTremorOff;
	uint8_t chSync;
	int chSyncTime;
	int delayfreq;

	unsigned int nextstop;
	unsigned int nextsamp;
	unsigned int nextpos;
	struct xmpsample *cursamp;

	int evpos0;
	int evmodtype;
	int evmod;
	int evmodpos;
	int evpos;
	int evtime;

	int notehit;
	uint8_t volslide;
	uint8_t pitchslide;
	uint8_t panslide;
	uint8_t volfx;
	uint8_t pitchfx;
	uint8_t notefx;
	uint8_t fx;
};

static int looping;
static int looped;
static int usersetpos;
static struct channel channels[256];

static uint8_t mutech[256];
static uint8_t globalvol;
static uint8_t globalfx;

static uint8_t curtick;
static uint8_t curtempo;
static uint8_t tick0;

static int currow;
static uint8_t (*patptr)[5];
static int patlen;
static int curord;

static int nord;
static int ninst;
static int nsamp;
static int linearfreq;
static int nchan;
static int loopord;
static int nenv;
static char ismod;
static char ft2_e60bug;
static struct xmpinstrument *instruments;
static struct xmpsample *samples;
static struct sampleinfo *sampleinfos;
static struct xmpenvelope *envelopes;
static uint8_t (**patterns)[5];
static uint16_t *orders;
static uint16_t *patlens;

static int jumptoord;
static int jumptorow;
static int nextpatternrow; /* which row to go do, when doing roll-over at the end of pattern - normally row 0, except for Fast Tracker II E60 bug */
static int patdelay;

static uint8_t procnot;
static uint8_t procins;
static uint8_t procvol;
static uint8_t proccmd;
static uint8_t procdat;
static int firstspeed;
static int curbpm;

static int realsync;
static int realsynctime;

static int realpos;

static int (*que)[4];
static int querpos;
static int quewpos;
static int quelen;
static int cmdtime;
static int realtempo;
static int realspeed;
static int realgvol;


enum
{
	quePos, queSync, queTempo, queSpeed, queGVol
};

static short sintab[256]=
{
	    0,    50,   100,   151,   201,   251,   301,   350,
	  400,   449,   498,   546,   595,   642,   690,   737,
	  784,   830,   876,   921,   965,  1009,  1053,  1096,
	 1138,  1179,  1220,  1260,  1299,  1338,  1375,  1412,
	 1448,  1483,  1517,  1551,  1583,  1615,  1645,  1674,
	 1703,  1730,  1757,  1782,  1806,  1829,  1851,  1872,
	 1892,  1911,  1928,  1945,  1960,  1974,  1987,  1998,
	 2009,  2018,  2026,  2033,  2038,  2042,  2046,  2047,
	 2048,  2047,  2046,  2042,  2038,  2033,  2026,  2018,
	 2009,  1998,  1987,  1974,  1960,  1945,  1928,  1911,
	 1892,  1872,  1851,  1829,  1806,  1782,  1757,  1730,
	 1703,  1674,  1645,  1615,  1583,  1551,  1517,  1483,
	 1448,  1412,  1375,  1338,  1299,  1260,  1220,  1179,
	 1138,  1096,  1053,  1009,   965,   921,   876,   830,
	  784,   737,   690,   642,   595,   546,   498,   449,
	  400,   350,   301,   251,   201,   151,   100,    50,
	    0,   -50,  -100,  -151,  -201,  -251,  -301,  -350,
	 -400,  -449,  -498,  -546,  -595,  -642,  -690,  -737,
	 -784,  -830,  -876,  -921,  -965, -1009, -1053, -1096,
	-1138, -1179, -1220, -1260, -1299, -1338, -1375, -1412,
	-1448, -1483, -1517, -1551, -1583, -1615, -1645, -1674,
	-1703, -1730, -1757, -1782, -1806, -1829, -1851, -1872,
	-1892, -1911, -1928, -1945, -1960, -1974, -1987, -1998,
	-2009, -2018, -2026, -2033, -2038, -2042, -2046, -2047,
	-2048, -2047, -2046, -2042, -2038, -2033, -2026, -2018,
	-2009, -1998, -1987, -1974, -1960, -1945, -1928, -1911,
	-1892, -1872, -1851, -1829, -1806, -1782, -1757, -1730,
	-1703, -1674, -1645, -1615, -1583, -1551, -1517, -1483,
	-1448, -1412, -1375, -1338, -1299, -1260, -1220, -1179,
	-1138, -1096, -1053, -1009,  -965,  -921,  -876,  -830,
	 -784,  -737,  -690,  -642,  -595,  -546,  -498,  -449,
	 -400,  -350,  -301,  -251,  -201,  -151,  -100,   -50
};

static int freqrange(int x)
{
	if (linearfreq)
		return (x<-72*256)?-72*256:(x>96*256)?96*256:x;
	else
		return (x<107)?107:(x>438272)?438272:x;
}

static int volrange(int x)
{
	return (x<0)?0:(x>0x40)?0x40:x;
}

static int panrange(int x)
{
	return (x<0)?0:(x>0xFF)?0xFF:x;
}


static void ReadQue (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int type,val1,val2,t;
	int i;
	int time = cpifaceSession->mcpGet(-1, mcpGTimer);
	while (1)
	{
		if (querpos==quewpos)
			break;
		if (time<que[querpos][0])
			break;

		t=que[querpos][0];
		type=que[querpos][1];
		val1=que[querpos][2];
		val2=que[querpos][3];
		querpos=(querpos+1)%quelen;

		switch (type)
		{
			case queSync:
				realsync=val2;
				realsynctime=t;
				channels[val1].chSync=val2;
				channels[val1].chSyncTime=t;
				break;
			case quePos:
				realpos=val2;
				for (i=0; i<nchan; i++)
				{
					struct channel *c=&channels[i];
					if (c->evpos==-1)
					{
						if (c->evpos0==realpos)
						{
							c->evpos=realpos;
							c->evtime=t;
						}
					} else {
						switch (c->evmodtype)
						{
							case 1:
								c->evmodpos++;
								break;
							case 2:
								if (!(realpos&0xFF))
									c->evmodpos++;
								break;
							case 3:
								if (!(realpos&0xFFFF))
									c->evmodpos++;
								break;
						}
						if ((c->evmodpos==c->evmod)&&c->evmod)
						{
							c->evmodpos=0;
							c->evpos=realpos;
							c->evtime=t;
						}
					}
				}
				break;
			case queGVol: realgvol=val2; break;
			case queTempo: realtempo=val2; break;
			case queSpeed: realspeed=val2; break;
		}
	}
}

static void putque(int type, int val1, int val2)
{
	if (((quewpos+1)%quelen)==querpos)
		return;
	que[quewpos][0]=cmdtime;
	que[quewpos][1]=type;
	que[quewpos][2]=val1;
	que[quewpos][3]=val2;
	quewpos=(quewpos+1)%quelen;
}


static void PlayNote(struct cpifaceSessionAPI_t *cpifaceSession, struct channel *ch)
{
	int portatmp=0;
	int delaytmp;
	int keyoff=0;

	if (proccmd==xmpCmdPortaNote)
		portatmp=1;
	if (proccmd==xmpCmdPortaVol)
		portatmp=1;
	if ((procvol>>4)==xmpVCmdPortaNote)
		portatmp=1;

	delaytmp=(proccmd==xmpCmdDelayNote)&&procdat;

	if (procnot==97)
	{
		procnot=0;
		procins=0;
		keyoff=1;
	}

	if ((proccmd==xmpCmdKeyOff)&&!procdat)
		keyoff=1;

	if (!ch->chCurIns)
		return;

	if (ismod && !procnot && procins && ch->chCurIns!=ch->chLastIns)
		procnot=ch->curnote;

	if (procins && !keyoff && !delaytmp)
		ch->chSustain=1;

	if (procnot && !delaytmp)
		ch->curnote=procnot;

	if (procins && (ismod || !delaytmp))
	{
		int32_t checknote = ch->curnote;
		if (!checknote)
			checknote=49;
		if (ismod)
			ch->cursamp=&samples[ch->chCurIns-1];
		else {
			struct xmpinstrument *ins=&instruments[ch->chCurIns-1];
			if (ins->samples[checknote-1]>nsamp)
				return;
			ch->cursamp=&samples[ins->samples[checknote-1]];
		}
		ch->chDefVol=(ch->cursamp->stdvol+1)>>2;
		ch->chDefPan=ch->cursamp->stdpan;
	}

	if (procnot && !delaytmp)
	{

		if (!portatmp)
		{
			int32_t nn, frq;
			ch->nextstop=1;
			ch->notehit=1;

			if (!ismod && procins)
			{
				struct xmpinstrument *ins=&instruments[ch->chCurIns-1];
				if (ins->samples[ch->curnote-1]>nsamp)
					return;
				ch->cursamp=&samples[ins->samples[ch->curnote-1]];
				ch->chDefVol=(ch->cursamp->stdvol+1)>>2;
				ch->chDefPan=ch->cursamp->stdpan;
			}

			if (ch->cursamp)
			{
				ch->nextsamp=ch->cursamp->handle;

				nn=ch->cursamp->normnote;
				if (proccmd==xmpCmdSFinetune)
				{
					nn=ch->cursamp->normtrans-(int16_t)(procdat<<4)+0x80;
					ch->fx=xfxSetFinetune;
				}

				ch->chCurNormNote=nn;
			} else {
				/* if we have no sample yet, just do as much as we can */
				if (proccmd==xmpCmdSFinetune)
					ch->fx=xfxSetFinetune;
			}

			frq=48*256-(((procnot-1)<<8)-ch->chCurNormNote);
			if (!linearfreq)
				frq=cpifaceSession->mcpAPI->GetFreq6848(frq);
			ch->chPitch=frq;
			ch->chFinalPitch=frq;
			ch->chPortaToPitch=frq;

			ch->nextpos=0;

			if (proccmd==xmpCmdOffset)
			{
				if (procdat!=0)
					ch->chOffset=procdat;
				ch->nextpos=ch->chOffset<<8;
				if (ismod && ch->nextpos>sampleinfos[ch->nextsamp].length)
					ch->nextpos=sampleinfos[ch->nextsamp].length-16;
				ch->fx=xfxOffset;
			}

			ch->chVibPos=0;
			ch->chTremPos=0;
			ch->chArpPos=0;
			ch->chMRetrigPos=0;
			ch->chTremorPos=0;
		} else {
			int32_t frq=48*256-(((procnot-1)<<8)-ch->chCurNormNote);
			if (!linearfreq)
				frq=cpifaceSession->mcpAPI->GetFreq6848(frq);
			ch->chPortaToPitch=frq;
		}
	}

	if (procnot && delaytmp && !ismod)
		return;

	if (keyoff&&ch->cursamp)
	{
		ch->chSustain=0;
		if ((ch->cursamp->volenv>=nenv)&&!procins)
			ch->chFadeVol=0;
	}

	if (procins && (ismod || ch->chSustain))
	{
		ch->chVol=ch->chDefVol;
		ch->chFinalVol=ch->chDefVol;
		if (ch->chDefPan!=-1)
		{
			ch->chPan=ch->chDefPan;
			ch->chFinalPan=ch->chDefPan;
		}
		ch->chFadeVol=0x8000;
		ch->chAVibPos=0;
		ch->chAVibSwpPos=0;
		ch->chVolEnvPos=0;
		ch->chPanEnvPos=0;
	}
}

static uint16_t notetab[16]={32768,30929,29193,27554,26008,24548,23170,21870,20643,19484,18390,17358,16384,15464,14596,13777};

static void xmpPlayTick (struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;
	struct xmpsample *sm;
	int vol, pan;

	if ((!looping) && looped)
	{
		cpifaceSession->mcpSet (-1, mcpMasterPause, 1);
		return;
	}

	if (firstspeed)
	{
		cpifaceSession->mcpSet (-1, mcpGSpeed, firstspeed);
		firstspeed=0;
	}

	cmdtime = cpifaceSession->mcpGet (-1, mcpGCmdTimer);
	ReadQue (cpifaceSession);

	tick0=0;
	for (i=0; i<nchan; i++)
	{
		struct channel *ch=&channels[i];
		ch->chFinalVol=ch->chVol;
		ch->chFinalPan=ch->chPan;
		ch->chFinalPitch=ch->chPitch;
		ch->nextstop=0;
		ch->nextsamp=-1;
		ch->nextpos=-1;
	}

	curtick++;
	if (curtick>=curtempo)
		curtick=0;

	if (!curtick&&patdelay)
	{
		if (jumptoord!=-1)
		{
			if (jumptoord!=curord)
				for (i=0; i<nchan; i++)
				{
					struct channel *ch=&channels[i];
					ch->chPatLoopCount=0;
					ch->chPatLoopStart=0;
				}

			if (jumptoord>=nord)
			{
				jumptoord=loopord;
				if (!usersetpos)
					looped=1;
			}
			if ((jumptoord<curord)&&!usersetpos)
				looped=1;
			usersetpos=0;

			curord=jumptoord;
			currow=jumptorow;
			jumptoord=-1;
			jumptorow=0;
			patlen=patlens[orders[curord]];
			patptr=patterns[orders[curord]];
		}
	}

	if (!curtick && (!patdelay || ismod))
	{
		// no more ticks, we need to step
		tick0=1;

		if (!patdelay)
		{
			currow++;
			// no jump configured? and at the end of row? jump to the next order, and start fresh
			if ((jumptoord==-1)&&(currow>=patlen))
			{
				jumptoord=curord+1;
				jumptorow=nextpatternrow;
				nextpatternrow=0;
			}
			// jump is configured
			if (jumptoord!=-1)
			{
				// jump is not the same order.. (jump is not caused by a loop)
				if (jumptoord!=curord)
					for (i=0; i<nchan; i++)
					{ // reset all loop counters
						struct channel *ch=&channels[i];
						ch->chPatLoopCount=0;
						ch->chPatLoopStart=0;
					}

				// jumping into/beyond EOF, loop module
				if (jumptoord>=nord)
				{
					jumptoord=loopord;
				}
				// if jump is backwards, song has globally looped, flag it for the UI
				if ((jumptoord<curord)&&!usersetpos)
					looped=1;
				usersetpos=0;

				// take the position, and clear jumptoord
				curord=jumptoord;
				currow=jumptorow;
				jumptoord=-1;
				jumptorow=0;
				patlen=patlens[orders[curord]];
				patptr=patterns[orders[curord]];
			}
		}

		for (i=0; i<nchan; i++)
		{
			struct channel *ch=&channels[i];

			ch->notehit=0;
			ch->volslide=0;
			ch->pitchslide=0;
			ch->panslide=0;
			ch->pitchfx=0;
			ch->volfx=0;
			ch->notefx=0;
			ch->fx=0;

			procnot=patptr[nchan*currow+i][0];
			procins=patptr[nchan*currow+i][1];
			procvol=patptr[nchan*currow+i][2];
			proccmd=patptr[nchan*currow+i][3];
			procdat=patptr[nchan*currow+i][4];
			if (procnot)
			{
				ch->chDelayNote = procnot;
			}

			if (!patdelay)
			{
				if (procnot==97)
					procins=0;
				if (procins && procins<=ninst)
				{
					ch->chLastIns=ch->chCurIns;
					ch->chCurIns=procins;
				}
				if (procins<=ninst)
					PlayNote(cpifaceSession, ch);
			}

			ch->chVCommand=procvol>>4;

			switch (ch->chVCommand)
			{
				case xmpVCmdVol0x: case xmpVCmdVol1x: case xmpVCmdVol2x: case xmpVCmdVol3x:
					if ((proccmd!=xmpCmdDelayNote)||!procdat)
						ch->chFinalVol=ch->chVol=procvol-0x10;
					break;
				case xmpVCmdVol40:
					if ((proccmd!=xmpCmdDelayNote)||!procdat)
						ch->chFinalVol=ch->chVol=0x40;
					break;
				case xmpVCmdVolSlideD: case xmpVCmdVolSlideU: case xmpVCmdPanSlideL: case xmpVCmdPanSlideR:
					ch->chVVolPanSlideVal=procvol&0xF;
					break;
				case xmpVCmdFVolSlideD:
					if ((proccmd!=xmpCmdDelayNote)||!procdat)
						ch->chFinalVol=ch->chVol=volrange(ch->chVol-(procvol&0xF));
					ch->fx=xfxRowVolSlideDown;
					break;
				case xmpVCmdFVolSlideU:
					if ((proccmd!=xmpCmdDelayNote)||!procdat)
						ch->chFinalVol=ch->chVol=volrange(ch->chVol+(procvol&0xF));
					ch->fx=xfxRowVolSlideUp;
					break;
				case xmpVCmdVibRate:
					if (procvol&0xF)
						ch->chVibRate=((procvol&0xF)<<2);
					break;
				case xmpVCmdVibDep:
					ch->pitchfx=xfxPXVibrato;
					if (procvol&0xF)
						ch->chVibDep=((procvol&0xF)<<(1+!linearfreq));
					break;
				case xmpVCmdPanning:
					if ((proccmd!=xmpCmdDelayNote)||!procdat)
						ch->chFinalPan=ch->chPan=(procvol&0xF)*0x11;
					break;
				case xmpVCmdPortaNote:
					ch->pitchslide=xfxPSToNote;
					if (procvol&0xF)
						ch->chPortaToVal=(procvol&0xF)<<8;
					break;
			}

			ch->chCommand=proccmd;
			switch (ch->chCommand)
			{
				case xmpCmdArpeggio:
					if (!procdat)
						ch->chCommand=0xFF;
					else {
						ch->pitchfx=xfxPXArpeggio;
						ch->fx=xfxArpeggio;
					}
					ch->chArpNotes[0]=0;
					ch->chArpNotes[1]=procdat>>4;
					ch->chArpNotes[2]=procdat&0xF;
					break;
				case xmpCmdPortaU:
					if (procdat)
						ch->chPortaUVal=procdat<<4;
					ch->pitchslide=xfxPSUp;
					ch->fx=xfxPitchSlideUp;
					break;
				case xmpCmdPortaD:
					if (procdat)
						ch->chPortaDVal=procdat<<4;
					ch->pitchslide=xfxPSDown;
					ch->fx=xfxPitchSlideDown;
					break;
				case xmpCmdPortaNote:
					if (procdat)
						ch->chPortaToVal=procdat<<4;
					ch->pitchslide=xfxPSToNote;
					ch->fx=xfxPitchSlideToNote;
					break;
				case xmpCmdVibrato:
					ch->pitchfx=xfxPXVibrato;
					ch->fx=xfxPitchVibrato;
					if (procdat&0xF)
						ch->chVibDep=(procdat&0xF)<<(1+!linearfreq);
					if (procdat&0xF0)
						ch->chVibRate=(procdat>>4)<<2;
					break;
				case xmpCmdPortaVol: case xmpCmdVibVol: case xmpCmdVolSlide:
					if (procdat || ismod)
						ch->chVolSlideVal=procdat;
					if (ch->chVolSlideVal&0xf0)
					{
						ch->volslide=xfxVSUp;
						ch->fx=xfxVolSlideUp;
					} else if (ch->chVolSlideVal&0x0f)
					{
						ch->volslide=xfxVSDown;
						ch->fx=xfxVolSlideDown;
					}
					break;
				case xmpCmdTremolo:
					ch->volfx=xfxVXVibrato;
					ch->fx=xfxVolVibrato;
					if (procdat&0xF)
						ch->chTremDep=(procdat&0xF)<<2;
					if (procdat&0xF0)
						ch->chTremRate=(procdat>>4)<<2;
					break;
				case xmpCmdPanning:
					ch->chFinalPan=ch->chPan=procdat;
					break;
				case xmpCmdJump:
					if (!patdelay)
					{
						jumptoord=procdat;
						jumptorow=0;
						nextpatternrow=0;
					}
					break;
				case xmpCmdVolume:
					ch->chFinalVol=ch->chVol=volrange(procdat);
					break;
				case xmpCmdBreak:
					if (!patdelay)
					{
						if (jumptoord==-1)
							jumptoord=curord+1;
						jumptorow=(procdat&0xF)+(procdat>>4)*10;
						nextpatternrow=0;
					}
					break;
				case xmpCmdSpeed:
					if (!procdat)
					{
						jumptoord=0;
						jumptorow=0;
						break;
					}
					if (procdat>=0x20)
					{
						curbpm=procdat;
						cpifaceSession->mcpSet (-1, mcpGSpeed, 256*2*curbpm/5);
						putque(queTempo, -1, curbpm);
					} else {
						curtempo=procdat;
						putque(queSpeed, -1, curtempo);
					}
					break;
				case xmpCmdMODtTempo:
					if (!procdat)
					{
						jumptoord=procdat;
						jumptorow=0;
					} else {
						curtempo=procdat;
						putque(queSpeed, -1, curtempo);
					}
					break;
				case xmpCmdGVolume:
					globalvol=volrange(procdat);
					putque(queGVol, -1, globalvol);
					break;
				case xmpCmdGVolSlide:
					if (procdat)
						ch->chGVolSlideVal=procdat;
					if (ch->chGVolSlideVal&0xf0)
						globalfx=xfxGVSUp;
					else if (ch->chGVolSlideVal&0x0f)
						globalfx=xfxGVSDown;
					break;
				case xmpCmdKeyOff:
					ch->chActionTick=procdat;
					break;
				case xmpCmdRetrigger:
					ch->notefx=xfxNXRetrig;
					ch->fx=xfxRetrig;
					ch->chActionTick=procdat;
					break;
				case xmpCmdNoteCut:
					ch->notefx=xfxNXNoteCut;
					ch->fx=xfxNoteCut;
					ch->chActionTick=procdat;
					break;
				case xmpCmdEnvPos:
					ch->chVolEnvPos=ch->chPanEnvPos=procdat;
					ch->fx=xfxEnvPos;
					if (ch->cursamp)
					{
						if (ch->cursamp->volenv<nenv)
							if (ch->chVolEnvPos>envelopes[ch->cursamp->volenv].len)
								ch->chVolEnvPos=envelopes[ch->cursamp->volenv].len;
						if (ch->cursamp->panenv<nenv)
							if (ch->chPanEnvPos>envelopes[ch->cursamp->panenv].len)
								ch->chPanEnvPos=envelopes[ch->cursamp->panenv].len;
					} else
						fprintf(stderr, __FILE__ " CmdEnvPos ch->cursamp not set\n");
					break;
				case xmpCmdPanSlide:
					if (procdat)
						ch->chPanSlideVal=procdat;
					if (ch->chPanSlideVal&0xF0)
						ch->panslide=xfxPnSLeft;
					else if (ch->chPanSlideVal&0x0F)
						ch->panslide=xfxPnSRight;
					break;
				case xmpCmdMRetrigger:
					ch->notefx=xfxNXRetrig;
					ch->fx=xfxRetrig;
					if (procdat)
					{
						ch->chMRetrigLen=procdat&0xF;
						ch->chMRetrigAct=procdat>>4;
					}
					break;
				case xmpCmdSync1: case xmpCmdSync2: case xmpCmdSync3:
					putque(queSync, i, procdat);
					break;
				case xmpCmdTremor:
					ch->volfx=xfxVXTremor;
					ch->fx=xfxTremor;
					if (procdat)
					{
						ch->chTremorLen=(procdat&0xF)+(procdat>>4)+2;
						ch->chTremorOff=(procdat>>4)+1;
						ch->chTremorPos=0;
					}
					break;
				case xmpCmdXPorta:
					if ((procdat>>4)==1)
					{
						if (procdat&0xF)
							ch->chXFinePortaUVal=procdat&0xF;
						ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch-(ch->chXFinePortaUVal<<2));
					} else if ((procdat>>4)==2)
					{
						if (procdat&0xF)
							ch->chXFinePortaDVal=procdat&0xF;
						ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch+(ch->chXFinePortaDVal<<2));
					}
					break;
				case xmpCmdFPortaU:
					if (procdat)
						ch->chFinePortaUVal=procdat;
					ch->fx=xfxRowPitchSlideUp;
					ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch-(ch->chFinePortaUVal<<4));
					break;
				case xmpCmdFPortaD:
					if (procdat)
						ch->chFinePortaDVal=procdat;
					ch->fx=xfxRowPitchSlideDown;
					ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch+(ch->chFinePortaDVal<<4));
					break;
				case xmpCmdGlissando:
					ch->chGlissando=procdat;
					break;
				case xmpCmdVibType:
					ch->chVibType=procdat&3;
					break;
				case xmpCmdPatLoop:
					/* if(plLoopPatterns)*/ /* TODO ?? */
					{
						if (!procdat)
						{
							ch->chPatLoopStart=currow;
							if (ft2_e60bug)
							{
								nextpatternrow=currow;
							}
						} else {
							ch->chPatLoopCount++;
							if (ch->chPatLoopCount<=procdat)
							{
								jumptorow=ch->chPatLoopStart;
								jumptoord=curord;
							} else {
								ch->chPatLoopCount=0;
								ch->chPatLoopStart=currow+1;
							}
						}
					}
					break;
				case xmpCmdTremType:
					ch->chTremType=procdat&3;
					break;
				case xmpCmdSPanning:
					ch->chFinalPan=ch->chPan=procdat*0x11;
					break;
				case xmpCmdFVolSlideU:
					if (procdat || ismod )
						ch->chFineVolSlideUVal=procdat;
					ch->fx=xfxRowVolSlideUp;
					ch->chFinalVol=ch->chVol=volrange(ch->chVol+ch->chFineVolSlideUVal);
					break;
				case xmpCmdFVolSlideD:
					if (procdat || ismod )
						ch->chFineVolSlideDVal=procdat;
					ch->fx=xfxRowVolSlideDown;
					ch->chFinalVol=ch->chVol=volrange(ch->chVol-ch->chFineVolSlideDVal);
					break;
				case xmpCmdPatDelay:
					if (!patdelay)
						patdelay=procdat+1;
					break;
				case xmpCmdDelayNote:
					ch->fx=xfxDelay;
					ch->notefx=xfxNXDelay;
					ch->chDelayIns=procins;
					ch->chDelayVol=procvol;
					ch->chActionTick=procdat;
					break;
			}
		}
	}
	if (!curtick&&patdelay)
	{
		patdelay--;
	}

	for (i=0; i<nchan; i++)
	{
		struct channel *ch=&channels[i];

		switch (ch->chVCommand)
		{
			case xmpVCmdVolSlideD:
				ch->volslide=xfxVSDown;
				if (tick0)
					break;
				ch->chFinalVol=ch->chVol=volrange(ch->chVol-ch->chVVolPanSlideVal);
				break;
			case xmpVCmdVolSlideU:
				ch->volslide=xfxVSUp;
				if (tick0)
					break;
				ch->chFinalVol=ch->chVol=volrange(ch->chVol+ch->chVVolPanSlideVal);
				break;
			case xmpVCmdVibDep:  /* FICKEN */
				switch (ch->chVibType)
				{
					case 0:
						ch->chFinalPitch=freqrange((( sintab[ch->chVibPos] *ch->chVibDep)>>7)+ch->chPitch);
						break;
					case 1:
						ch->chFinalPitch=freqrange((( (ch->chVibPos-0x80)   *ch->chVibDep)>>3)+ch->chPitch);
						break;
					case 2:
						ch->chFinalPitch=freqrange((( ((ch->chVibPos&0x80)-0x40) *ch->chVibDep)>>2)+ch->chPitch);
						break;
				}
				if (!tick0)
					ch->chVibPos+=ch->chVibRate;
				break;
			case xmpVCmdPanSlideL:
				if (tick0)
					break;
				ch->chFinalPan=ch->chPan=panrange(ch->chPan-ch->chVVolPanSlideVal);
				break;
			case xmpVCmdPanSlideR:
				if (tick0)
					break;
				ch->chFinalPan=ch->chPan=panrange(ch->chPan+ch->chVVolPanSlideVal);
				break;
			case xmpVCmdPortaNote:
				if (!tick0)
				{
					if (ch->chPitch<ch->chPortaToPitch)
					{
						ch->chPitch+=ch->chPortaToVal;
						if (ch->chPitch>ch->chPortaToPitch)
							ch->chPitch=ch->chPortaToPitch;
					} else {
						ch->chPitch-=ch->chPortaToVal;
						if (ch->chPitch<ch->chPortaToPitch)
							ch->chPitch=ch->chPortaToPitch;
					}
				}
				if (ch->chGlissando)
				{
					if (linearfreq)
					{
						ch->chFinalPitch=((ch->chPitch+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote;
					} else {
						ch->chFinalPitch=cpifaceSession->mcpAPI->GetFreq6848(((cpifaceSession->mcpAPI->GetNote6848(ch->chPitch)+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote);
					}
				} else
					ch->chFinalPitch=ch->chPitch;
				break;
		}

		switch (ch->chCommand)
		{
			case xmpCmdArpeggio:
				if (linearfreq)
					ch->chFinalPitch=freqrange(ch->chPitch-(ch->chArpNotes[ch->chArpPos]<<8));
				else
					ch->chFinalPitch=freqrange((ch->chPitch*notetab[ch->chArpNotes[ch->chArpPos]])>>15);
				ch->chArpPos++;
				if (ch->chArpPos==3)
					ch->chArpPos=0;
				break;
			case xmpCmdPortaU:
				if (tick0)
					break;
				ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch-ch->chPortaUVal);
				break;
			case xmpCmdPortaD:
				if (tick0)
					break;
				ch->chFinalPitch=ch->chPitch=freqrange(ch->chPitch+ch->chPortaDVal);
				break;
			case xmpCmdPortaNote:
				if (!tick0)
				{
					if (ch->chPitch<ch->chPortaToPitch)
					{
						ch->chPitch+=ch->chPortaToVal;
						if (ch->chPitch>ch->chPortaToPitch)
							ch->chPitch=ch->chPortaToPitch;
					} else {
						ch->chPitch-=ch->chPortaToVal;
						if (ch->chPitch<ch->chPortaToPitch)
							ch->chPitch=ch->chPortaToPitch;
					}
				}
				if (ch->chGlissando)
				{
					if (linearfreq)
						ch->chFinalPitch=((ch->chPitch+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote;
					else
						ch->chFinalPitch=cpifaceSession->mcpAPI->GetFreq6848(((cpifaceSession->mcpAPI->GetNote6848(ch->chPitch)+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote);
				} else
					ch->chFinalPitch=ch->chPitch;
				break;
			case xmpCmdVibrato:
				switch (ch->chVibType)
				{
					case 0:
						ch->chFinalPitch=freqrange((( sintab[ch->chVibPos] *ch->chVibDep)>>8)+ch->chPitch);
						break;
					case 1:
						ch->chFinalPitch=freqrange((( (ch->chVibPos-0x80)   *ch->chVibDep)>>4)+ch->chPitch);
						break;
					case 2:
						ch->chFinalPitch=freqrange((( ((ch->chVibPos&0x80)-0x40) *ch->chVibDep)>>3)+ch->chPitch);
						break;
				}
				if (!tick0)
					ch->chVibPos+=ch->chVibRate;
				break;
			case xmpCmdPortaVol:
				if (!tick0)
				{
					if (ch->chPitch<ch->chPortaToPitch)
					{
						ch->chPitch+=ch->chPortaToVal;
						if (ch->chPitch>ch->chPortaToPitch)
							ch->chPitch=ch->chPortaToPitch;
					} else {
						ch->chPitch-=ch->chPortaToVal;
						if (ch->chPitch<ch->chPortaToPitch)
							ch->chPitch=ch->chPortaToPitch;
					}
				}
				if (ch->chGlissando)
				{
					if (linearfreq)
						ch->chFinalPitch=((ch->chPitch+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote;
					else
						ch->chFinalPitch=cpifaceSession->mcpAPI->GetFreq6848(((cpifaceSession->mcpAPI->GetNote6848(ch->chPitch)+ch->chCurNormNote+0x80)&~0xFF)-ch->chCurNormNote);
				} else
					ch->chFinalPitch=ch->chPitch;

				if (tick0)
					break;
				ch->chFinalVol=ch->chVol=volrange(ch->chVol+((ch->chVolSlideVal&0xF0)?(ch->chVolSlideVal>>4):-(ch->chVolSlideVal&0xF)));
				break;
			case xmpCmdVibVol:
				switch (ch->chVibType)
				{
					case 0:
						ch->chFinalPitch=freqrange((( sintab[ch->chVibPos] *ch->chVibDep)>>8)+ch->chPitch);
						break;
					case 1:
						ch->chFinalPitch=freqrange((( (ch->chVibPos-0x80)   *ch->chVibDep)>>4)+ch->chPitch);
						break;
					case 2:
						ch->chFinalPitch=freqrange((( ((ch->chVibPos&0x80)-0x40) *ch->chVibDep)>>3)+ch->chPitch);
						break;
				}
				if (!tick0)
					ch->chVibPos+=ch->chVibRate;

				if (tick0)
					break;
				ch->chFinalVol=ch->chVol=volrange(ch->chVol+((ch->chVolSlideVal&0xF0)?(ch->chVolSlideVal>>4):-(ch->chVolSlideVal&0xF)));
				break;
			case xmpCmdTremolo:
				switch (ch->chTremType)
				{
					case 0:
						ch->chFinalVol+=(( sintab[ch->chTremPos] *ch->chTremDep)>>11);
						break;
					case 1:
						ch->chFinalVol+=(( (ch->chTremPos-0x80)   *ch->chTremDep)>>7);
						break;
					case 2:
						ch->chFinalVol+=(( ((ch->chTremPos&0x80)-0x40) *ch->chTremDep)>>6);
						break;
				}
				ch->chFinalVol=volrange(ch->chFinalVol);
				if (!tick0)
					ch->chTremPos+=ch->chTremRate;
				break;
			case xmpCmdVolSlide:
				if (tick0)
					break;
				ch->chFinalVol=ch->chVol=volrange(ch->chVol+((ch->chVolSlideVal&0xF0)?(ch->chVolSlideVal>>4):-(ch->chVolSlideVal&0xF)));
				break;
			case xmpCmdGVolSlide:
				if (tick0)
					break;
				if (ch->chGVolSlideVal&0xF0)
					globalvol=volrange(globalvol+(ch->chGVolSlideVal>>4));
				else
					globalvol=volrange(globalvol-(ch->chGVolSlideVal&0xF));
				putque(queGVol, -1, globalvol);
				break;
			case xmpCmdKeyOff:
				if (tick0)
					break;
				if (curtick==ch->chActionTick)
				{
					ch->chSustain=0;
					if (ch->cursamp&&(ch->cursamp->volenv>=nenv))
						ch->chFadeVol=0;
				}
				break;
			case xmpCmdPanSlide:
				if (tick0)
					break;
				ch->chFinalPan=ch->chPan=panrange(ch->chPan+((ch->chPanSlideVal&0xF0)?(ch->chPanSlideVal>>4):-(ch->chPanSlideVal&0xF)));
				break;
			case xmpCmdMRetrigger:
				if (ch->chMRetrigPos++!=ch->chMRetrigLen)
					break;
				ch->chMRetrigPos=1;
				ch->nextpos=0;
				ch->chVolEnvPos=0;
				ch->chPanEnvPos=0;

				switch (ch->chMRetrigAct)
				{
					case 0: case 8: break;
					case 1: case 2: case 3: case 4: case 5:
							ch->chVol=ch->chVol-(1<<(ch->chMRetrigAct-1));
							break;
					case 9: case 10: case 11: case 12: case 13:
							ch->chVol=ch->chVol+(1<<(ch->chMRetrigAct-9));
							break;
					case 6:  ch->chVol=(ch->chVol*5)>>3; break;
					case 14: ch->chVol=(ch->chVol*3)>>1; break;
					case 7:  ch->chVol>>=1; break;
					case 15: ch->chVol<<=1; break;
				}
				ch->chFinalVol=ch->chVol=volrange(ch->chVol);
				break;
			case xmpCmdTremor:
				if (ch->chTremorPos>=ch->chTremorOff)
					ch->chFinalVol=0;
				if (tick0)
					break;
				ch->chTremorPos++;
				if (ch->chTremorPos==ch->chTremorLen)
					ch->chTremorPos=0;
				break;
			case xmpCmdRetrigger:
				if (!ch->chActionTick)
					break;
				if (!(curtick%ch->chActionTick))
				{
					ch->nextpos=0;
					ch->chVolEnvPos=0;
					ch->chPanEnvPos=0;
				}

				break;
			case xmpCmdNoteCut:
				if (tick0)
					break;
				if (curtick==ch->chActionTick)
					ch->chFinalVol=ch->chVol=0;
				break;
			case xmpCmdDelayNote:
				if (tick0)
					break;
				if (curtick!=ch->chActionTick)
					break;
				procnot=ch->chDelayNote;
				procins=ch->chDelayIns;
				proccmd=0;
				procdat=0;
				procvol=0;
				PlayNote(cpifaceSession, ch);
				switch (ch->chDelayVol>>4)
				{
					case xmpVCmdVol0x: case xmpVCmdVol1x: case xmpVCmdVol2x: case xmpVCmdVol3x:
						ch->chFinalVol=ch->chVol=ch->chDelayVol-0x10;
						break;
					case xmpVCmdVol40:
						ch->chFinalVol=ch->chVol=0x40;
						break;
					case xmpVCmdPanning:
						ch->chFinalPan=ch->chPan=(ch->chDelayVol&0xF)*0x11;
						break;
				}
				break;
		}

		if (!ch->cursamp)
		{
			cpifaceSession->mcpSet (i, mcpCStatus, 0);
			continue;
		}

		sm=ch->cursamp;

		vol=(ch->chFinalVol*globalvol)>>4;
		pan=ch->chFinalPan-128;
		if (!ch->chSustain)
		{
			vol=(vol*ch->chFadeVol)>>15;
			if (ch->chFadeVol>=sm->volfade)
				ch->chFadeVol-=sm->volfade;
			else
				ch->chFadeVol=0;
		}

		if (sm->volenv<nenv)
		{
			const struct xmpenvelope *env=&envelopes[sm->volenv];
			vol=(env->env[ch->chVolEnvPos]*vol)>>8;

			if (ch->chVolEnvPos<env->len)
				if  (!ch->chSustain || !(env->type&xmpEnvSLoop) || ch->chVolEnvPos!=env->sustain)
				{
					ch->chVolEnvPos++;
					if (env->type&xmpEnvLoop)
					{
						if (ch->chVolEnvPos==env->loope && (ch->chSustain || env->loope!=env->sustain))
							ch->chVolEnvPos=env->loops;
					}
				}
		}

		if (sm->panenv<nenv)
		{
			const struct xmpenvelope *env=&envelopes[sm->panenv];
			pan+=((env->env[ch->chPanEnvPos]-128)*(128-((pan<0)?-pan:pan)))>>7;

			if (ch->chPanEnvPos<env->len)
				if  (!ch->chSustain || !(env->type&xmpEnvSLoop) || ch->chPanEnvPos!=env->sustain)
				{
					ch->chPanEnvPos++;
					if (env->type&xmpEnvLoop)
					{
						if (ch->chVolEnvPos==env->loope && (ch->chSustain || env->loope!=env->sustain))
							ch->chPanEnvPos=env->loops;
					}
				}
		}

		if (sm->vibrate&&sm->vibdepth)
		{
			int dep=0;
			switch (sm->vibtype)
			{
				case 0:
					dep=(sintab[ch->chAVibPos>>8]*sm->vibdepth)>>11;
					break;
				case 1:
					dep=(ch->chAVibPos&0x8000)?-sm->vibdepth:sm->vibdepth;
					break;
				case 2:
					dep=(sm->vibdepth*(32768-ch->chAVibPos))>>14;
					break;
				case 3:
					dep=(sm->vibdepth*(ch->chAVibPos-32768))>>14;
					break;
			}

			ch->chAVibSwpPos+=sm->vibsweep;
			if (ch->chAVibSwpPos>0x10000)
				ch->chAVibSwpPos=0x10000;
			dep=(dep*(int)ch->chAVibSwpPos)>>17;

			ch->chFinalPitch-=dep;

			ch->chAVibPos+=sm->vibrate;
		}

		if (ch->nextstop)
			cpifaceSession->mcpSet (i, mcpCStatus, 0);
		if (ch->nextsamp!=(unsigned)-1)
			cpifaceSession->mcpSet( i, mcpCInstrument, ch->nextsamp);
		if (ch->nextpos!=(unsigned)-1)
		{
			cpifaceSession->mcpSet (i, mcpCPosition, ch->nextpos);
			cpifaceSession->mcpSet (i, mcpCLoop, 1);
			cpifaceSession->mcpSet (i, mcpCDirect, 0);
			cpifaceSession->mcpSet (i, mcpCStatus, 1);
		}
		if (linearfreq)
			cpifaceSession->mcpSet (i, mcpCPitch, -ch->chFinalPitch);
		else
			cpifaceSession->mcpSet (i, mcpCPitch6848, ch->chFinalPitch);
		cpifaceSession->mcpSet (i, mcpCVolume, (looping||!looped)?vol:0);
		cpifaceSession->mcpSet (i, mcpCPanning, pan);
		cpifaceSession->mcpSet (i, mcpCMute, mutech[i]);
	}
	putque(quePos, -1, curtick|(curord<<16)|(currow<<8));
}

int __attribute__ ((visibility ("internal"))) xmpGetRealPos (struct cpifaceSessionAPI_t *cpifaceSession)
{
	ReadQue (cpifaceSession);
	return realpos;
}

int __attribute__ ((visibility ("internal"))) xmpChanActive (struct cpifaceSessionAPI_t *cpifaceSession, int ch)
{
	return cpifaceSession->mcpGet(ch, mcpCStatus)&&channels[ch].cursamp&&channels[ch].chVol&&channels[ch].chFadeVol;
}

int __attribute__ ((visibility ("internal"))) xmpGetChanIns(int ch)
{
	return channels[ch].chCurIns;
}

int __attribute__ ((visibility ("internal"))) xmpGetChanSamp(int ch)
{
	if (!channels[ch].cursamp)
		return 0xFFFF;
	return channels[ch].cursamp-samples;
}

int __attribute__ ((visibility ("internal"))) xmpGetDotsData (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int *smp, int *frq, int *voll, int *volr, int *sus)
{
	struct channel *c;

	if (!cpifaceSession->mcpGet (ch, mcpCStatus))
		return 0;
	c=&channels[ch];
	if (!c->cursamp||!c->chVol||!c->chFadeVol)
		return 0;
	*smp=c->cursamp-samples;
	if (linearfreq)
		*frq=60*256+c->cursamp->normnote-freqrange(c->chFinalPitch);
	else
		*frq=60*256+c->cursamp->normnote+cpifaceSession->mcpAPI->GetNote8363(6848*8363/freqrange(c->chFinalPitch));
	cpifaceSession->mcpGetRealVolume (ch, voll, volr);
	*sus=c->chSustain;
	return 1;
}

void __attribute__ ((visibility ("internal"))) xmpGetRealVolume (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int *voll, int *volr)
{
	cpifaceSession->mcpGetRealVolume (ch, voll, volr);
}

uint16_t __attribute__ ((visibility ("internal"))) xmpGetPos(void)
{
	return (curord<<8)|currow;
}

void __attribute__ ((visibility ("internal"))) xmpSetPos (struct cpifaceSessionAPI_t *cpifaceSession, int ord, int row)
{
	int i;

	if (row<0)
		ord--;
	if (ord>=nord)
		ord=0;
	if (ord<0)
	{
		ord=0;
		row=0;
	}
	if (row>=patlens[orders[ord]])
	{
		ord++;
		row=0;
	}
	if (ord>=nord)
		ord=0;
	if (row<0)
	{
		row+=patlens[orders[ord]];
		if (row<0)
			row=0;
	}
	for (i=0; i<nchan; i++)
		cpifaceSession->mcpSet (i, mcpCReset, 0);
	jumptoord=ord;
	jumptorow=row;
	curtick=curtempo;
	curord=ord;
	currow=row;
	usersetpos=1;
	querpos=0;
	quewpos=0;
	realpos=(curord<<16)|(currow<<8);
}

void __attribute__ ((visibility ("internal"))) xmpMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m)
{
	cpifaceSession->MuteChannel[i] = m;
	mutech[i]=m;
}

int __attribute__ ((visibility ("internal"))) xmpLoop(void)
{
	return looped;
}

void __attribute__ ((visibility ("internal"))) xmpSetLoop(int x)
{
	looping=x;
}

int __attribute__ ((visibility ("internal"))) xmpLoadSamples (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m)
{
	return cpifaceSession->mcpDevAPI->LoadSamples (m->sampleinfos, m->nsampi);
}

int __attribute__ ((visibility ("internal"))) xmpPlayModule (struct xmodule *m, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;

	memset(channels, 0, sizeof(channels));

	looping=1;
	globalvol=0x40;
	realgvol=0x40;
	jumptorow=0;
	jumptoord=0;
	curord=0;
	currow=0;
	realpos=0;
	ninst=m->ninst;
	nord=m->nord;
	nsamp=m->nsamp;
	instruments=m->instruments;
	envelopes=m->envelopes;
	samples=m->samples;
	sampleinfos=m->sampleinfos;
	patterns=m->patterns;
	orders=m->orders;
	patlens=m->patlens;
	linearfreq=m->linearfreq;
	nchan=m->nchan;
	loopord=m->loopord;
	nenv=m->nenv;
	ismod=m->ismod;
	ft2_e60bug=m->ft2_e60bug;
	looped=0;

	curtempo=m->initempo;
	curtick=m->initempo-1;

	for (i=0; i<nchan; i++)
	{
		channels[i].chPan=m->panpos[i];
		mutech[i]=0;
	}

	quelen=100;
	que=malloc(sizeof(int)*quelen*4);
	if (!que)
	{
		return errAllocMem;
	}
	querpos=0;
	quewpos=0;

	curbpm=m->inibpm;
	realtempo=m->inibpm;
	realspeed=m->initempo;
	firstspeed=256*2*curbpm/5;
	if (!cpifaceSession->mcpDevAPI->OpenPlayer(nchan, xmpPlayTick, file, cpifaceSession))
	{
		return errPlay;
	}

	cpifaceSession->mcpAPI->Normalize (cpifaceSession, mcpNormalizeDefaultPlayW);

	if (nchan != cpifaceSession->PhysicalChannelCount)
	{
		cpifaceSession->mcpDevAPI->ClosePlayer (cpifaceSession);
		return errFormStruc;
	}

	return errOk;
}

void __attribute__ ((visibility ("internal"))) xmpStopModule (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpifaceSession->mcpDevAPI->ClosePlayer (cpifaceSession);
	free(que);
}

void __attribute__ ((visibility ("internal"))) xmpGetGlobInfo(int *tmp, int *bpm, int *gvol)
{
	*tmp=realspeed;
	*bpm=realtempo;
	*gvol=realgvol;
}

void __attribute__ ((visibility ("internal"))) xmpGetChanInfo(unsigned char ch, struct xmpchaninfo *ci)
{
	const struct channel *t=&channels[ch];
	ci->note=t->curnote+11;
	ci->vol=t->chVol;
	if (!t->chFadeVol)
		ci->vol=0;
	ci->pan=t->chPan;
	ci->notehit=t->notehit;
	ci->volslide=t->volslide;
	ci->pitchslide=t->pitchslide;
	ci->panslide=t->panslide;
	ci->volfx=t->volfx;
	ci->pitchfx=t->pitchfx;
	ci->notefx=t->notefx;
	ci->fx=t->fx;
}

void __attribute__ ((visibility ("internal"))) xmpGetGlobInfo2(struct xmpglobinfo *gi)
{
	gi->globvol=globalvol;
	gi->globvolslide=globalfx;
}
