/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlayer - player for Impulse Tracker 2.xx modules
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
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -fixed many many uncountable small replay bugs
 *    -strange division by zero on song start fixed (occurred when
 *     the player irq/timer routine started before it was supposed
 *     to do so)
 *    -vastly improved channel allocation (uncontrollably vanishing
 *     channels should belong to the past)
 *    -implemented filter handling (not used tho, as the wavetable
 *     system isn't able to handle them, so it's only for playing
 *     filtered ITs right (pitch envelopes dont get messed up anymore))
 *    -implemented exit on song loop function
 *    -added many many various structures, data types, variables and
 *     functions for the player's display
 *    -again fixed even more uncountable replay bugs (a BIG thanx to
 *     mindbender who didn't only reveal two of them, but made it
 *     possible that around 5 other bugs suddenly appeared after
 *     fixing :)
 *    -finally implemented Amiga frequency table on heavy public
 *     demand (or better: cut/pasted/modified it from the xm player -
 *     be happy with this or not)
 *     (btw: "heavy public demand" == Mindbender again ;))
 *    -meanwhile, fixed so many replay bugs again that i'd better have
 *     written this player from scratch ;)
 *    -rewritten envelope handling (envelope loops weren't correct and
 *     the routine crashed on some ITs (due to a bug in pulse's saving
 *     routine i think)
 *    -fixed replay bugs again (thx to maxx)
 *    -and again fixed replay bugs (slowly, this is REALLY getting on
 *     my nerves... grmMRMMRMRMRMROARRMRMrmrararharhhHaheaheaheheaHEAHE)
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -now plays correctly when first order pos contains "+++"
 *    -vibrato tables fixed (were backwards according to some people)
 *    -too fast note retrigs fixed
 *  -ryg990426  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -bla. again applied some SIMPLE bugfix from kb. this sucks :)
 *  -kb990606 Tammo Hinrichs <opencp@gmx.net>
 *    -bugfixed instrument filter settings
 *    -added filter calls to the mcp
 *  -doj20020901 Dirk Jagdmann <doj@cubic.org>
 *    -enable/disable pattern looping
 *  -ss20040908  Stian Skjelstad <stian@nixia.no>
 *    -copy 5 bytes instead of sizeof(c->delayed) in playtick(). We don't want to touch unmapped memory
 */


/* to do:
 * - midi command parsing
 * - filter envelopes still to be tested
 * - fucking damned division overflow error in wavetable device still
 *   has to be found (when using wavetable sound cards).
 *   (fixed due to some zero checks in devwiw/gus/sb32???)
 */


#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "itplay.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"


/* overriding stdlib's one, even though it would do */
#define random(this) it_random(this)
static int it_random(struct itplayer *this)
{
	this->randseed=this->randseed*0x15A4E35+12345;
	return (this->randseed>>16)&32767;
}

static struct itplayer *staticthis=NULL;

static int8_t sintab[256] =
{
	  0,   2,   3,   5,   6,   8,   9,  11,  12,  14,  16,  17,  19,  20,
	 22,  23,  24,  26,  27,  29,  30,  32,  33,  34,  36,  37,  38,  39,
	 41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,
	 55,  56,  56,  57,  58,  59,  59,  60,  60,  61,  61,  62,  62,  62,
	 63,  63,  63,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
	 63,  63,  63,  62,  62,  62,  61,  61,  60,  60,  59,  59,  58,  57,
	 56,  56,  55,  54,  53,  52,  51,  50,  49,  48,  47,  46,  45,  44,
	 43,  42,  41,  39,  38,  37,  36,  34,  33,  32,  30,  29,  27,  26,
	 24,  23,  22,  20,  19,  17,  16,  14,  12,  11,   9,   8,   6,   5,
	  3,   2,   0,  -2,  -3,  -5,  -6,  -8,  -9, -11, -12, -14, -16, -17,
	-19, -20, -22, -23, -24, -26, -27, -29, -30, -32, -33, -34, -36, -37,
	-38, -39, -41, -42, -43, -44, -45, -46, -47, -48, -49, -50, -51, -52,
	-53, -54, -55, -56, -56, -57, -58, -59, -59, -60, -60, -61, -61, -62,
	-62, -62, -63, -63, -63, -64, -64, -64, -64, -64, -64, -64, -64, -64,
	-64, -64, -63, -63, -63, -62, -62, -62, -61, -61, -60, -60, -59, -59,
	-58, -57, -56, -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, -46,
	-45, -44, -43, -42, -41, -39, -38, -37, -36, -34, -33, -32, -30, -29,
	-27, -26, -24, -23, -22, -20, -19, -17, -16, -14, -12, -11,  -9,  -8,
	 -6,  -5,  -3,  -2
};


static void playtick (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this);
static int range64(int v);
static int range128(int v);
static void dovibrato(struct itplayer *this, struct it_logchan *c);
static void putque(struct itplayer *this, int type, int val1, int val2);
static void readque (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this);
static void dotremolo(struct itplayer *this, struct it_logchan *c);
static void dodelay(struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, struct it_logchan *c);
static void dopanbrello(struct itplayer *this, struct it_logchan *c);


static void playtickstatic (struct cpifaceSessionAPI_t *cpifaceSession)
{
	playtick (cpifaceSession, staticthis);
}

static void playnote(struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, struct it_logchan *c, const uint8_t *cmd)
{
	int instchange=0;
	/* int triginst=0;  unused */
	int porta, frq;
	struct it_physchan *p;
	const struct it_sample *s;
	const struct it_instrument *in;

	if (cmd[0])
	{
		c->lastnote=cmd[0];
		/* this is kind of wierd, but we need to know of retrigger changes already now */
		if (cmd[3]==cmdRetrigger)
			c->retrigpos=cmd[4]>>4|!(cmd[4]&0xf0); /* |!(cmd[4]&0xf0) makes value 0, into 1 */
		else
			c->retrigpos=c->retrigspd;
	}

	if (cmd[1])
	{
		if (cmd[1]!=c->lastins)
			instchange=1;
		c->lastins=cmd[1];
	}

	if (c->lastnote==cmdNNoteOff)
	{
		if (c->pch)
			c->pch->noteoff=1;
		return;
	}
	if (c->lastnote==cmdNNoteCut)
	{
		if (c->pch)
			c->pch->notecut=1;
		return;
	}
	if (c->lastnote>=cmdNNoteFade)
	{
		if (c->pch)
			c->pch->notefade=1;
		return;
	}
	if ((c->lastins>this->ninst)||!c->lastins||!c->lastnote||(c->lastnote>=cmdNNoteFade))
		return;

	c->curnote=c->lastnote;

	porta=(cmd[3]==cmdPortaNote)||(cmd[3]==cmdPortaVol)||((cmd[2]>=cmdVPortaNote)&&(cmd[2]<(cmdVPortaNote+10)));

	if (!c->pch || c->pch->dead || !c->pch->fadeval)
		porta=0;

	if (instchange || !porta)
		c->fnotehit=1;

	do
	{
		if (!((!cmd[0]) && cmd[1] && (!instchange) && (!porta) && (cmd[3]!=cmdOffset) && c->pch))
			break;

/* http://eval.sovietrussia.org/wiki/Player_abuse_tests#Ping-pong_loop_and_sample_number
 *
 * Bug-fix workaround for cases where you just set instrument number with no note, should not re-trigger note,
 * just reset volumes
 */
		if (!(p=c->pch))
			break;
		if (p->notecut)
			break;
		if (p->dead)
			break;
		if (!(s=p->smp))
			break;
		if (!(in=p->inst))
			break;
		c->nna=in->nna;
		c->vol=((100-in->rv)*s->vol+in->rv*(rand()%65))/100;
		c->fvol=c->vol;
		c->pan=(in->dfp&128)?c->cpan:in->dfp;
		c->pan=(s->dfp&128)?(s->dfp&127):c->pan;
		c->pan=range64(c->pan+(((c->lastnote-cmdNNote-in->ppc)*in->pps)>>8));
		c->pan=/*(in->dfp&128)?c->pan:*/range64(c->pan+((in->rp*(rand() %129 -64))>>6));
		c->fpan=c->pan;
		return;
	} while(0); /* runs atleast one time */

	if (c->fnotehit)
	{
		int smp;

		if (porta)
			c->pch->notecut=1;
		smp=this->instruments[c->lastins-1].keytab[c->lastnote-cmdNNote][1];
		if (!smp||(smp>this->nsamp))
			return;
		if (!porta)
		{
			if (c->pch)
			{
				c->newchan.volenvpos=c->pch->volenvpos;
				c->newchan.panenvpos=c->pch->panenvpos;
				c->newchan.pitchenvpos=c->pch->pitchenvpos;
				c->newchan.filterenvpos=c->pch->filterenvpos;
				c->newchan=*c->pch;
				switch (c->nna)
				{
					case 0: c->pch->notecut=1; break;
					case 2: c->pch->noteoff=1; break;
					case 3: c->pch->notefade=1; break;
				}
				c->pch=0;
			} else {
				c->newchan.volenvpos=0;
				c->newchan.panenvpos=0;
				c->newchan.pitchenvpos=0;
				c->newchan.filterenvpos=0;
			}
			c->pch=&c->newchan;
			c->pch->avibpos=0;
			c->pch->avibdep=0;
			/* c->retrigpos=0; */
		}
		if (this->samples[smp-1].handle==0xFFFF)
		{
			fprintf(stderr, "playit/itplay.c: Assert #1\n");
			c->pch=0; /* deallocate channel, invalid sample */
			return;
		}

		c->pch->inst=&this->instruments[c->lastins-1];
		if (!c->pch->inst)
		{
			/* should be unreachable */
			fprintf(stderr, "playit/itplay.c: Assert #2\n");
			c->pch=0;
		}
		c->pch->smp=&this->samples[smp-1];
		if (!c->pch->smp)
		{
			/* should be unreachable */
			fprintf(stderr, "playit/itplay.c: Assert #3\n");
			c->pch=0;
		}
		c->basenote=c->pch->note=c->lastnote-cmdNNote;
		c->realnote=c->pch->inst->keytab[c->basenote][0];
		c->pch->noteoffset=(60-c->realnote+c->basenote)*256+c->pch->smp->normnote;
	}

	p=c->pch;
	s=p->smp;
	in=p->inst;

	if (cmd[3]==cmdOffset)
	{
		if (cmd[4])
			c->offset=(c->offset&0xF00)|cmd[4];
		p->newsamp=s->handle;
		p->newpos=c->offset<<8;
		if (p->newpos>=(signed)this->sampleinfos[s->handle].length)
			p->newpos=this->sampleinfos[s->handle].length-16;
	} else if (c->fnotehit)
	{
		p->newsamp=s->handle;
		p->newpos=0;
	}

	if (c->fnotehit || cmd[1])
	{
		int i;

		if (in->dct)
		{
			for (i=0; i<this->npchan; i++)
			{
				struct it_physchan *dp=&this->pchannels[i];
				if (dp==p)
					continue;
				if (dp->lch!=p->lch)
					continue;
				if (dp->inst!=p->inst)
					continue;
				if ((in->dct!=3)&&(dp->smp!=p->smp))
					continue;
				if ((in->dct==1)&&(dp->note!=p->note))
					continue;
				switch (in->dca)
				{
					case 0: dp->notecut=1; break;
					case 1: dp->noteoff=1; break;
					case 2: dp->notefade=1; break;
				}
			}
		}

		if (cmd[1])
		{
			p->fadeval=1024;
			p->fadespd=in->fadeout;
			p->notefade=0;
			p->dead=0;
			p->notecut=0;
			p->noteoff=0;
			p->looptype=0;
			p->volenv=in->envs[0].type&env_type_active;
			p->panenv=in->envs[1].type&env_type_active;
			p->penvtype=(in->envs[2].type&env_type_filter);
			p->pitchenv=(in->envs[2].type&env_type_active) && !p->penvtype;
			p->filterenv=(in->envs[2].type&env_type_active) && p->penvtype;
			if (!(in->envs[0].type&env_type_carry))
				p->volenvpos=0;
			if (!(in->envs[1].type&env_type_carry))
				p->panenvpos=0;
			if (!(in->envs[2].type&env_type_carry))
			{
				p->pitchenvpos=0;
				p->filterenvpos=0;
			}

			c->nna=in->nna;
			c->vol=((100-in->rv)*s->vol+in->rv*(rand()%65))/100;
			c->fvol=c->vol;
			c->pan=(in->dfp&128)?c->cpan:in->dfp;
			c->pan=(s->dfp&128)?(s->dfp&127):c->pan;
			c->pan=range64(c->pan+(((c->lastnote-cmdNNote-in->ppc)*in->pps)>>8));
			c->pan=/*(in->dfp&128)?c->pan:*/range64(c->pan+((in->rp*(rand() %129 -64))>>6));
			c->fpan=c->pan;
			c->cutoff=(in->ifp&128)?in->ifp:c->cutoff;
			c->fcutoff=c->cutoff;
			c->reso=(in->ifr&128)?in->ifr:c->reso;
		}
	}

	if (porta && instchange && !this->geffect)
	{
		c->basenote=c->lastnote-cmdNNote;
		c->realnote=in->keytab[c->basenote][0];
		p->noteoffset=(60-c->realnote+c->basenote)*256+s->normnote;
	}

	frq=p->noteoffset-256*(c->lastnote-cmdNNote);
	if (!this->linear)
		frq=cpifaceSession->mcpAPI->GetFreq6848(frq);
	c->dpitch=frq;
	if (!porta)
		c->fpitch=c->pitch=c->dpitch;
}

static void playvcmd(struct itplayer *this, struct it_logchan *c, int vcmd)
{
	c->vcmd=vcmd;
	if ((vcmd>=cmdVVolume)&&(vcmd<=(cmdVVolume+64)))
		c->fvol=c->vol=vcmd-cmdVVolume;
	else if ((vcmd>=cmdVPanning)&&(vcmd<=(cmdVPanning+64)))
	{
		c->fpan=c->pan=c->cpan=vcmd-cmdVPanning;
		c->srnd=0;
	} else if ((vcmd>=cmdVFVolSlU)&&(vcmd<(cmdVFVolSlU+10)))
	{
		if (vcmd!=cmdVFVolSlU)
			c->vvolslide=vcmd-cmdVFVolSlU;
		c->fvol=c->vol=range64(c->vol+c->vvolslide);
	} else if ((vcmd>=cmdVFVolSlD)&&(vcmd<(cmdVFVolSlD+10)))
	{
		if (vcmd!=cmdVFVolSlD)
			c->vvolslide=vcmd-cmdVFVolSlD;
		c->fvol=c->vol=range64(c->vol-c->vvolslide);
	} else if ((vcmd>=cmdVVolSlU)&&(vcmd<(cmdVVolSlU+10)))
	{
		if (vcmd!=cmdVVolSlU)
			c->vvolslide=vcmd-cmdVVolSlU;
		c->fvolslide=ifxVSUp;
	} else if ((vcmd>=cmdVVolSlD)&&(vcmd<(cmdVVolSlD+10)))
	{
		if (vcmd!=cmdVVolSlD)
			c->vvolslide=vcmd-cmdVVolSlD;
		c->fvolslide=ifxVSDown;
	} else  if ((vcmd>=cmdVPortaD)&&(vcmd<(cmdVPortaD+10)))
	{
		if (vcmd!=cmdVPortaD)
			c->porta=4*(vcmd-cmdVPortaD);
		c->vporta=c->porta;
		c->fpitchslide=ifxPSDown;
	} else if ((vcmd>=cmdVPortaU)&&(vcmd<(cmdVPortaU+10)))
	{
		if (vcmd!=cmdVPortaU)
			c->porta=4*(vcmd-cmdVPortaU);
		c->vporta=c->porta;
		c->fpitchslide=ifxPSUp;
	} else if ((vcmd>=cmdVPortaNote)&&(vcmd<(cmdVPortaNote+10)))
	{
		if (vcmd!=cmdVPortaNote)
		{
			int tmp="\x00\x01\x04\x08\x10\x20\x40\x60\x80\xFF"[vcmd-cmdVPortaNote];
			if (this->geffect)
				c->portanote=tmp;
			else
				c->porta=tmp;
		}
		if (this->geffect)
			c->vportanote=c->portanote;
		else
			c->vporta=c->porta;

		c->fpitchslide=ifxPSToNote;
	} else if ((vcmd>=cmdVVibrato)&&(vcmd<(cmdVVibrato+10)))
	{
		if (vcmd!=cmdVVibrato)
			c->vibdep=(vcmd-cmdVVibrato)*(this->oldfx?8:4);
		c->fpitchfx=ifxPXVibrato;
		dovibrato(this, c);
	}
}

static int range64(int v)
{
	return (v<0)?0:(v>64)?64:v;
}

static int range128(int v)
{
	return (v<0)?0:(v>128)?128:v;
}

static int rowslide(int data)
{
	if ((data&0x0F)==0x0F)
		return data>>4;
	else
		if ((data&0xF0)==0xF0)
			return -(data&0xF);
	return 0;
}

static int rowudslide(int data)
{
	if (data>=0xF0)
		return (data&0xF)*4;
	else if (data>=0xE0)
		return (data&0xF);
	return 0;
}

static int rowvolslide(int data)
{
	if (data==0xF0)
		return 0xF;
	else
		if (data==0x0F)
			return -0xF;
		else
			return rowslide(data);
}

static int tickslide(int data)
{
	if (!(data&0x0F))
		return data>>4;
	else
		if (!(data&0xF0))
			return -(data&0xF);
	return 0;
}

static void doretrigger(struct it_logchan *c)
{
	int x;
	struct it_physchan *p;

	c->retrigpos--;
	if (c->retrigpos)
		return;
	c->retrigpos=c->retrigspd;
	x=c->vol;
	switch (c->retrigvol)
	{
		case 1: case 2: case 3: case 4: case 5: x-=1<<(c->retrigvol-1); break;
		case 6: x=(5*x)>>3; break;
		case 7: x=x>>1; break;
		case 9: case 10: case 11: case 12: case 13: x+=1<<(c->retrigvol-9); break;
		case 14: x=(3*x)>>1; break;
		case 15: x=2*x; break;
	}
	c->fvol=c->vol=range64(x);
	if (!c->pch)
		return;
	p=c->pch;
	p->newpos=0;
	p->dead=0;
}

static void dotremor(struct it_logchan *c)
{
	if (c->tremoroncounter)
		c->tremoroncounter--;
	if (!c->tremoroncounter)
	{
		if (c->tremoroffcounter)
		{
			c->fvol=0;
			c->tremoroffcounter--;
		} else {
			c->tremoroncounter=c->tremoron;
			c->tremoroffcounter=c->tremoroff;
		}
	}
}

static int ishex(uint8_t c)
{
	return ((c>=48 && c<58) || (c>=65 && c<71));
}


static void parsemidicmd(struct it_logchan *c, char *cmd, int z)
{
	uint8_t bytes[32];
	int  count=0;
	while (*cmd)
	{
		if (ishex(*cmd))
		{
			int v0=(*cmd)-48;
			if (v0>9) v0-=7;
			cmd++;
			if (ishex(*cmd))
			{
				int v1=(*cmd)-48;
				if (v1>9) v1-=7;
				cmd++;
				bytes[count++]=(v0<<4)|v1;
			}
		} else if ((*cmd)=='Z')
		{
			bytes[count++]=z;
			cmd++;
		} else
			cmd++;
	}
	/* filter commands? */
	if (count==4 && bytes[0]==0xf0 && bytes[1]==0xf0)
	{
		switch (bytes[2])
		{
			case 0:
				c->cutoff=c->fcutoff=bytes[3]+128;
				break;
			case 1:
				c->reso=bytes[3]+128;
				break;
		}
	}
}

static void playcmd(struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, struct it_logchan *c, int cmd, int data)
{
	int i;
	c->command=cmd;
	switch (cmd)
	{
		case cmdSpeed:
			if (data)
				this->speed=data;
			putque(this, queSpeed, -1, this->speed);
			break;
		case cmdJump:
			this->gotorow=0;
			this->gotoord=data;
			break;
		case cmdBreak:
			if (this->gotoord==-1)
				this->gotoord=this->curord+1;
			this->gotorow=data;
			break;
		case cmdVolSlide:
			if (data)
				c->volslide=data;
			data=c->volslide;
			if (!(data&0x0F) && (data&0xF0))
			{
				c->fvolslide=ifxVSUp;
				c->fx=ifxVolSlideUp;
			} else if (!(data&0xF0) && (data&0x0F))
			{
				c->fvolslide=ifxVSDown;
				c->fx=ifxVolSlideDown;
			} else if ((data&0x0F)>0x0D && (data&0xF0))
				c->fx=ifxRowVolSlideUp;
			else if (((data&0xF0)>0xD0) && (data&0x0F))
				c->fx=ifxRowVolSlideDown;
			c->fvol=c->vol=range64(c->vol+rowvolslide(c->volslide));
			break;
		case cmdPortaD:
			if (data)
				c->porta=data;
			c->eporta=c->porta;
			c->fpitchslide=ifxPSDown;
			c->fx=ifxPitchSlideDown;
			c->fpitch=c->pitch=c->pitch+4*rowudslide(c->eporta);
			break;
		case cmdPortaU:
			if (data)
				c->porta=data;
			c->eporta=c->porta;
			c->fpitchslide=ifxPSUp;
			c->fx=ifxPitchSlideUp;
			c->fpitch=c->pitch=c->pitch-4*rowudslide(c->eporta);
			break;
		case cmdPortaNote:
			if (data)
			{
				if (this->geffect)
					c->portanote=data;
				else
					c->porta=data;
			}
			if (this->geffect)
				c->eportanote=c->portanote;
			else
				c->eporta=c->porta;
			c->fpitchslide=ifxPSToNote;
			c->fx=ifxPitchSlideToNote;
			break;
		case cmdVibrato:
			if (data&0xF)
				c->vibdep=(data&0xF)*(this->oldfx?8:4);
			if (data>>4)
				c->vibspd=data>>4;
			c->fpitchfx=ifxPXVibrato;
			c->fx=ifxPitchVibrato;
			dovibrato(this, c);
			break;
		case cmdTremor:
			if (data)
			{
				c->tremoron=(data>>4)+(this->oldfx);
				c->tremoroff=(data&0xF)+(this->oldfx);
			}
			if (!c->tremoroff)
				c->tremoroff=1;
			if (!c->tremoron)
				c->tremoron=1;
			c->fvolfx=ifxVXTremor;
			c->fx=ifxTremor;
			dotremor(c);
			break;
		case cmdArpeggio:
			if (data)
			{
				c->arpeggio1=data>>4;
				c->arpeggio2=data&0xF;
			}
			c->fpitchfx=ifxPXArpeggio;
			c->fx=ifxArpeggio;
			break;
		case cmdPortaVol:
			if (data)
				c->volslide=data;
			data=c->volslide;
			c->fpitchslide=ifxPSToNote;
			c->fx=ifxPitchSlideToNote;
			if (!(data&0x0F) && (data&0xF0))
			{
				c->fvolslide=ifxVSUp;
				c->fx=ifxVolSlideUp;
			} else if (!(data&0xF0) && (data&0x0F))
			{
				c->fvolslide=ifxVSDown;
				c->fx=ifxVolSlideDown;
			} else if ((data&0x0F)>0x0D && (data&0xF0))
				c->fx=ifxRowVolSlideUp;
			else if (((data&0xF0)>0xD0) && (data&0x0F))
				c->fx=ifxRowVolSlideDown;
			c->fvol=c->vol=range64(c->vol+rowvolslide(c->volslide));
			break;
		case cmdVibVol:
			if (data)
				c->volslide=data;
			data=c->volslide;
			c->fpitchfx=ifxPXVibrato;
			c->fx=ifxPitchVibrato;
			if (!(data&0x0F) && (data&0xF0))
			{
				c->fvolslide=ifxVSUp;
				c->fx=ifxVolSlideUp;
			} else if (!(data&0xF0) && (data&0x0F))
			{
				c->fvolslide=ifxVSDown;
				c->fx=ifxVolSlideDown;
			} else if ((data&0x0F)>0x0D && (data&0xF0))
				c->fx=ifxRowVolSlideUp;
			else if (((data&0xF0)>0xD0) && (data&0x0F))
				c->fx=ifxRowVolSlideDown;
			c->fvol=c->vol=range64(c->vol+rowvolslide(c->volslide));
			dovibrato(this, c);
			break;
		case cmdChanVol:
			if (data<=64)
				c->cvol=data;
			break;
		case cmdChanVolSlide:
			if (data)
				c->cvolslide=data;
			data=c->cvolslide;
			if ((data&0x0F)==0 && data&0xF0)
				c->fx=ifxChanVolSlideUp;
			else if ((data&0xF0)==0 && data&0x0F)
				c->fx=ifxChanVolSlideDown;
			else if ((data&0x0F)>0x0D && data&0xF0)
				c->fx=ifxRowChanVolSlideUp;
			else if ((data&0xF0)>0xD0 && data&0x0F)
				c->fx=ifxRowChanVolSlideDown;
			c->cvol=range64(c->cvol+rowslide(c->cvolslide));
			break;
		case cmdOffset:
			c->fx=ifxOffset;
			break;
		case cmdPanSlide:
			if (data)
				c->panslide=data;
			data=c->panslide;
			if (!(data&0x0F))
			{
				c->fpanslide=ifxPnSLeft;
				c->fx=ifxPanSlideLeft;
			} else if (!(data&0xF0))
			{
				c->fpanslide=ifxPnSRight;
				c->fx=ifxPanSlideRight;
			}
			c->fpan=c->cpan=c->pan=range64(c->pan-rowslide(c->panslide));
			break;
		case cmdRetrigger:
			if (data)
			{
				c->retrigspd=data&0xF;
				c->retrigvol=data>>4;
			}
			if (!c->retrigspd)
				c->retrigspd=1;
			c->fx=ifxRetrig;
			doretrigger(c);
			break;
		case cmdTremolo:
			if (data&0xF)
				c->tremdep=data&0xF;
			if (data>>4)
				c->tremspd=data>>4;
			c->fvolfx=ifxVXVibrato;
			c->fx=ifxVolVibrato;
			dotremolo(this, c);
			break;
		case cmdSpecial:
			switch (c->specialcmd)
			{
				case cmdSVibType:
					if (c->specialdata<4)
						c->vibtype=c->specialdata;
					break;
				case cmdSTremType:
					if (c->specialdata<4)
						c->tremtype=c->specialdata;
					break;
				case cmdSPanbrType:
					if (c->specialdata<4)
						c->panbrtype=c->specialdata;
					break;
				case cmdSPatDelayTick:
					this->patdelaytick=c->specialdata;
					break;
				case cmdSInstFX:
					switch (c->specialdata)
					{
						case cmdSIPastCut:
							c->fx=ifxPastCut;
							for (i=0; i<this->npchan; i++)
								if ((c==this->pchannels[i].lchp)&&(c->pch!=&this->pchannels[i]))
									this->pchannels[i].notecut=1;
							break;
						case cmdSIPastOff:
							c->fx=ifxPastOff;
							for (i=0; i<this->npchan; i++)
								if ((c==this->pchannels[i].lchp)&&(c->pch!=&this->pchannels[i]))
									this->pchannels[i].noteoff=1;
							break;
						case cmdSIPastFade:
							c->fx=ifxPastFade;
							for (i=0; i<this->npchan; i++)
								if ((c==this->pchannels[i].lchp)&&(c->pch!=&this->pchannels[i]))
									this->pchannels[i].notefade=1;
							break;
						case cmdSINNACut:
							c->nna=0;
							break;
						case cmdSINNACont:
							c->nna=1;
							break;
						case cmdSINNAOff:
							c->nna=2;
							break;
						case cmdSINNAFade:
							c->nna=3;
							break;
						case cmdSIVolEnvOff:
							c->fx=ifxVEnvOff;
							if (c->pch)
								c->pch->volenv=0;
							break;
						case cmdSIVolEnvOn:
							c->fx=ifxVEnvOn;
							if (c->pch)
								c->pch->volenv=1;
							break;
						case cmdSIPanEnvOff:
							c->fx=ifxPEnvOff;
							if (c->pch)
								c->pch->panenv=0;
							break;
						case cmdSIPanEnvOn:
							c->fx=ifxPEnvOn;
							if (c->pch)
								c->pch->panenv=1;
							break;
						case cmdSIPitchEnvOff:
							c->fx=ifxFEnvOff;
							if (c->pch)
							{
								if (c->pch->penvtype)
									c->pch->filterenv=0;
								else
									c->pch->pitchenv=0;
							}
							break;
						case cmdSIPitchEnvOn:
							c->fx=ifxFEnvOn;
							if (c->pch)
							{
								if (c->pch->penvtype)
									c->pch->filterenv=1;
								else
									c->pch->pitchenv=1;
							}
							break;
					}
					break;
				case cmdSPanning:
					c->srnd=0;
					c->fpan=c->pan=c->cpan=((c->specialdata*0x11)+1)>>2;
					break;
				case cmdSSurround:
					if (c->specialdata==1)
						c->srnd=1;
					break;
				case cmdSOffsetHigh:
					c->offset=(c->offset&0xFF)|(c->specialdata<<8);
					break;
				case cmdSPatLoop:
					/*if(plLoopPatterns)*/ /* TODO */
					{
						if (!c->specialdata)
							c->patloopstart=this->currow;
						else
						{
							if (!c->patloopcount)
								c->patloopcount=c->specialdata;
							else
								c->patloopcount--;
							if (c->patloopcount)
							{
								this->gotorow=c->patloopstart;
								this->gotoord=this->curord;
							} else {
								c->patloopstart=this->currow+1;
							}
						}
					}
					break;
				case cmdSNoteCut:
					c->fx=ifxNoteCut;
					if (!c->specialdata)
						c->specialdata++; /* SC0 should do the same as SC1 */
					break;
				case cmdSNoteDelay:
					c->fx=ifxDelay;
					if (!c->specialdata)
						c->specialdata++; /* SD0 should do the same as SD1 */
					dodelay(cpifaceSession, this, c);
					break;
				case cmdSPatDelayRow:
					if (!this->patdelayrow) /* only use the first command, per row */
						this->patdelayrow=c->specialdata+1;
					break;
				case cmdSSetMIDIMacro:
					c->sfznum=c->specialdata;
					break;
			}
			break;
		case cmdTempo:
			if (data)
				c->tempo=data;
			if (c->tempo>=0x20)
			{
				this->tempo=c->tempo;
				putque(this, queTempo, -1, this->tempo);
			}
			break;
		case cmdFineVib:
			if (data&0xF)
				c->vibdep=(data&0xF<<(this->oldfx))>>1;
			if (data>>4)
				c->vibspd=data>>4;
			dovibrato(this, c);
			break;
		case cmdGVolume:
			if (data<=128)
				this->gvol=data;
			putque(this, queGVol, -1, this->gvol);
			break;
		case cmdGVolSlide:
			if (data)
				c->gvolslide=data;
			this->gvolslide=data;
			this->gvol=range128(this->gvol+rowslide(c->gvolslide));
			putque(this, queGVol, -1, this->gvol);
			break;
		case cmdPanning:
			c->srnd=0;
			c->fpan=c->cpan=c->pan=(data+1)>>2;
			break;
		case cmdPanbrello:
			if (data&0xF)
				c->panbrdep=data&0xF;
			if (data>>4)
				c->panbrspd=data>>4;
			c->fx=ifxPanBrello;
			dopanbrello(this, c);
			break;
		case cmdMIDI:
			if (this->midicmds)
			{
				if (data&0x80)
					parsemidicmd(c, this->midicmds[data-103], 0);
				else
					parsemidicmd(c, this->midicmds[c->sfznum+9], data);
			}
			break;
		/* DEPRECATED - Zxx IS MIDI MACRO TRIGGER
		case cmdSync:
			putque(this, queSync, c->newchan.lch, data);
		*/
	}
}

static void dovibrato(struct itplayer *this, struct it_logchan *c)
{
	int x;
	switch (c->vibtype)
	{
		case 0: x=sintab[4*(c->vibpos&63)]>>1; break;
		case 1: x=32-(c->vibpos&63); break;
		case 2: x=32-(c->vibpos&32); break;
		default: /* remove a warning */
		case 3: x=(random(this)&63)-32;
	}
	if (this->curtick || !this->oldfx)
	{
		c->fpitch-=(c->vibdep*x)>>3;
		c->vibpos-=c->vibspd;
	}
}

static void dotremolo(struct itplayer *this, struct it_logchan *c)
{
	int x;
	switch (c->tremtype)
	{
		case 0: x=sintab[4*(c->trempos&63)]>>1; break;
		case 1: x=32-(c->trempos&63); break;
		case 2: x=32-(c->trempos&32); break;
		default: /* remove a warning */
		case 3: x=(random(this)&63)-32;
	}
	c->fvol=range64(c->fvol+((c->tremdep*x)>>4));
	c->trempos+=c->tremspd;
}

static void dopanbrello(struct itplayer *this, struct it_logchan *c)
{
	int x;

	if (c->panbrtype==3)
	{
		if (c->panbrpos>=c->panbrspd)
		{
			c->panbrpos=0;
			c->panbrrnd=random(this);
		}
		c->fpan=range64(c->fpan+((c->panbrdep*((c->panbrrnd&255)-128))>>6));
	} else {
		switch (c->panbrtype)
		{
			case 0: x=sintab[c->panbrpos&255]*2; break;
			case 1: x=128-(c->panbrpos&255); break;
			default: /* remove a warning */
			case 2: x=128-2*(c->panbrpos&128); break;
		}
		c->fpan=range64(c->fpan+((c->panbrdep*x)>>6));
	}
	c->panbrpos+=c->panbrspd;
}

static void doportanote(struct itplayer *this, struct it_logchan *c, int v) /* if v, then do vporta, else eporta */
{

	if (!c->dpitch)
		return;

	if (c->pitch<c->dpitch)
	{
		if (v)
			c->pitch+=(this->geffect?c->vportanote:c->vporta)*16;
		else
			c->pitch+=(this->geffect?c->eportanote:c->eporta)*16;

		if (c->pitch>c->dpitch)
			c->pitch=c->dpitch;
	} else {
		if (v)
			c->pitch-=(this->geffect?c->vportanote:c->vporta)*16;
		else
			c->pitch-=(this->geffect?c->eportanote:c->eporta)*16;
		if (c->pitch<c->dpitch)
			c->pitch=c->dpitch;
	}
	c->fpitch=c->pitch;
	if (c->pitch==c->dpitch)
		c->dpitch=0;
}

static void dodelay(struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, struct it_logchan *c)
{
	if (this->curtick==c->specialdata)
	{
		if (c->delayed[0]||c->delayed[1])
			playnote(cpifaceSession, this, c, c->delayed);
		if (c->delayed[2])
			playvcmd(this, c, c->delayed[2]);
	} else if (((this->curtick+1)==(this->speed+this->patdelaytick))&&(!this->patdelayrow))
	{
/* Impulse Tracker has a bug where a too long delay affects the lastinstrument, even if no note were played.
 *
 * http://eval.sovietrussia.org/wiki/Player_abuse_tests#Out-of-range_note_delays
 */
		if (c->delayed[1])
			c->lastins=c->delayed[1];
	}

}

static int rangepitch(struct itplayer *this, int p)
{
	return (p<this->pitchhigh)?this->pitchhigh:(p>this->pitchlow)?this->pitchlow:p;
}

static const uint16_t arpnotetab[] =
{
	32768, 30929, 29193, 27554,
	26008, 24548, 23170, 21870,
	20643, 19484, 18390, 17358,
	16384, 15464, 14596, 13777
};

static void processfx(struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, struct it_logchan *c)
{
	switch (c->command)
	{
		case cmdSpeed:
			break;
		case cmdJump:
			break;
		case cmdBreak:
			break;
		case cmdVolSlide:
			c->vol=range64(c->vol+tickslide(c->volslide));
			break;
		case cmdPortaD:
			if (c->eporta>=0xE0)
				break;
			c->fpitch=c->pitch=rangepitch(this, c->pitch+c->eporta*16);
			break;
		case cmdPortaU:
			if (c->eporta>=0xE0)
				break;
			c->fpitch=c->pitch=rangepitch(this, c->pitch-c->eporta*16);
			if ((c->pitch==this->pitchhigh)&&c->pch)
				c->pch->notecut=1;
			break;
		case cmdPortaNote:
			doportanote(this, c, 0);
			break;
		case cmdVibrato:
			dovibrato(this, c);
			break;
		case cmdTremor:
			dotremor(c);
			break;
		case cmdArpeggio:
			{
				int arpnote;
				switch (this->curtick%3)
				{
					default: /* remove a warning */
					case 0:
						arpnote=0;
						break;
					case 1:
						arpnote=c->arpeggio1;
						break;
					case 2:
						arpnote=c->arpeggio2;
						break;
				}
				if (this->linear)
					c->fpitch=c->fpitch-(arpnote<<8);
				else
					c->fpitch=(c->fpitch*arpnotetab[arpnote])>>15;
				break;
			}
		case cmdPortaVol:
			doportanote(this, c, 0);
			c->fvol=c->vol=range64(c->vol+tickslide(c->volslide));
			break;
		case cmdVibVol:
			dovibrato(this, c);
			c->fvol=c->vol=range64(c->vol+tickslide(c->volslide));
			break;
		case cmdChanVol:
			break;
		case cmdChanVolSlide:
			c->cvol=range64(c->cvol+tickslide(c->cvolslide));
			break;
		case cmdOffset:
			break;
		case cmdPanSlide:
			c->fpan=c->cpan=c->pan=range64(c->pan-tickslide(c->panslide));
			break;
		case cmdRetrigger:
			doretrigger(c);
			break;
		case cmdTremolo:
			dotremolo(this, c);
			break;
		case cmdSpecial:
			switch (c->specialcmd)
			{
				case cmdSVibType:
					break;
				case cmdSTremType:
					break;
				case cmdSPanbrType:
					break;
				case cmdSPatDelayTick:
					break;
				case cmdSInstFX:
					break;
				case cmdSPanning:
					break;
				case cmdSSurround:
					break;
				case cmdSOffsetHigh:
					break;
				case cmdSPatLoop:
					break;
				case cmdSNoteCut:
					if ((this->curtick>=c->specialdata)&&c->pch&&(!this->patdelayrow))
						c->pch->notecut=1;
					break;
				case cmdSNoteDelay:
					dodelay(cpifaceSession, this, c);
					break;
				case cmdSPatDelayRow:
					break;
			}
			break;
		case cmdTempo:
			if (c->tempo<0x20)
			{
				this->tempo+=(c->tempo<0x10)?-c->tempo:(c->tempo&0xF);
				this->tempo=(this->tempo<0x20)?0x20:(this->tempo>0xFF)?0xFF:this->tempo;
				putque(this, queTempo, -1, this->tempo);
			}
			break;
		case cmdFineVib:
			dovibrato(this, c);
			break;
		case cmdGVolume:
			break;
		case cmdGVolSlide:
			this->gvol=range128(this->gvol+tickslide(c->gvolslide));
			putque(this, queGVol, -1, this->gvol);
			break;
		case cmdPanning:
			break;
		case cmdPanbrello:
			dopanbrello(this, c);
			break;
	}


	if ((c->vcmd>=cmdVVolSlD)&&(c->vcmd<(cmdVVolSlD+10)))
		c->fvol=c->vol=range64(c->vol-c->vvolslide);
	else if ((c->vcmd>=cmdVVolSlU)&&(c->vcmd<(cmdVVolSlU+10)))
		c->fvol=c->vol=range64(c->vol+c->vvolslide);
	else if ((c->vcmd>=cmdVVibrato)&&(c->vcmd<(cmdVVibrato+10)))
		dovibrato(this, c);
	else if ((c->vcmd>=cmdVPortaNote)&&(c->vcmd<(cmdVPortaNote+10)))
		doportanote(this, c, 1);
	else if ((c->vcmd>=cmdVPortaU)&&(c->vcmd<(cmdVPortaU+10)))
	{
		c->fpitch=c->pitch=rangepitch(this, c->pitch-c->vporta*16);
		if ((c->pitch==this->pitchhigh)&&c->pch)
			c->pch->notecut=1;
	} else if ((c->vcmd>=cmdVPortaD)&&(c->vcmd<(cmdVPortaD+10)))
		c->fpitch=c->pitch=rangepitch(this, c->pitch+c->vporta*16);
}

static void inittickchan(struct it_physchan *p)
{
	p->fvol=p->vol;
	p->fpan=p->pan;
	p->fpitch=p->pitch;
}

static void inittick(struct it_logchan *c)
{
	c->fvol=c->vol;
	c->fpan=c->pan;
	c->fpitch=c->pitch;
}

static void initrow(struct it_logchan *c)
{
	c->command=0;
	c->vcmd=0;
}

static void updatechan(struct it_logchan *c)
{
	struct it_physchan *p;
	if (!c->pch)
		return;
	p=c->pch;
	p->vol=(c->vol*c->cvol)>>4;
	p->fvol=(c->fvol*c->cvol)>>4;
	p->pan=c->pan*4-128;
	p->fpan=c->fpan*4-128;
	p->cutoff=c->cutoff;
	p->fcutoff=c->fcutoff;
	p->reso=c->reso;
	p->pitch=-c->pitch;
	p->fpitch=-c->fpitch;
	p->srnd=c->srnd;
}

static int processenvelope(const struct it_envelope *env, int *pos, int noteoff, int env_type_active)
{
	int i, x;

	for (i=0; i<env->len; i++)
		if ((*pos)<env->x[i+1])
			break;

	if (env->x[i]==env->x[i+1] || (*pos)==env->x[i])
		x=256*env->y[i];
	else {
		float s=(float)((*pos)-env->x[i])/(float)(env->x[i+1]-env->x[i]);
		x=256.0*((1-s)*env->y[i]+s*env->y[i+1]);
	}

	if (env_type_active)
		(*pos)++;

	if (!noteoff&&(env->type&env_type_slooped))
	{
		if ((*pos)==(env->x[env->sloope]+1))
			(*pos)=env->x[env->sloops];
	} else if (env->type&env_type_looped)
	{
		if ((*pos)==(env->x[env->loope]+1))
			(*pos)=env->x[env->loops];
	}
	if ((*pos)>env->x[env->len])
		(*pos)=env->x[env->len];

	return x;
}

static void processchan(struct itplayer *this, struct it_physchan *p)
{
	int x;

	if (p->volenvpos||p->volenv)
		p->fvol=(processenvelope(&p->inst->envs[0], &p->volenvpos, p->noteoff, p->volenv)*p->fvol)>>14;

	if (p->volenv)
	{
		const struct it_envelope *env=&p->inst->envs[0];
		if (p->noteoff&&(p->inst->envs[0].type&env_type_looped))
			p->notefade=1;
		if ((p->volenvpos==env->x[env->len])&&!(p->inst->envs[0].type&env_type_looped)&&(!(p->inst->envs[0].type&env_type_slooped)||p->noteoff))
		{
			if (!env->y[env->len])
				p->notecut=1;
			else
				p->notefade=1;
		}
	} else
		if (p->noteoff)
			p->notefade=1;

	p->fvol=(p->fvol*p->fadeval)>>10;
	p->fadeval-=p->notefade?(p->fadeval>p->fadespd)?p->fadespd:p->fadeval:0;
	if (!p->fadeval)
		p->notecut=1;

	p->fvol=(this->gvol*p->fvol)>>7;
	p->fvol=(p->smp->gvl*p->fvol)>>6;
	p->fvol=(p->inst->gbv*p->fvol)>>7;


	if (p->panenvpos||p->panenv)
		p->fpan+=processenvelope(&p->inst->envs[1], &p->panenvpos, p->noteoff, p->panenv)>>6;

	if (p->srnd)
		p->fpan=0;

	p->fpan=(this->chsep*p->fpan)>>7;


	if (p->pitchenvpos||p->pitchenv)
	{
		if (this->linear)
			p->fpitch+=processenvelope(&p->inst->envs[2], &p->pitchenvpos, p->noteoff, p->pitchenv)>>1;
		else {
			int e=processenvelope(&p->inst->envs[2], &p->pitchenvpos, p->noteoff, p->pitchenv);
			int e2;
			int shl=0;
			int shr=0;
			int fac;
			while (e>6144)
			{
				e-=6144;
				shl++;
			}
			while (e<0)
			{
				e+=6144;
				shr++;
			}
			e2=0x200-(e&0x1ff);
			e>>=9;
			fac=(e2*arpnotetab[12-e]+(512-e2)*arpnotetab[11-e])>>9;
			fac>>=shr;
			fac<<=shl;
			p->fpitch=imuldiv(p->fpitch,16384,fac);
		}
	}

	switch (p->smp->vit)
	{
		case 0: x=sintab[p->avibpos&255]*2; break;
		case 1: x=128-(p->avibpos&255); break;
		case 2: x=128-(p->avibpos&128); break;
		default: /* remove a warning */
		case 3: x=(random(this)&255)-128;
	}
	p->fpitch+=(x*p->avibdep)>>14;
	p->avibpos+=p->smp->vis;
	p->avibdep+=p->smp->vir;
	if (p->avibdep>(p->smp->vid*256))
		p->avibdep=p->smp->vid*256;

	if (p->filterenvpos||p->filterenv)
	{
		p->fcutoff=p->cutoff&0x7f;
		p->fcutoff*=processenvelope(&p->inst->envs[2], &p->filterenvpos, p->noteoff, p->filterenv)+8192;
		p->fcutoff>>=14;
		p->fcutoff|=0x80;
	}

}

static void putchandata (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, struct it_physchan *p)
{
	if (p->newsamp!=-1)
	{
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCReset, 0);
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCInstrument, p->newsamp);
		p->newsamp=-1;
	}
	if (p->newpos!=-1)
	{
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCPosition, p->newpos);
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCLoop, 1);
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCDirect, 0);
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCStatus, 1);
		p->newpos=-1;
		p->dead=0;
	}
	if (p->noteoff&&!p->looptype)
	{
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCLoop, 2);
		p->looptype=1;
	}
	if (this->linear)
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCPitch, p->fpitch);
	else
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCPitch6848, -p->fpitch);

	cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCVolume, p->fvol);
	cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCPanning, p->fpan);
	cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCSurround, p->srnd);
	cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCMute, this->channels[p->lch].mute);
	cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCFilterFreq, p->fcutoff);
	cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCFilterRez, p->reso);
}

void __attribute__ ((visibility ("internal"))) mutechan(struct itplayer *this, int c, int m)
{
	if ((c>=0)||(c<this->nchan))
		this->channels[c].mute=m;
}

static void allocatechan(struct itplayer *this, struct it_logchan *c)
{
	int i;

	if (c->disabled)
	{
		c->pch=0;
		return;
	}
	for (i=0; i<this->npchan; i++)
		if (this->pchannels[i].lch==-1)
			break;

	if (i==this->npchan)
		for (i=0; i<this->npchan; i++)
			if (this->pchannels[i].dead)
				break;

	/* search for most silent NNAed channel */
	if (i==this->npchan)
	{
		int bestpch=this->npchan;
		int bestvol=0xffffff;
		for (i=0; i<this->npchan; i++)
			if ((this->pchannels[i].notecut || this->pchannels[i].noteoff || this->pchannels[i].notefade) && this->pchannels[i].fvol<=bestvol)
			{
				bestpch=i;
				bestvol=this->pchannels[i].fvol;
			}
		i=bestpch;
	}

	/* when everything fails, search for channel with lowest vol */
	if (i==this->npchan)
	{
		int bestpch=this->npchan;
		int bestvol=c->newchan.fvol;
		for (i=0; i<this->npchan; i++)
			if (this->pchannels[i].fvol<bestvol )
			{
				bestpch=i;
				bestvol=this->pchannels[i].fvol;
			}
		i=bestpch;
	}

	if (i<this->npchan)
	{
		if (this->pchannels[i].lch!=-1)
		{
			if (this->channels[this->pchannels[i].lch].pch==&this->pchannels[i])
				this->channels[this->pchannels[i].lch].pch=0;
		}
		this->pchannels[i]=c->newchan;
		this->pchannels[i].lchp=c;
		this->pchannels[i].no=i;
		c->pch=&this->pchannels[i];
	} else
		c->pch=0;

}

static void getproctime (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this)
{
	this->proctime = cpifaceSession->mcpGet (-1, mcpGCmdTimer);
}

static void putglobdata (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this)
{
	cpifaceSession->mcpSet (cpifaceSession, -1, mcpGSpeed, 256*2*this->tempo/5);
}

static void putque(struct itplayer *this, int type, int val1, int val2)
{
	if (((this->quewpos+1)%this->quelen)==this->querpos)
		return;
	this->que[this->quewpos][0]=this->proctime;
	this->que[this->quewpos][1]=type;
	this->que[this->quewpos][2]=val1;
	this->que[this->quewpos][3]=val2;
	this->quewpos=(this->quewpos+1)%this->quelen;
}

static int gettime (struct cpifaceSessionAPI_t *cpifaceSession)
{
	return cpifaceSession->mcpGet (-1, mcpGTimer);
}

static void readque (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this)
{
	int i;
	int time = gettime (cpifaceSession);
	while (1)
	{
		int t, val1, val2;
		if (this->querpos==this->quewpos)
			break;
		t=this->que[this->querpos][0];
		if (time<t)
			break;
		val1=this->que[this->querpos][2];
		val2=this->que[this->querpos][3];
		switch (this->que[this->querpos][1])
		{
			case queSync:
				this->realsync=val2;
				this->realsynctime=t;
				this->channels[val1].realsync=val2;
				this->channels[val1].realsynctime=t;
				break;
			case quePos:
				this->realpos=val2;
				for (i=0; i<this->nchan; i++)
				{
					struct it_logchan *c=&this->channels[i];
					if (c->evpos==-1)
					{
						if (c->evpos0==this->realpos)
						{
							c->evpos=this->realpos;
							c->evtime=t;
						}
					} else {
						switch (c->evmodtype)
						{
							case 1:
								c->evmodpos++;
								break;
							case 2:
								if (!(this->realpos&0xFF))
									c->evmodpos++;
								break;
							case 3:
								if (!(this->realpos&0xFFFF))
									c->evmodpos++;
								break;
						}
						if ((c->evmodpos==c->evmod)&&c->evmod)
						{
							c->evmodpos=0;
							c->evpos=this->realpos;
							c->evtime=t;
						}
					}
				}
				break;
			case queGVol: this->realgvol=val2; break;
			case queTempo: this->realtempo=val2; break;
			case queSpeed: this->realspeed=val2; break;
		};
		this->querpos=(this->querpos+1)%this->quelen;
	}
}

static void checkchan (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, struct it_physchan *p)
{
	if (!cpifaceSession->mcpGet (p->no, mcpCStatus))
		p->dead=1;
	if (p->dead&&(this->channels[p->lch].pch!=p))
		p->notecut=1;
	if (p->notecut)
	{
		if (this->channels[p->lch].pch==p)
			this->channels[p->lch].pch=0;
		p->lch=-1;
		cpifaceSession->mcpSet (cpifaceSession, p->no, mcpCReset, 0);
		return;
	}
}

static void playtick (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this)
{
	int i;

	if (this->looped&&this->noloop)
	{
		cpifaceSession->mcpSet (cpifaceSession, -1, mcpMasterPause, 1);
		return;
	}

	if (!this->npchan)
		return;

	getproctime (cpifaceSession, this);
	readque (cpifaceSession, this);

	for (i=0; i<this->nchan; i++)
		inittick(&this->channels[i]);

	this->curtick++;
	if ((this->curtick==(this->speed+this->patdelaytick))&&this->patdelayrow)
	{
		this->curtick=0;
		this->patdelayrow--;
	}

	if (this->curtick==(this->speed+this->patdelaytick))
	{
		this->patdelaytick=0;
		this->curtick=0;
		for (i=0; i<this->nchan; i++)
		{
			struct it_logchan *ch=&this->channels[i];
			ch->fnotehit=0;
			ch->fvolslide=0;
			ch->fpitchslide=0;
			ch->fpanslide=0;
			ch->fpitchfx=0;
			ch->fvolfx=0;
			ch->fnotefx=0;
			ch->fx=0;
		}
		this->currow++;
		if ((this->gotoord==-1)&&(this->currow==this->patlens[this->orders[this->curord]]))
		{
			this->gotoord=this->curord+1;
			this->gotorow=0;
		}
		if (this->gotoord!=-1)
		{
			if (this->gotoord!=this->curord)
				for (i=0; i<this->nchan; i++)
				{
					struct it_logchan *c=&this->channels[i];
					c->patloopcount=0;
					c->patloopstart=0;
				}
			if (this->gotoord>=this->endord)
			{
				this->gotoord=0;
				if (!this->manualgoto && this->noloop)
					this->looped=1;
				for (i=0; i<this->nchan; i++)
				{
					struct it_logchan *c=&this->channels[i];
					c->retrigspd=1;
					c->tremoron=1;
					c->tremoroff=1;
					c->tremoroncounter=0;
					c->tremoroffcounter=0;
				}
			}
			if (this->gotoord < this->curord && !this->manualgoto)
			{
				this->looped=1;
			}
			while (this->orders[this->gotoord]==0xFFFF)
				this->gotoord++;
			if (this->gotoord==this->endord)
				this->gotoord=0;
			if (this->gotorow>=this->patlens[this->orders[this->gotoord]])
			{
				this->gotoord++;
				this->gotorow=0;
				while (this->orders[this->gotoord]==0xFFFF)
					this->gotoord++;
				if (this->gotoord==this->endord)
					this->gotoord=0;
			}
			this->curord=this->gotoord;
			this->patptr=this->patterns[this->orders[this->curord]];
			for (this->currow=0; this->currow<this->gotorow; this->currow++)
			{
				while (*this->patptr)
					this->patptr+=6;
				this->patptr++;
			}
			this->gotoord=-1;
		}

		if (this->looped&&this->noloop)
		{
			for (i=0; i<this->nchan; i++)
			{
				mutechan(this, i, 1);
				updatechan(&this->channels[i]);
			}
			return;
		}

		for (i=0; i<this->nchan; i++)
			initrow(&this->channels[i]);
		while (*this->patptr)
		{
			struct it_logchan *c=&this->channels[*this->patptr++-1];
			int delay;
			if ((this->patptr[3]==cmdSpecial)&&this->patptr[4])
			{
				c->specialcmd=this->patptr[4]>>4;
				c->specialdata=this->patptr[4]&0xF;
			}
			if ((this->patptr[3]==cmdSpecial)&&(c->specialcmd==cmdSNoteDelay))
			{
				memcpy(c->delayed, this->patptr, /*sizeof(c->delayed)*/5); /* else we might read unknown unmapped memory */
				delay=1;
			} else {
				if (this->patptr[0]||this->patptr[1])
					playnote(cpifaceSession, this, c, this->patptr);
				delay=0;
			}
			if (this->patptr[3])
				playcmd(cpifaceSession, this, c, this->patptr[3], this->patptr[4]); /* we need notedata to know if Portamento should do anything */
			if (!delay)
				if (this->patptr[2])
					playvcmd(this, c, this->patptr[2]);
			this->patptr+=5;
		}
		this->patptr++;
		if (this->patdelayrow) /* in case, the first SEx command was SE0, we fetch this by storing SEx+1 to this->patdelayrow */
			this->patdelayrow--;
	} else
		for (i=0; i<this->nchan; i++)
			processfx(cpifaceSession, this, &this->channels[i]);

	this->manualgoto=0;
	this->gvolslide=0;

	for (i=0; i<this->npchan; i++)
		inittickchan(&this->pchannels[i]);
	for (i=0; i<this->nchan; i++)
		updatechan(&this->channels[i]);
	for (i=0; i<this->npchan; i++)
		if (this->pchannels[i].lch!=-1)
			checkchan (cpifaceSession, this, &this->pchannels[i]);
	for (i=0; i<this->npchan; i++)
		if (this->pchannels[i].lch!=-1)
			processchan(this, &this->pchannels[i]);
	for (i=0; i<this->nchan; i++)
		if (this->channels[i].pch==&this->channels[i].newchan)
		{
			processchan(this, this->channels[i].pch);
			allocatechan(this, &this->channels[i]);
		}
	for (i=0; i<this->npchan; i++)
		if (this->pchannels[i].lch!=-1)
			putchandata (cpifaceSession, this, &this->pchannels[i]);

	putglobdata (cpifaceSession, this);
	putque(this, quePos, -1, (this->curtick&0xFF)|(this->currow<<8)|(this->curord<<16));
}

int __attribute__ ((visibility ("internal"))) loadsamples (struct cpifaceSessionAPI_t *cpifaceSession, struct it_module *m)
{
	return cpifaceSession->mcpDevAPI->LoadSamples (cpifaceSession, m->sampleinfos, m->nsampi);
}

int __attribute__ ((visibility ("internal"))) play (struct itplayer *this, const struct it_module *m, int ch, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int i;

	if (!cpifaceSession->mcpDevAPI)
	{
		return errPlay;
	}

	staticthis=this;
	this->randseed=1;
	this->patdelayrow=0;
	this->patdelaytick=0;
	this->gotoord=0;
	this->gotorow=0;
	this->endord=m->endord;
	this->nord=m->nord;
	this->nchan=m->nchan;
	this->orders=m->orders;
	this->patlens=m->patlens;
	this->patterns=m->patterns;
	this->ninst=m->ninst;
	this->instruments=m->instruments;
	this->nsamp=m->nsamp;
	this->samples=m->samples;
	this->sampleinfos=m->sampleinfos;
	this->nsampi=m->nsampi;
	this->midicmds=m->midicmds;
	this->speed=m->inispeed;
	this->tempo=m->initempo;
	this->gvol=m->inigvol;
	this->chsep=m->chsep;
	this->linear=m->linear;
	this->oldfx=!!m->oldfx;
	this->instmode=m->instmode;
	this->geffect=m->geffect;
	this->curtick=this->speed-1;
	this->currow=0;
	this->realpos=0;
	this->pitchhigh=-0x6000;
	this->pitchlow=0x6000;
	this->realsynctime=0;
	this->realsync=0;
	this->realtempo=this->tempo;
	this->realspeed=this->speed;
	this->realgvol=this->gvol;

	this->curord=0;
	while (this->orders[this->curord]==0xFFFF && this->curord<this->nord)
		this->curord++;

	if (this->curord==this->nord)
	{
		return errFormStruc;
	}

	this->channels=malloc(sizeof(struct it_logchan)*this->nchan);
	this->pchannels=malloc(sizeof(struct it_physchan)*ch);
	this->quelen=500;
	this->que=malloc(sizeof(int)*this->quelen*4);
	if (!this->channels||!this->pchannels||!this->que)
	{
		if (this->channels)
		{
			free(this->channels);
			this->channels=NULL;
		};
		if (this->pchannels)
		{
			free(this->pchannels);
			this->pchannels=NULL;
		}
		if (this->que)
		{
			free(this->que);
			this->que=NULL;
		}
		return errFormStruc;
	}
	this->querpos=this->quewpos=0;
	memset(this->channels, 0, sizeof(*this->channels)*this->nchan);
	memset(this->pchannels, 0, sizeof(*this->pchannels)*ch);
	for (i=0; i<ch; i++)
		this->pchannels[i].lch=-1;
	for (i=0; i<this->nchan; i++)
	{
		struct it_logchan *c=&this->channels[i];
		c->newchan.lch=i;
		c->cvol=m->inivol[i];
		c->cpan=m->inipan[i]&127;
		c->srnd=c->cpan==100;
		c->disabled=m->inipan[i]&128;
		c->retrigspd=1;
		c->tremoron=1;
		c->tremoroff=1;
		c->tremoroncounter=0;
		c->tremoroffcounter=0;
	}

	if (!cpifaceSession->mcpDevAPI->OpenPlayer(ch, playtickstatic, file, cpifaceSession))
	{
		return errPlay;
	}

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeDefaultPlayW);

	this->npchan = cpifaceSession->PhysicalChannelCount;

	return errOk;
}

void __attribute__ ((visibility ("internal"))) stop (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this)
{
	cpifaceSession->mcpDevAPI->ClosePlayer (cpifaceSession);
	if (this->channels)
	{
		free(this->channels);
		this->channels=NULL;
	};
	if (this->pchannels)
	{
		free(this->pchannels);
		this->pchannels=NULL;
	}
	if (this->que)
	{
		free(this->que);
		this->que=NULL;
	}
}

int __attribute__ ((visibility ("internal"))) getpos(struct itplayer *this)
{
	if (this->manualgoto)
		return (this->gotorow<<8)|(this->gotoord<<16);
	return (this->curtick&0xFF)|(this->currow<<8)|(this->curord<<16);
}

int __attribute__ ((visibility ("internal"))) getrealpos (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this)
{
	readque (cpifaceSession, this);
	return this->realpos;
}

int __attribute__ ((visibility ("internal"))) getchansample (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int16_t *buf, int len, uint32_t rate, int opt)
{
	int i,n;
	unsigned int chn[64];
	n=0;
	for (i=0; i<this->npchan; i++)
		if (this->pchannels[i].lch==ch)
			chn[n++]=i;
	cpifaceSession->mcpMixChanSamples (cpifaceSession, chn, n, buf, len, rate, opt);
	return 1;
}

void __attribute__ ((visibility ("internal"))) itplayer_getrealvol (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int *l, int *r)
{
	int i, voll, volr;

	*l = *r = 0;

	for (i=0; i<this->npchan; i++)
		if (this->pchannels[i].lch==ch)
		{
			cpifaceSession->mcpGetRealVolume (i, &voll, &volr);
			(*l)+=voll;
			(*r)+=volr;
		}
}

void __attribute__ ((visibility ("internal"))) setpos(struct itplayer *this, int ord, int row)
{
	int i;
	if (this->curord!=ord)
		for (i=0; i<this->npchan; i++)
			this->pchannels[i].notecut=1;
	this->curtick=this->speed-1;
	this->patdelaytick=0;
	this->patdelayrow=0;
	if ((ord==this->curord)&&(row>this->patlens[this->orders[this->curord]]))
	{
		row=0;
		ord++;
	}
	this->gotorow=(row>0xFF)?0xFF:(row<0)?0:row;
	this->gotoord=((ord>=this->nord)||(ord<0))?0:ord;
	this->manualgoto=1;
	this->querpos=this->quewpos=0;
	this->realpos=(this->gotorow<<8)|(this->gotoord<<16);
}

int __attribute__ ((visibility ("internal"))) getdotsdata (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int pch, int *smp, int *note, int *voll, int *volr, int *sus)
{
	struct it_physchan *p;
	for (; pch<this->npchan; pch++)
		if ((this->pchannels[pch].lch==ch)&&!this->pchannels[pch].dead)
			break;
	if (pch>=this->npchan)
		return -1;
	p=&this->pchannels[pch];
	*smp=p->smp->handle;

	if (this->linear)
		*note=p->noteoffset+p->fpitch;
	else
		if (p->noteoffset+p->fpitch)
			*note=p->noteoffset+cpifaceSession->mcpAPI->GetNote8363(6848*8363/p->fpitch);
		else
			*note=0;

	cpifaceSession->mcpGetRealVolume (p->no, voll, volr);
	*sus=!(p->noteoff||p->notefade);
	return pch+1;
}

void __attribute__ ((visibility ("internal"))) getglobinfo (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int *tmp, int *bp, int *gv, int *gs)
{
	readque (cpifaceSession, this);
	*tmp=this->realspeed;
	*bp=this->realtempo;
	*gv=this->realgvol;
	*gs=this->gvolslide?(this->gvolslide>0?ifxGVSUp:ifxGVSDown):0;
}

int __attribute__ ((visibility ("internal"))) getsync (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int *time)
{
	readque (cpifaceSession, this);
	if ((ch<0)||(ch>=this->nchan))
	{
		*time = gettime(cpifaceSession) - this->realsynctime;
		return this->realsync;
	} else {
		*time = gettime(cpifaceSession) - this->channels[ch].realsynctime;
		return this->channels[ch].realsync;
	}
}
/*
int __attribute__ ((visibility ("internal"))) getticktime(struct itplayer *this)
{
	readque(this);
	return 65536*5/(2*this->realtempo);
}

int __attribute__ ((visibility ("internal"))) getrowtime(struct itplayer *this)
{
	readque(this);
	return 65536*5*this->realspeed/(2*this->realtempo);
}*/
/*
void __attribute__ ((visibility ("internal"))) setevpos(struct itplayer *this, int ch, int pos, int modtype, int mod)
{
	struct it_logchan *c;
	if ((ch<0)||(ch>=this->nchan))
		return;
	c=&this->channels[ch];
	c->evpos0=pos;
	c->evmodtype=modtype;
	c->evmod=mod;
	c->evmodpos=0;
	c->evpos=-1;
	c->evtime=-1;
}
*/
/*
int __attribute__ ((visibility ("internal"))) getevpos(struct itplayer *this, int ch, int *time)
{
	readque(this);
	if ((ch<0)||(ch>=this->nchan))
	{
		*time=-1;
		return -1;
	}
	*time=gettime()-this->channels[ch].evtime;
	return this->channels[ch].evpos;
}
*/
/*
int __attribute__ ((visibility ("internal"))) findevpos(struct itplayer *this, int pos, int *time)
{
	int i;
	readque(this);
	for (i=0; i<this->nchan; i++)
		if (this->channels[i].evpos==pos)
			break;
	*time=gettime()-this->channels[i].evtime;
	return this->channels[i].evpos;
}
*/

void __attribute__ ((visibility ("internal"))) setloop(struct itplayer *this, int s)
{
	this->noloop=!s;
}

int __attribute__ ((visibility ("internal"))) getloop(struct itplayer *this)
{
	return this->looped;
}

int __attribute__ ((visibility ("internal"))) chanactive (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int *lc)
{
	struct it_physchan *p=&this->pchannels[ch];
	*lc=p->lch;
	if (!(*lc>-1)&&p->smp&&p->fvol)
		return 0; /* force mcpGet to be checked last - stian */
	return cpifaceSession->mcpGet(ch, mcpCStatus);
}

int __attribute__ ((visibility ("internal"))) lchanactive (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int lc)
{
	struct it_physchan *p=this->channels[lc].pch;
	if (!p)
		return 0; /* avoid strange crashes on some archs - stian */
	if (!(p->smp&&p->fvol))
		return 0; /* force mcpGet to be checked last - stian */
	return cpifaceSession->mcpGet (p->no, mcpCStatus);
}

int __attribute__ ((visibility ("internal"))) getchanins(struct itplayer *this, int ch)
{
	struct it_physchan *p=&this->pchannels[ch];
	return p->inst->handle+1;
}

int __attribute__ ((visibility ("internal"))) getchansamp(struct itplayer *this, int ch)
{
	struct it_physchan *p=&this->pchannels[ch];
	if (!p->smp)
		return 0xFFFF;
	return p->smp->handle;
}


void __attribute__ ((visibility ("internal"))) getchaninfo(struct itplayer *this, uint8_t ch, struct it_chaninfo *ci)
{
	const struct it_logchan *t=&this->channels[ch];
	if (t->pch)
	{
		ci->ins=getchanins(this, t->pch->no);
		ci->smp=getchansamp(this, t->pch->no);
		ci->note=t->curnote+11;
		ci->vol=t->vol;
		if (!t->pch->fadeval)
			ci->vol=0;
		ci->pan=t->srnd?16:t->pan>>2;
		ci->notehit=t->fnotehit;
		ci->volslide=t->fvolslide;
		ci->pitchslide=t->fpitchslide;
		ci->panslide=t->fpanslide;
		ci->volfx=t->fvolfx;
		ci->pitchfx=t->fpitchfx;
		ci->notefx=t->fnotefx;
		ci->fx=t->fx;
	} else
		memset(ci, 0, sizeof(*ci));
}

int __attribute__ ((visibility ("internal"))) getchanalloc(struct itplayer *this, uint8_t ch)
{
	int num=0;
	int i;
	for (i=0; i<this->npchan; i++)
	{
		struct it_physchan *p=&this->pchannels[i];
		if (p->lch==ch && !p->dead)
			num++;
	}
	return num;
}
