/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMDPlay - generic modular module player
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
 *    -fixed offset command behaviour on offset>samplelength
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
#include "gmdplay.h"
#include "stuff/imsrtns.h"

#define MAXLCHAN MP_MAXCHANNELS
#define MAXPCHAN 32

struct trackdata
{
	uint8_t num;
	struct gmdtrack trk;
	uint8_t selinst;
	const struct gmdinstrument *instr;
	const struct gmdsample *samp;
	uint16_t cursampnum;
	int16_t vol;
	int16_t pan;
	int16_t pany;
	int16_t panz;
	uint8_t pansrnd;
	int32_t pitch;
	uint8_t nteval;
	uint8_t notehit;
	uint8_t volslide;
	uint8_t pitchslide;
	uint8_t panslide;
	uint8_t volfx;
	uint8_t pitchfx;
	uint8_t panfx;
	uint8_t notefx;
	int16_t delay;
	uint8_t fx;
	int16_t volslideval;
	uint8_t volslides3m;
	int16_t pitchslideval;
	uint8_t pitchslides3m;
	int32_t pitchslidepitch;
	int16_t pitchslidenteval;
	int8_t panslideval;
	uint8_t volvibpos, volvibspd, volvibamp, volvibwave;
	uint8_t pitchvibpos, pitchvibspd, pitchvibamp, pitchvibwave;
	uint8_t panvibpos, panvibspd, panvibamp, panvibwave;
	uint8_t tremval, trempos, tremon, tremlen;
	uint8_t arpval;
	uint8_t arppos;
	uint8_t arpnte[3];
	uint32_t ofs;
	uint8_t ofshigh;
	uint32_t insofs;
	uint8_t retrig;
	uint8_t retrigpos;
	uint8_t cuttick;
	const uint8_t *delaycmd;
	int16_t rowvolslval;
	int8_t rowpanslval;
	int16_t rowpitchslval;
	int16_t finalvol;
	int16_t finalpan;
	int32_t finalpitch;
	uint16_t venvpos, penvpos, pchenvpos, vibenvpos;
	uint32_t venvfrac, penvfrac, pchenvfrac, vibenvfrac;
	uint32_t vibsweeppos;
	uint16_t fadevol;
	uint8_t sustain;
	uint8_t chanvol;
	uint8_t lastvolsl;
	uint8_t lastpitchsl;
	int8_t glissando;
	uint_fast32_t newpos;
	uint_fast32_t newposend;
	int newdir;
	int newloop;
	int newinst;
	int stopchan;
	int phys;
	int mute;
};

static int pchan[MAXPCHAN];


static uint16_t notetab[16]={32768,30929,29193,27554,26008,24548,23170,21870,20643,19484,18390,17358,16384,15464,14596,13777};

static int16_t sintab[256]=
  {
    0,    50,   100,   151,   201,   251,   301,   350,
    400,   449,   498,   546,   595,   642,   690,   737,
    784,   830,   876,   921,   965,  1009,  1053,  1096,
    1138,  1179,  1220,  1260,  1299,  1338,  1375,  1412,
    1448,  1483,  1517,  1551,  1583,  1615,  1645,  1674,
    1703,  1730,  1757,  1782,  1806,  1829,  1851,  1872,
    1892,  1911,  1928,  1945,  1960,  1974,  1987,  1998,
    2009,  2018,  2026,  2033,  2038,  2042,  2046,  2047,
    2048
    /*
      ,  2047,  2046,  2042,  2038,  2033,  2026,  2018,
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
    */
  };

static uint8_t channels;
static uint8_t physchan;
static uint8_t currenttick;
static uint8_t tempo;
static uint16_t currentrow;
static uint16_t patternlen;
static uint16_t currentpattern;
static int lockpattern;
static uint16_t patternnum;
static uint16_t looppat;
static uint16_t endpat;
static struct trackdata tdata[MP_MAXCHANNELS];
static struct trackdata *tdataend;
static struct gmdtrack gtrack;
static const struct gmdenvelope *envelopes;
static const struct gmdpattern *patterns;
static const struct gmdtrack *tracks;
static const struct gmdsample *modsamples;
static const struct gmdinstrument *instruments;
static const struct sampleinfo *sampleinfos;
static const uint16_t *orders;
static uint16_t instnum;
static uint16_t speed;
static int modsampnum;
static int sampnum;
static int envnum;
static int16_t brkpat;
static int16_t brkrow;
static uint8_t newtickmode;
static uint8_t processtick;
static uint8_t patlooprow[MAXLCHAN];
static uint8_t patloopcount[MAXLCHAN];
static uint8_t globchan;
static uint8_t patdelay;
static uint8_t globalvol;
static uint8_t globalvolslide[MAXLCHAN];
static int8_t globalvolslval[MAXLCHAN];
static uint8_t looped;
static uint8_t exponential;
static uint8_t samiextrawurscht;
static uint8_t samisami;
static uint8_t gusvol;
static uint8_t expopitchenv;
static uint8_t donotloopmodule;
static uint8_t donotshutup;
static int realpos;

static int (*que)[4]; /* one int is padding */
static int querpos;
static int quewpos;
static int quelen;

#define HPITCHMIN (6848>>6)
#define HPITCHMAX ((int32_t)6848<<6)
#define EPITCHMIN -72*256
#define EPITCHMAX 96*256

static void readque(void)
{
	int type,val1/*,val2*/;
	int time=mcpGet(-1, mcpGTimer);
	while (1)
	{
		if (querpos==quewpos)
			break;
		if (time<que[querpos][0])
			break;
		type=que[querpos][1];
		val1=que[querpos][2];
		/* val2=que[querpos][3] */;
		querpos=(querpos+1)%quelen;
		if (type==-1)
			realpos=val1;
	}
}

static void trackmoveto(struct gmdtrack *t, uint8_t row)
{
	while (1)
	{
		if (t->ptr>=t->end)
			break;
		if (t->ptr[0]>=row)
			break;
		t->ptr+=t->ptr[1]+2;
    }
}

static void LoadPattern(uint16_t p, uint8_t r)
{
	const struct gmdpattern *pat=&patterns[orders[p]];
	struct trackdata *td;

	patternlen=pat->patlen;
	if (r>=patternlen)
		r=0;
	currenttick=0;
	currentrow=r;
	currentpattern=p;

	gtrack=tracks[pat->gtrack];
	trackmoveto(&gtrack, r);
	for (td=tdata; td<tdataend; td++)
	{
		td->trk=tracks[pat->tracks[td->num]];
		trackmoveto(&td->trk, r);
	}
}

static inline unsigned char checkvol(int16_t v)
{
	return (v<0)?0:(v>255)?255:v;
}

static inline unsigned char checkpan(int16_t p)
{
	return (p<0)?0:(p>=255)?255:p;
}

static inline signed long checkpitchh(int32_t pitch)
{
	return (pitch<HPITCHMIN)?HPITCHMIN:(pitch>HPITCHMAX)?HPITCHMAX:pitch;
}

static inline signed long checkpitche(int32_t pitch)
{
	return (pitch<EPITCHMIN)?EPITCHMIN:(pitch>EPITCHMAX)?EPITCHMAX:pitch;
}

static inline signed long checkpitch(int32_t pitch)
{
	if (exponential)
		return checkpitche(pitch);
	else
		return checkpitchh(pitch);
}

static uint8_t PlayNote(struct trackdata *t, const uint8_t *dat)
{
	const uint8_t *od=dat;
	int16_t ins=-1;
	int16_t nte=-1;
	int16_t portante=-1;
	int16_t vol=t->vol;
	int16_t pan=t->pan;
	int16_t delay=-1;
	uint8_t opt=*dat++;

	if (opt&cmdPlayIns)
		ins=*dat++;
	if (opt&cmdPlayNte)
	{
		if (*dat&0x80)
			portante=*dat++&~0x80;
		else
			nte=*dat++;
	}
	if (opt&cmdPlayVol)
		vol=*dat++;
	if (opt&cmdPlayPan)
	{
		pan=*dat++;
		t->pansrnd=0;
	}
	if (opt&cmdPlayDelay)
		delay=*dat++;

	if (delay!=-1)
	{
		t->fx=fxDelay;
		t->delaycmd=od;
		t->delay=delay;
	}

	if (((delay!=currenttick)||!processtick)&&((delay!=-1)||patdelay))
		return dat-od;

	if (ins!=-1)
	{
		t->selinst=ins;
		if (samiextrawurscht)
			t->insofs=0;
		if (t->sustain||(nte!=-1)||(portante!=-1))
		{
			t->venvpos=t->penvpos=t->pchenvpos=t->vibenvpos=0;
			t->venvfrac=t->penvfrac=t->pchenvfrac=t->vibenvfrac=0;
			t->fadevol=0x8000;
			t->sustain=1;
			t->vibsweeppos=0;
		}
		if (&instruments[t->selinst]==t->instr)
		{
		} else if (portante!=-1)
		{
			if (t->instr&&samiextrawurscht)
				nte=t->nteval;
			if (!t->instr)
				nte=portante;
		}
	}

	if (nte!=-1)
	{
		if ((t->selinst>=instnum)||(instruments[t->selinst].samples[nte]>=modsampnum)||(modsamples[instruments[t->selinst].samples[nte]].handle>=sampnum))
		{
			if (t->phys!=-1)
			{
				mcpSet(t->phys, mcpCReset, 0);
				pchan[t->phys]=-1;
				t->phys=-1;
			}
			t->instr=0;
			t->samp=0;
			nte=-1;
		} else {
			t->instr=&instruments[t->selinst];
			t->samp=&modsamples[t->instr->samples[nte]];
			t->newinst=t->samp->handle;
		}
	}

	if ((opt&cmdPlayIns)&&t->instr&&t->samp/*&&((&instruments[t->selinst]==t->instr)||!samiextrawurscht)*/)
	{
		if ((t->samp->stdvol!=-1)&&!(opt&cmdPlayVol))
			vol=t->samp->stdvol;
		if ((t->samp->stdpan!=-1)&&!(opt&cmdPlayPan))
		{
			pan=t->samp->stdpan;
			t->pansrnd=0;
		}
	}


	if (nte!=-1)
	{
		uint_fast32_t pos=0;

		t->nteval=nte;
		t->notehit=1;
		if (exponential)
			t->pitchslidepitch=t->finalpitch=t->pitch=60*256-(t->nteval<<8)+t->samp->normnote;
		else
			t->pitchslidepitch=t->finalpitch=t->pitch=mcpGetFreq6848(60*256+t->samp->normnote-(t->nteval<<8));
		if (samiextrawurscht&&t->insofs)
			pos=(t->samp->opt&MP_OFFSETDIV2)?(t->insofs>>1):t->insofs;
		t->newpos=pos;
		t->retrigpos=t->trempos=t->arppos=t->pitchvibpos=t->volvibpos=0;
	}

	if (portante!=-1)
	{
		t->nteval=portante;
		if (t->samp)
		{
			if (exponential)
				t->pitchslidepitch=60*256-(t->nteval<<8)+t->samp->normnote;
			else
				t->pitchslidepitch=mcpGetFreq6848(60*256+t->samp->normnote-(t->nteval<<8));
		}
	}

	t->vol=t->finalvol=checkvol(vol);
	t->pan=t->finalpan=checkpan(pan);

	return dat-od;
}

static void PlayGCommand(const uint8_t *cmd, uint8_t len)
{
	const uint8_t *cend=cmd+len;
	while (cmd<cend)
	{
		switch (*cmd++)
		{
			case cmdTempo:
				tempo=*cmd;
				break;
			case cmdSpeed:
				speed=*cmd;
				mcpSet(-1, mcpGSpeed, 256*2*speed/5);
				break;
			case cmdFineSpeed:
				mcpSet(-1, mcpGSpeed, 256*2*(10*speed+*cmd)/50);
				break;
			case cmdBreak:
				if (brkpat==-1)
				{
					brkpat=currentpattern+1;
					if (brkpat==endpat)
					{
						brkpat=looppat;
						looped=1;
					}
				}
				brkrow=*cmd;
				donotshutup=0;
				break;
			case cmdGoto:
				brkpat=*cmd;
				if (brkpat<=currentpattern)
					looped=1;
				/*      brkrow=0; */
				donotshutup=0;
				break;
			case cmdPatLoop:
/*
				if(plLoopPatterns)*/ /*TODO. take this back? */
				{
					if (*cmd)
						if (patloopcount[globchan]++<*cmd)
						{
							brkpat=currentpattern;
							brkrow=patlooprow[globchan];
							donotshutup=1;
						} else {
							patloopcount[globchan]=0;
							patlooprow[globchan]=currentrow+1;
						} else
							patlooprow[globchan]=currentrow;
				}
				break;
			case cmdPatDelay:
				if (!patdelay&&*cmd)
					patdelay=*cmd+1;
				break;
			case cmdGlobVol:
				globalvol=*cmd;
				break;
			case cmdGlobVolSlide:
				if (*cmd)
					globalvolslval[globchan]=*cmd;
				globalvolslide[globchan]=(globalvolslval[globchan]>0)?fxGVSUp:fxGVSDown;
				break;
			case cmdSetChan:
				globchan=*cmd;
				break;
		}
		cmd++;
	}
}

static void PlayCommand(struct trackdata *t, const uint8_t *cmd, uint8_t len)
{
	const uint8_t *cend=cmd+len;
	while (cmd<cend)
	{
		if (*cmd&cmdPlayNote)
		{
			cmd+=PlayNote(t, cmd);
			continue;
		}
		switch (*cmd++)
		{
			case cmdVolSlideUp:
				if (*cmd)
					t->volslideval=*cmd;
				else
					t->volslideval=abs(t->volslideval);
				t->volslide=fxVSUp;
				t->fx=fxVolSlideUp;
				t->lastvolsl=0;
				break;
			case cmdVolSlideDown:
				if (*cmd)
					t->volslideval=-*cmd;
				else
					t->volslideval=-abs(t->volslideval);
				t->volslide=fxVSDown;
				t->fx=fxVolSlideDown;
				t->lastvolsl=0;
				break;
			case cmdRowVolSlideUp:
				if (*cmd)
					t->rowvolslval=*cmd;
				else
					t->rowvolslval=abs(t->rowvolslval);
				t->fx=fxRowVolSlideUp;
				t->vol=t->finalvol=checkvol(t->vol+t->rowvolslval);
				t->lastvolsl=1;
				break;
			case cmdRowVolSlideDown:
				if (*cmd)
					t->rowvolslval=-*cmd;
				else
					t->rowvolslval=-abs(t->rowvolslval);
				t->fx=fxRowVolSlideDown;
				t->vol=t->finalvol=checkvol(t->vol+t->rowvolslval);
				t->lastvolsl=1;
				break;
			case cmdVolSlideUDMF:
				t->volslideval=*cmd;
				t->volslide=fxVSUDMF;
				t->fx=fxVolSlideUp;
				break;
			case cmdVolSlideDDMF:
				t->volslideval=-*cmd;
				t->volslide=fxVSDDMF;
				t->fx=fxVolSlideDown;
				break;
			case cmdRowPanSlide:
				if (*cmd)
					t->rowpanslval=*cmd;
				t->pan=t->finalpan=checkpan(t->pan+t->rowpanslval);
				break;
			case cmdPanSlide:
				if (*cmd)
					t->panslideval=*cmd;
				t->panslide=(t->panslideval<0)?fxPnSLeft:fxPnSRight;
				t->fx=(t->panslideval<0)?fxPanSlideLeft:fxPanSlideRight;
				break;
			case cmdPanSlideLDMF:
				t->panslideval=-(uint16_t)*cmd;
				t->panslide=fxPnSLDMF;
				t->fx=fxPanSlideLeft;
				break;
			case cmdPanSlideRDMF:
				t->panslideval=*cmd;
				t->panslide=fxPnSRDMF;
				t->fx=fxPanSlideRight;
				break;
			case cmdPitchSlideUp:
				if (*cmd)
					t->pitchslideval=(int16_t)*cmd<<4;
				else
					t->pitchslideval=abs(t->pitchslideval);
				t->pitchslide=fxPSUp;
				t->fx=fxPitchSlideUp;
				t->lastpitchsl=0;
				break;
			case cmdPitchSlideDown:
				if (*cmd)
					t->pitchslideval=-(int16_t)*cmd<<4;
				else
					t->pitchslideval=-abs(t->pitchslideval);
				t->pitchslide=fxPSDown;
				t->fx=fxPitchSlideDown;
				t->lastpitchsl=0;
				break;
			case cmdRowPitchSlideUp:
				if (*cmd)
					t->rowpitchslval=*cmd;
				t->fx=fxRowPitchSlideUp;
				t->finalpitch=t->pitch=checkpitch(t->finalpitch-t->rowpitchslval);
				t->lastpitchsl=1;
				break;
			case cmdRowPitchSlideDown:
				if (*cmd)
					t->rowpitchslval=*cmd;
				t->fx=fxRowPitchSlideDown;
				t->finalpitch=t->pitch=checkpitch(t->finalpitch+t->rowpitchslval);
				t->lastpitchsl=1;
				break;
			case cmdPitchSlideUDMF:
				t->fx=fxPitchSlideUp;
				t->pitchslide=fxPSUDMF;
				t->pitchslideval=*cmd<<4;
				break;
			case cmdPitchSlideDDMF:
				t->fx=fxPitchSlideDown;
				t->pitchslide=fxPSDDMF;
				t->pitchslideval=-(int16_t)(*cmd<<4);
				break;
			case cmdRowPitchSlideDMF:
				t->fx=(*cmd&128)?fxRowPitchSlideDown:fxRowPitchSlideUp;
				t->finalpitch=t->pitch=checkpitch(t->finalpitch-((int8_t)*cmd<<1));
				break;
			case cmdPitchSlideToNote:
				t->pitchslide=fxPSToNote;
				t->fx=fxPitchSlideToNote;
				if (*cmd)
					t->pitchslidenteval=((uint16_t)*cmd)<<4;
				break;
			case cmdPitchSlideNDMF:
				t->fx=fxPitchSlideToNote;
				t->pitchslide=fxPSNDMF;
				t->pitchslidenteval=*cmd<<4;
				break;
			case cmdVolVibrato:
				t->volfx=fxVXVibrato;
				t->fx=fxVolVibrato;
				if (*cmd&0x0F)
					t->volvibamp=(*cmd&0x0F)<<2;
				if (*cmd>>4)
					t->volvibspd=(*cmd>>4)<<2;
				break;
			case cmdVolVibratoSinDMF:
			case cmdVolVibratoTrgDMF:
			case cmdVolVibratoRecDMF:
				t->volfx=fxVXVibrato;
				t->fx=fxVolVibrato;
				t->volvibamp=(*cmd&0xF)+1;
				t->volvibspd=(*cmd>>4)+1;
				t->volvibwave=0x20+cmd[-1]-cmdVolVibratoSinDMF;
				break;
			case cmdVolVibratoSetWave:
				switch (*cmd)
				{
					case 0: case 1: case 2: case 0x10: case 0x11: case 0x12:
						t->volvibwave=*cmd;
						break;
					case 3:
						t->volvibwave=rand()%3;
						break;
					case 0x13:
						t->volvibwave=rand()%3+0x10;
						break;
				}
				break;
			case cmdTremor:
				if (*cmd||samiextrawurscht)
					t->tremval=*cmd;
				t->volfx=fxVXTremor;
				t->fx=fxTremor;
				t->tremon=(t->tremval>>4)+1;
				t->tremlen=(t->tremval&0xF)+1+t->tremon;
				break;
			case cmdPitchVibrato:
				t->pitchfx=fxPXVibrato;
				t->fx=fxPitchVibrato;
				if (*cmd&0x0F)
					t->pitchvibamp=(*cmd&0x0F)<<2;
				if (*cmd>>4)
					t->pitchvibspd=(*cmd>>4)<<2;
				break;
			case cmdPitchVibratoSinDMF:
			case cmdPitchVibratoTrgDMF:
			case cmdPitchVibratoRecDMF:
				t->pitchfx=fxPXVibrato;
				t->fx=fxPitchVibrato;
				t->pitchvibamp=(*cmd&0xF)+1;
				t->pitchvibspd=(*cmd>>4)+1;
				t->pitchvibwave=0x20+cmd[-1]-cmdPitchVibratoSinDMF;
				break;
			case cmdPitchVibratoFine:
				t->pitchfx=fxPXVibrato;
				t->fx=fxPitchVibrato;
				if (*cmd&0x0F)
					t->pitchvibamp=*cmd&0x0F;
				if (*cmd>>4)
					t->pitchvibspd=(*cmd>>4)<<2;
				break;
			case cmdPitchVibratoSetSpeed:
				if (*cmd&0x0F)
					t->pitchvibspd=*cmd<<2;
				break;
			case cmdPitchVibratoSetWave:
				switch (*cmd)
				{
					case 0: case 1: case 2: case 0x10: case 0x11: case 0x12:
						t->pitchvibwave=*cmd;
						break;
					case 3:
						t->pitchvibwave=rand()%3;
						break;
					case 0x13:
						t->pitchvibwave=rand()%3+0x10;
						break;
				}
				break;
			case cmdArpeggio:
				t->pitchfx=fxPXArpeggio;
				t->fx=fxArpeggio;
				if (*cmd)
					t->arpval=*cmd;
				t->arpnte[0]=0;
				t->arpnte[1]=t->arpval>>4;
				t->arpnte[2]=t->arpval&0x0F;
				break;
			case cmdPanSurround:
				t->pansrnd=1;
				break;
			case cmdKeyOff:
				t->sustain=0;
				break;
			case cmdSetEnvPos:
				t->venvpos=t->penvpos=t->pchenvpos=t->vibenvpos=*cmd;
				if (t->samp->volenv<envnum)
					if (t->venvpos>envelopes[t->samp->volenv].len)
						t->venvpos=envelopes[t->samp->volenv].len;
				if (t->samp->panenv<envnum)
					if (t->penvpos>envelopes[t->samp->panenv].len)
						t->penvpos=envelopes[t->samp->panenv].len;
				if (t->samp->pchenv<envnum)
					if (t->pchenvpos>envelopes[t->samp->pchenv].len)
						t->pchenvpos=envelopes[t->samp->pchenv].len;
				/*
				   if (t.samp->vibenv<envnum)
				   if (t.vibenvpos>envelopes[t.samp->vibenv].len)
				   t.vibenvpos=envelopes[t.samp->vibenv].len;
				 */
				break;
			case cmdNoteCut:
				t->notefx=fxNXNoteCut;
				t->fx=fxNoteCut;
				t->cuttick=*cmd;
				break;
			case cmdRetrig:
				if (*cmd)
				{
					t->retrig=*cmd;
					t->retrigpos=0;
				}
				t->notefx=fxNXRetrig;
				t->fx=fxRetrig;
				break;
			case cmdOffsetHigh:
				t->ofshigh=*cmd;
				break;
			case cmdOffset:
				t->fx=fxOffset;
				if (!samiextrawurscht || t->notehit)
				{
					if (*cmd|t->ofshigh)
					{
						t->ofs=(*cmd<<8)|(t->ofshigh<<16);
						t->ofshigh=0;
					}
					t->insofs=t->ofs;
					t->newpos=(t->samp->opt&MP_OFFSETDIV2)?(t->ofs>>1):t->ofs;
				}
				break;
			case cmdOffsetEnd:
				t->fx=fxOffset;
				if (*cmd|t->ofshigh)
				{
					t->ofs=(*cmd<<8)|(t->ofshigh<<16);
					t->ofshigh=0;
				}
				t->insofs=t->ofs;
				t->newpos=(t->samp->opt&MP_OFFSETDIV2)?(t->ofs>>1):t->ofs;
				t->newposend=1;
				t->newdir=1;
				break;
			case cmdPanVibratoSinDMF:
				t->panfx=fxPnXVibrato;
				t->fx=fxPanVibrato;
				t->panvibamp=(*cmd&0xF)+1;
				t->panvibspd=(*cmd>>4)+1;
				t->panvibwave=0x20;
				break;
			case cmdPanHeight:
				t->pany=*cmd-0x80;
				break;
			case cmdPanDepth:
				t->panz=*cmd-0x80;
				break;
			case cmdChannelVol:
				t->chanvol=*cmd;
				break;
			case cmdSetDir:
				t->newdir=*cmd;
				break;
			case cmdSetLoop:
				t->newloop=*cmd;
				break;
			case cmdSpecial:
				switch (*cmd)
				{
					case cmdContVolSlide:
						t->volslide=(t->volslideval<0)?fxVSDown:fxVSUp;
						t->fx=(t->volslideval<0)?fxVolSlideDown:fxVolSlideUp;
						break;
					case cmdContRowVolSlide:
						t->fx=(t->rowvolslval<0)?fxRowVolSlideDown:fxRowVolSlideUp;
						t->vol=t->finalvol=checkvol(t->vol+t->rowvolslval);
						break;
					case cmdContMixVolSlide:
						if (!t->lastvolsl)
						{
							t->volslide=(t->volslideval<0)?fxVSDown:fxVSUp;
							t->fx=(t->volslideval<0)?fxVolSlideDown:fxVolSlideUp;
						} else {
							t->fx=(t->rowvolslval<0)?fxRowVolSlideDown:fxRowVolSlideUp;
							t->vol=t->finalvol=checkvol(t->vol+t->rowvolslval);
						}
						break;
					case cmdContMixVolSlideUp:
						if (!t->lastvolsl)
						{
							t->volslideval=abs(t->volslideval);
							t->volslide=fxVSUp;
							t->fx=fxVolSlideUp;
						} else {
							t->rowvolslval=abs(t->rowvolslval);
							t->fx=fxRowVolSlideUp;
							t->vol=t->finalvol=checkvol(t->vol+t->rowvolslval);
						}
						break;
					case cmdContMixVolSlideDown:
						if (!t->lastvolsl)
						{
							t->volslideval=-abs(t->volslideval);
							t->volslide=fxVSDown;
							t->fx=fxVolSlideDown;
						} else {
							t->rowvolslval=-abs(t->rowvolslval);
							t->fx=fxRowVolSlideDown;
							t->vol=t->finalvol=checkvol(t->vol+t->rowvolslval);
						}
						break;
					case cmdContMixPitchSlideUp:
						if (!t->lastpitchsl)
						{
							t->pitchslideval=abs(t->pitchslideval);
							t->pitchslide=fxPSUp;
							t->fx=fxPitchSlideUp;
						} else {
							t->fx=fxRowPitchSlideUp;
							t->finalpitch=t->pitch=checkpitch(t->finalpitch-t->rowpitchslval);
						}
						break;
					case cmdContMixPitchSlideDown:
						if (!t->lastpitchsl)
						{
							t->pitchslideval=-abs(t->pitchslideval);
							t->pitchslide=fxPSDown;
							t->fx=fxPitchSlideDown;
						} else {
							t->fx=fxRowPitchSlideDown;
							t->finalpitch=t->pitch=checkpitch(t->finalpitch+t->rowpitchslval);
						}
						break;
					case cmdGlissOn:
						t->glissando=1;
						break;
					case cmdGlissOff:
						t->glissando=0;
						break;
				}
		}
		cmd++;
	}
}

static void DoGCommand(void)
{
	int i;
	for (i=0; i<MAXLCHAN; i++)
		if (globalvolslide[i])
			if (processtick)
				globalvol=checkvol(globalvol+globalvolslval[i]);
}

static void DoCommand(struct trackdata *t)
{
	if (t->delay==currenttick)
		PlayNote(t, t->delaycmd);

	switch (t->volslide)
	{
		case 0:
			break;
		case fxVSUp: case fxVSDown:
			if (processtick||samisami)
				t->vol=t->finalvol=checkvol(t->vol+t->volslideval);
			break;
		case fxVSUDMF: case fxVSDDMF:
			t->vol=t->finalvol=checkvol(t->vol+t->volslideval*(currenttick+1)/tempo-t->volslideval*currenttick/tempo);
			break;
	}
	switch (t->panslide)
	{
		case 0:
			break;
		case fxPnSLeft: case fxPnSRight:
			if (processtick)
				t->pan=t->finalpan=checkpan(t->pan+t->panslideval);
			break;
		case fxPnSLDMF: case fxPnSRDMF:
			t->pan=t->finalpan=checkpan(t->pan+t->panslideval*(currenttick+1)/tempo-t->panslideval*currenttick/tempo);
			break;
	}
	switch (t->pitchslide)
	{
		case 0:
			break;
		case fxPSUp: case fxPSDown:
			if (processtick)
			{
				if (samiextrawurscht&&((t->pitch-t->pitchslideval)<0))
					t->stopchan=1;
				t->finalpitch=t->pitch=checkpitch(t->pitch-t->pitchslideval);
			}
			break;
		case fxPSToNote:
			if (!t->samp)
				return;
			if (processtick)
			{
				if (t->pitch<t->pitchslidepitch)
				{
					if ((t->pitch+=t->pitchslidenteval)>t->pitchslidepitch)
						t->pitch=t->pitchslidepitch;
				} else {
					if ((t->pitch-=t->pitchslidenteval)<t->pitchslidepitch)
						t->pitch=t->pitchslidepitch;
				}
				t->pitch=checkpitch(t->pitch);
			}
			if (t->glissando)
				if (exponential)
					t->finalpitch=((t->pitch+0x80-t->samp->normnote)&~0xFF)+t->samp->normnote;
				else
					t->finalpitch=mcpGetFreq6848(((mcpGetNote6848(t->pitch)+0x80-t->samp->normnote)&~0xFF)+t->samp->normnote);
			else
				t->finalpitch=t->pitch;
			break;
		case fxPSUDMF: case fxPSDDMF:
			t->finalpitch=t->pitch=checkpitch(t->pitch-t->pitchslideval*(currenttick+1)/tempo+t->pitchslideval*currenttick/tempo);
			break;
		case fxPSNDMF:
			{
				uint16_t delta;
				delta=t->pitchslidenteval*(currenttick+1)/tempo-t->pitchslidenteval*currenttick/tempo;
				if (t->pitch<t->pitchslidepitch)
				{
					if ((t->pitch+=delta)>t->pitchslidepitch)
						t->pitch=t->pitchslidepitch;
				} else {
					if ((t->pitch-=delta)<t->pitchslidepitch)
						t->pitch=t->pitchslidepitch;
				}
				t->pitch=t->finalpitch=checkpitch(t->pitch);
				break;
			}
	}

	switch (t->volfx)
	{
		case fxVXVibrato:
			switch (t->volvibwave)
			{
				case 0:
					t->finalvol=checkvol(t->vol+((sintab[t->volvibpos]*t->volvibamp)>>9));
					break;
				case 1:
					t->finalvol=checkvol(t->vol+(((128-t->volvibpos)*t->volvibamp/4)>>3));
					break;
				case 2:
					t->finalvol=checkvol(t->vol+t->volvibamp*((t->volvibpos&128)?-4:4));
					break;
				case 0x10:
					t->finalvol=checkvol(t->vol+((sintab[t->volvibpos]*t->volvibamp)>>10));
					break;
				case 0x11:
					t->finalvol=checkvol(t->vol+((((int16_t)t->volvibpos-128)*t->volvibamp/4)>>4));
					break;
				case 0x12:
					t->finalvol=checkvol(t->vol+t->volvibamp*((t->volvibpos&128)?0:2));
					break;
			}
			if (processtick)
				t->volvibpos=t->volvibpos+t->volvibspd;
			break;
		case fxVXTremor:
			t->finalvol=(t->trempos<t->tremon)?t->vol:0;
			if (processtick||samiextrawurscht)
				t->trempos=(t->trempos+1)%t->tremlen;
			break;
	}

	switch (t->pitchfx)
	{
		case fxPXVibrato:
			if (t->pitchvibwave>=0x20)
			{
				uint8_t vpos=256*(t->pitchvibpos*tempo+currenttick)/(tempo*t->pitchvibspd);
				switch (t->pitchvibwave)
				{
					case 0x20:
						t->finalpitch=checkpitch(t->finalpitch-((sintab[vpos]*t->pitchvibamp)>>6));
						break;
				}
				if ((currenttick+1)==tempo)
				{
					t->pitchvibpos++;
					if (t->pitchvibpos==t->pitchvibspd)
						t->pitchvibpos=0;
				}
				break;
			}
			switch (t->pitchvibwave)
			{
				case 0:
					t->finalpitch=checkpitch(t->finalpitch+((sintab[t->pitchvibpos]*t->pitchvibamp)>>8));
					break;
				case 1:
					t->finalpitch=checkpitch(t->finalpitch-(128-t->pitchvibpos)*t->pitchvibamp/16);
					break;
				case 2:
					t->finalpitch=checkpitch(t->finalpitch-t->pitchvibamp*((t->pitchvibpos&128)?8:-8));
					break;
				case 0x10:
					t->finalpitch=checkpitch(t->finalpitch+((sintab[t->pitchvibpos]*t->pitchvibamp)>>8));
					break;
				case 0x11:
					t->finalpitch=checkpitch(t->finalpitch+((int16_t)t->pitchvibpos-128)*t->pitchvibamp/16);
					break;
				case 0x12:
					t->finalpitch=checkpitch(t->finalpitch+t->pitchvibamp*((t->pitchvibpos&128)?0:8));
					break;
			}
			if (processtick)
				t->pitchvibpos=t->pitchvibpos+t->pitchvibspd;
			break;
		case fxPXArpeggio:
			if (exponential)
				t->finalpitch=checkpitch(t->finalpitch-t->arpnte[t->arppos]*256);
			else
				t->finalpitch=checkpitch(t->finalpitch*notetab[t->arpnte[t->arppos]]/32768);
			t->arppos=(t->arppos+1)%3;
			break;
	}

	switch (t->notefx)
	{
		case fxNXNoteCut:
			if (currenttick==t->cuttick)
				t->vol=t->finalvol=0;
			break;
		case fxNXRetrig:
			if (t->retrigpos==(t->retrig&0x0F))
			{
				t->retrigpos=0;
				t->newpos=0;
				switch (t->retrig>>4)
				{
					case 1: case 2: case 3: case 4: case 5:
						t->vol=t->finalvol=checkvol(t->finalvol-(4<<((t->retrig>>4)-1)));
						break;
					case 6:
						t->vol=t->finalvol=checkvol(t->finalvol*5/8);  /* s3m only? */
						break;
					case 7:
						t->vol=t->finalvol=checkvol(t->finalvol/2);
						break;
					case 9: case 10: case 11: case 12: case 13:
						t->vol=t->finalvol=checkvol(t->finalvol+(4<<((t->retrig>>4)-9)));
						break;
					case 14:
						t->vol=t->finalvol=checkvol(t->finalvol*3/2);
						break;
					case 15:
						t->vol=t->finalvol=checkvol(t->finalvol*2);
						break;
				}
			}
			t->retrigpos++;
			break;
	}
}

static void putque(int time, int type, int val1, int val2)
{
	if (((quewpos+1)%quelen)==querpos)
		return;
	que[quewpos][0]=time;
	que[quewpos][1]=type;
	que[quewpos][2]=val1;
	que[quewpos][3]=val2;
	quewpos=(quewpos+1)%quelen;
}

static void PlayTick(void)
{
	struct trackdata *td;
	int i;

	int cmdtime;

	if (!physchan)
		return;

	for (i=0; i<physchan; i++)
		if (!mcpGet(i, mcpCStatus))
			if (pchan[i]!=-1)
			{
				mcpSet(i, mcpCReset, 0);
				tdata[pchan[i]].phys=-1;
				pchan[i]=-1;
			}

	for (td=tdata; td<tdataend; td++)
	{
		td->finalvol=td->vol;
		td->finalpan=td->pan;
		td->finalpitch=td->pitch;
		td->newpos=(uint_fast32_t)-1;
		td->newposend=0;
		td->newdir=-1;
		td->newloop=-1;
		td->stopchan=0;
		td->newinst=-1;
	}

	currenttick++;
	if (currenttick>=tempo)
		currenttick=0;

	if (!currenttick&&patdelay)
	{
		brkpat=currentpattern;
		brkrow=currentrow;
		/*    patdelay--; */
	}

	processtick=newtickmode||currenttick||patdelay;

	if (!currenttick/*&&!patdelay*/)
	{
		currenttick=0;

		currentrow++;

		if ((currentrow>=patternlen)&&(brkpat==-1))
		{
			brkpat=currentpattern+1;
			donotshutup=0;
			if (brkpat==endpat)
			{
				looped=1;
				brkpat=looppat;
			}
			brkrow=0;
		}
		if (brkpat!=-1)
		{
			if (currentpattern!=brkpat)
			{
				if (lockpattern!=-1)
				{
					if (brkpat!=lockpattern)
						brkrow=0;
					brkpat=lockpattern;
					donotshutup=1;
				}
				memset(patloopcount, 0, sizeof(patloopcount));
				memset(patlooprow, 0, sizeof(patlooprow));
			}
			currentpattern=brkpat;
			currentrow=brkrow;
			brkpat=-1;
			brkrow=0;
			while ((currentpattern<patternnum)&&(orders[currentpattern]==0xFFFF))
				currentpattern++;
			if ((currentpattern>=patternnum)||(currentpattern==endpat))
			{
				currentpattern=looppat;
				looped=1;
			}
			if (!currentpattern&&!currentrow&&!patdelay&&!donotshutup)
			{
				currentpattern=0;
				currentrow=0;
				for (i=0; i<channels; i++)
				{
					int mute=tdata[i].mute;
					memset(&tdata[i], 0, sizeof(*tdata));
					tdata[i].mute=mute;
					tdata[i].num=i;
					tdata[i].chanvol=0xFF;
					tdata[i].finalpan=tdata[i].pan=(i&1)?255:0;
					tdata[i].newpos=(uint_fast32_t)-1;
					tdata[i].newloop=-1;
					tdata[i].newdir=-1;
					tdata[i].newinst=-1;
					tdata[i].phys=-1;
				}
				for (i=0; i<physchan; i++)
				{
					mcpSet(i, mcpCReset, 0);
					pchan[i]=-1;
				}
				tempo=6;
				speed=125;
				globalvol=0xFF;
				mcpSet(-1, mcpGSpeed, 12800);
			}
			LoadPattern(currentpattern, currentrow);
		}

		memset(globalvolslide, 0, sizeof(globalvolslide));

		for (td=tdata; td<tdataend; td++)
		{
			struct gmdtrack *t;
			td->notehit=0;
			td->volslide=0;
			td->pitchslide=0;
			td->panslide=0;
			td->pitchfx=0;
			td->volfx=0;
			td->panfx=0;
			td->notefx=0;
			td->delay=-1;
			td->fx=0;

			t=&td->trk;
			while (1)
			{
				if (t->ptr>=t->end)
					break;
				if (t->ptr[0]!=currentrow)
					break;
				PlayCommand(td, t->ptr+2, t->ptr[1]);
				t->ptr+=t->ptr[1]+2;
			}
		}

		while (1)
		{
			if (gtrack.ptr>=gtrack.end)
				break;
			if (gtrack.ptr[0]!=currentrow)
				break;
			PlayGCommand(gtrack.ptr+2, gtrack.ptr[1]);
			gtrack.ptr+=gtrack.ptr[1]+2;
		}

		if (patdelay)
			patdelay--;
	}

	DoGCommand();
	for (td=tdata; td<tdataend; td++)
		DoCommand(td);

	for (td=tdata; td<tdataend; td++)
	{
		int16_t vol, pan;
		/* const struct gmdinstrument *f; */
		const struct gmdsample *fs;
		if (!td->instr)
			continue;
		if (!td->samp)
			continue;
		vol=(td->finalvol*globalvol)>>8;
		pan=td->finalpan-0x80;
		/* f=&*td->instr; */
		fs=&*td->samp;

		if (!td->sustain&&fs->volfade)
		{
			vol=(vol*td->fadevol)>>15;
			if (td->fadevol>=fs->volfade)
				td->fadevol-=fs->volfade;
			else
				td->fadevol=0;
		}

		if (fs->volenv<envnum)
		{
			const struct gmdenvelope *env=&envelopes[fs->volenv];
			vol=(env->env[td->venvpos]*vol)>>8;

			if (!env->speed||(env->speed==speed))
				td->venvfrac+=65536;
			else
				td->venvfrac+=env->speed*65536/speed;

			while (td->venvfrac>=65536)
			{
				if (td->venvpos<env->len)
					td->venvpos++;
				if (td->sustain&&(env->type&mpEnvSLoop))
				{
					if (td->venvpos==env->sloope)
						td->venvpos=env->sloops;
				} else if (env->type&mpEnvLoop)
				{
					if (td->venvpos==env->loope)
						td->venvpos=env->loops;
				}
				td->venvfrac-=65536;
			}
		}
		if (fs->panenv<envnum)
		{
			const struct gmdenvelope *env=&envelopes[fs->panenv];
			pan+=((env->env[td->penvpos]-128)*(128-abs(pan)))>>7;

			if (!env->speed||(env->speed==speed))
				td->penvfrac+=65536;
			else
				td->penvfrac+=env->speed*65536/speed;

			while (td->penvfrac>=65536)
			{
				if (td->penvpos<env->len)
					td->penvpos++;
				if (td->sustain&&(env->type&mpEnvSLoop))
				{
					if (td->penvpos==env->sloope)
						td->penvpos=env->sloops;
				} else if (env->type&mpEnvLoop)
				{
					if (td->penvpos==env->loope)
						td->penvpos=env->loops;
				}
				td->penvfrac-=65536;
			}
		}

		if (fs->pchenv<envnum)
		{
			const struct gmdenvelope *env=&envelopes[fs->pchenv];
			int16_t dep=((env->env[td->pchenvpos]-128)<<fs->pchint)>>1;

			if (expopitchenv&&!exponential)
				td->finalpitch=checkpitch(umuldiv(td->finalpitch, mcpGetFreq8363(dep), 8363));
			else
				td->finalpitch=checkpitch(td->finalpitch-dep);

			if (!env->speed||(env->speed==speed))
				td->pchenvfrac+=65536;
			else
				td->pchenvfrac+=env->speed*65536/speed;

			while (td->pchenvfrac>=65536)
			{
				if (td->pchenvpos<env->len)
					td->pchenvpos++;
				if (td->sustain&&(env->type&mpEnvSLoop))
				{
					if (td->pchenvpos==env->sloope)
						td->pchenvpos=env->sloops;
				} else if (env->type&mpEnvLoop)
				{
					if (td->pchenvpos==env->loope)
						td->pchenvpos=env->loops;
				}
				td->pchenvfrac-=65536;
			}
		}
		if (fs->vibrate&&fs->vibdepth)
		{
			int dep=0;
			switch (fs->vibtype)
			{
				case 0:
					dep=(sintab[(td->vibenvpos>>8)&0xFF]*fs->vibdepth)>>11;
					break;
				case 1:
					dep=(td->vibenvpos&0x8000)?-fs->vibdepth:fs->vibdepth;
					break;
				case 2:
					dep=(fs->vibdepth*(32768-td->vibenvpos))>>14;
					break;
				case 3:
					dep=(fs->vibdepth*(td->vibenvpos-32768))>>14;
					break;
			}

			td->vibsweeppos+=fs->vibsweep;
			if (td->vibsweeppos<0) /* currently not needed, check is compiled out anyways */
				td->vibsweeppos=0;
			if (td->vibsweeppos>0x10000)
				td->vibsweeppos=0x10000;
			dep=(dep*td->vibsweeppos)>>16;

			if (expopitchenv&&!exponential)
				td->finalpitch=checkpitch(umuldiv(td->finalpitch, mcpGetFreq8363(dep), 8363));
			else
				td->finalpitch=checkpitch(td->finalpitch-dep);

			if (!fs->vibspeed||(fs->vibspeed==speed))
				td->vibenvpos+=fs->vibrate;
			else
				td->vibenvpos+=fs->vibspeed*fs->vibrate/speed;
		}
      /*
	if (fs.vibenv!=0xFFFF)
	{
	const envelope &env=envelopes[fs.vibenv];
	signed short dep=((env.env[td->vibenvpos]-128)<<fs.vibint)>>1;
	if (td->vibsweeppos!=fs.vibswp)
        dep=dep*td->vibsweeppos/fs.vibswp;

	if (expopitchenv&&!exponential)
        td->finalpitch=checkpitch(umuldiv(td->finalpitch, mcpGetFreq8363(dep), 8363));
	else
        td->finalpitch=checkpitch(td->finalpitch-dep);

	if (!env.speed||(env.speed==speed))
        td->vibenvfrac+=65536;
	else
        td->vibenvfrac+=env.speed*65536/speed;

	while (td->vibenvfrac>=65536)
	{
        if (td->vibsweeppos!=fs.vibswp)
	td->vibsweeppos++;
        if ((td->vibenvpos<env.len)&&((td->vibenvpos!=env.sustain)||!td->sustain))
	td->vibenvpos++;
        if ((td->vibenvpos==env.loope)&&(td->sustain||!(env.opt&1)))
	td->vibenvpos=env.loops;
        td->vibenvfrac-=65536;
	}
	}
      */

		if (gusvol)
		{
			if (vol>0xEF)
				vol=0xFF;
			else
				if (vol<0)
					vol=0;
				else
					if (vol>=0xB0)
						vol=((vol&0xF)|0x10)<<((vol>>4)-0xB);
					else
						vol=((vol&0xF)|0x10)>>(0xB-(vol>>4));
		}
		vol=(vol*td->chanvol)>>8;

		if (td->newinst!=-1)
			if (td->phys!=-1)
				mcpSet(td->phys, mcpCInstrument, td->newinst);
		if (td->newpos!=(uint_fast32_t)-1)
		{
			uint_fast32_t l;
			const struct sampleinfo *sm;
			if (td->phys==-1)
			{
				for (i=0; i<physchan; i++)
					if (pchan[i]==-1)
						break;
				if (i==physchan)
					i=rand()%physchan;
				if (pchan[i]!=-1)
					tdata[pchan[i]].phys=-1;
				pchan[i]=td->num;
				td->phys=i;
				mcpSet(td->phys, mcpCReset, 0);
				mcpSet(td->phys, mcpCInstrument, td->samp->handle);
			}
			sm=&sampleinfos[td->samp->handle];
			l=sm->length;
			if (sm->type&mcpSampRedRate4)
				l>>=2;
			else if (sm->type&mcpSampRedRate2)
				l>>=1;
			if (td->newposend)
				td->newpos=l-td->newpos;
			if (td->newpos>=l)
				td->newpos=l-16;

			mcpSet(td->phys, mcpCPosition, td->newpos);
			mcpSet(td->phys, mcpCLoop, 1);
			mcpSet(td->phys, mcpCDirect, 0);
			mcpSet(td->phys, mcpCStatus, 1);
		}
		if (td->newdir!=-1)
			mcpSet(td->phys, mcpCDirect, td->newdir);
		if (td->newloop!=-1)
			mcpSet(td->phys, mcpCLoop, td->newloop);
		if (td->phys!=-1)
		{
			if (td->stopchan)
				mcpSet(td->phys, mcpCStatus, 0);
			mcpSet(td->phys, mcpCVolume, (donotloopmodule&&looped)?0:vol);
			mcpSet(td->phys, mcpCPanning, pan);
			mcpSet(td->phys, mcpCPanY, td->pany);
			mcpSet(td->phys, mcpCPanZ, td->panz);
			mcpSet(td->phys, mcpCSurround, td->pansrnd);
			if (exponential)
				mcpSet(td->phys, mcpCPitch, -checkpitche(td->finalpitch));
			else
				mcpSet(td->phys, mcpCPitch6848, checkpitchh(td->finalpitch));
			mcpSet(td->phys, mcpCMute, td->mute);
		}
	}
	readque();
	cmdtime=mcpGet(-1, mcpGCmdTimer);
	putque(cmdtime, -1, (currentrow<<8)|(currentpattern<<16), 0);
}

char __attribute__ ((visibility ("internal"))) mpPlayModule(const struct gmdmodule *m)
{
	int i;
	for (i=65; i<=128; i++)
		sintab[i]=sintab[128-i];
	for (i=129; i<256; i++)
		sintab[i]=-sintab[256-i];

	if (m->orders[0]==0xFFFF)
		return 0;

	sampleinfos=m->samples;
	modsampnum=m->modsampnum;
	sampnum=m->sampnum;
	lockpattern=-1;
	patterns=m->patterns;
	orders=m->orders;
	envelopes=m->envelopes;
	instruments=m->instruments;
	instnum=m->instnum;
	modsamples=m->modsamples;
	patternnum=m->ordnum;
	channels=m->channum;
	envnum=m->envnum;
	tdataend=tdata+channels;
	tracks=m->tracks;
	looppat=(m->loopord<m->ordnum)?m->loopord:0;
	while (m->orders[looppat]==0xFFFF)
		looppat--;

	endpat=m->endord;
	samiextrawurscht=!!(m->options&MOD_S3M);
	samisami=!!(m->options&MOD_S3M30);
	newtickmode=!!(m->options&MOD_TICK0);
	exponential=!!(m->options&MOD_EXPOFREQ);
	gusvol=!!(m->options&MOD_GUSVOL);
	expopitchenv=!!(m->options&MOD_EXPOPITCHENV);
	donotshutup=0;

	tempo=6;
	patdelay=0;
	patternlen=0;
	currenttick=tempo;
	currentrow=0;
	currentpattern=0;
	looped=0;
	brkpat=0;
	brkrow=0;
	speed=125;
	globalvol=0xFF;
	realpos=0;

	for (i=0; i<channels; i++)
	{
		tdata[i].phys=-1;
		tdata[i].mute=0;
	}
	memset(pchan, -1, sizeof(pchan));

	quelen=100;
	que=malloc(sizeof(int)*quelen*4);
	if (!que)
		return 0;
	querpos=0;
	quewpos=0;

	if (!mcpOpenPlayer(channels, PlayTick))
		return 0;

	physchan=mcpNChan;

	return 1;
}

void __attribute__ ((visibility ("internal"))) mpStopModule(void)
{
	int i;
	for (i=0; i<physchan; i++)
		mcpSet(i, mcpCReset, 0);
	mcpClosePlayer();
	free(que);
}

void __attribute__ ((visibility ("internal"))) mpGetChanInfo(uint8_t ch, struct chaninfo *ci)
{
	const struct trackdata *t=&tdata[ch];
	ci->ins=0xFF;
	ci->smp=0xFFFF;
	if (t->instr)
	{
		if (t->samp)
			ci->smp=t->samp-modsamples;
		ci->ins=t->instr-instruments;
	}
	ci->note=t->nteval;
	ci->vol=t->vol;
	if (!t->fadevol)
		ci->vol=0;
	ci->pan=t->pan;
	ci->notehit=t->notehit;
	ci->volslide=t->volslide;
	ci->pitchslide=t->pitchslide;
	ci->panslide=t->panslide;
	ci->volfx=t->volfx;
	ci->pitchfx=t->pitchfx;
	ci->notefx=t->notefx;
	ci->fx=t->fx;
}



uint16_t __attribute__ ((visibility ("internal"))) mpGetRealNote(uint8_t ch)
{
	struct trackdata *td=&tdata[ch];
	if (exponential)
		return 60*256+td->samp->normnote-checkpitche(td->finalpitch);
	else
		return 60*256+td->samp->normnote+mcpGetNote8363(6848*8363/checkpitchh(td->finalpitch));
  /*    return td.nteval<<8; */
}

void __attribute__ ((visibility ("internal"))) mpGetGlobInfo(struct globinfo *gi)
{
	int i;

	gi->speed=speed;
	gi->curtick=currenttick;
	gi->tempo=tempo;
	gi->currow=currentrow;
	gi->patlen=patternlen;
	gi->curpat=currentpattern;
	gi->patnum=patternnum;
	gi->globvol=globalvol;
	gi->globvolslide=0;
	for (i=0; i<MAXLCHAN; i++)
		if (globalvolslide[i])
			gi->globvolslide=globalvolslide[i];
}

void __attribute__ ((visibility ("internal"))) mpGetPosition(uint16_t *pat, uint8_t *row)
{
	*pat=currentpattern;
	*row=currentrow;
}

int __attribute__ ((visibility ("internal"))) mpGetRealPos(void)
{
	readque();
	return realpos;
}

void __attribute__ ((visibility ("internal"))) mpSetPosition(int16_t pat, int16_t row)
{
	unsigned int i;
	if (row<0)
		pat--;
	if (pat<0)
	{
		pat=0;
		row=0;
	}
	if (pat>=patternnum)
	{
		pat=looppat;
		row=0;
	}
	if (row<0)
	{
		while (orders[pat]==0xFFFF)
			pat--;
		row+=patterns[orders[pat]].patlen;
		if (row<0)
			row=0;
	}
	while ((pat<patternnum)&&(orders[pat]==0xFFFF))
		pat++;
	if (pat>=patternnum)
	{
		pat=looppat;
		row=0;
	}
	if (row>patterns[orders[pat]].patlen)
	{
		pat++;
		row=0;
		if (pat>=patternnum)
			pat=looppat;
	}
	if (pat!=currentpattern)
	{
		if (lockpattern!=-1)
			lockpattern=pat;
		for (i=0; i<physchan; i++)
		{
			mcpSet(i, mcpCReset, 0);
			pchan[i]=-1;
		}
		for (i=0; i<channels; i++)
			tdata[i].phys=-1;
	}
	donotshutup=0;
	patdelay=0;
	brkpat=pat;
	brkrow=row;
	currentpattern=pat;
	currentrow=row;
	currenttick=tempo;
}

char __attribute__ ((visibility ("internal"))) mpLooped(void)
{
	return looped;
}

void __attribute__ ((visibility ("internal"))) mpSetLoop(uint8_t s)
{
	donotloopmodule=!s;
}

void __attribute__ ((visibility ("internal"))) mpLockPat(int st)
{
	if (st)
		lockpattern=currentpattern;
	else
		lockpattern=-1;
}

int __attribute__ ((visibility ("internal"))) mpGetChanSample(unsigned int ch, int16_t *buf, unsigned int len, uint32_t rate, int opt)
{
	if (tdata[ch].phys==-1)
	{
		memset(buf, 0, len*2);
		return 1;
	}
	return mcpGetChanSample(tdata[ch].phys, buf, len, rate, opt);
}

void __attribute__ ((visibility ("internal"))) mpMute(int ch, int mute)
{
	tdata[ch].mute=mute;
	if (tdata[ch].phys!=-1)
		mcpSet(tdata[ch].phys, mcpCMute, mute);
}

int __attribute__ ((visibility ("internal"))) mpGetMute(int ch)
{
	return tdata[ch].mute;
}

int __attribute__ ((visibility ("internal"))) mpGetChanStatus(int ch)
{
	if (tdata[ch].phys==-1)
		return 0;
	return mcpGet(tdata[ch].phys, mcpCStatus);
}

void __attribute__ ((visibility ("internal"))) mpGetRealVolume(int ch, int *l, int *r)
{
	if (tdata[ch].phys==-1)
	{
		*l=*r=0;
		return;
	}
	mcpGetRealVolume(tdata[ch].phys, l, r);
}

int __attribute__ ((visibility ("internal"))) mpLoadSamples(struct gmdmodule *m)
{
	return mcpLoadSamples(m->samples, m->sampnum);
}
