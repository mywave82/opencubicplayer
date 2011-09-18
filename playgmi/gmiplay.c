/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay - SMF file player using GUS patches
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
 *    -implemented sustain pedal handling (yes, really :)
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "gmiplay.h"

#define MAXCHAN 64
#define MAXCHANNOTE 32

struct mtrackdata
{
	struct miditrack pos;
	uint32_t tickpos;
	uint8_t cmd;
};

struct pchandata
{
	uint8_t mch;
	uint8_t notenum;
	struct msample *samp;
	uint8_t sus;
	uint8_t epos;
	int32_t curvol;
	uint32_t resvol;
	int16_t resfrq;
	uint8_t hold;

	uint16_t vibpos;
	uint16_t trempos;
	uint16_t vibswp;
	uint16_t tremswp;
};

struct mchandata
{
	uint8_t ins;
	uint8_t pan;
	uint8_t reverb;
	uint8_t chorus;
	int16_t pitch;
	uint8_t gvol;
	uint16_t rpn;
	uint8_t pitchsens;
	uint8_t mute;
	uint8_t susp;

	uint8_t note[MAXCHANNOTE];
	int16_t noteval[MAXCHANNOTE];
	uint8_t vol[MAXCHANNOTE];
	uint8_t pch[MAXCHANNOTE];
};

static uint32_t tempo;
static uint16_t quatertick;
static uint16_t tracknum;
static struct miditrack *tracks;
static struct minstrument *instr;
static uint32_t ticknum;
static uint32_t curtick;
static uint32_t outtick;
static struct mtrackdata trk[64];
static struct mchandata mchan[16];
static struct pchandata pchan[MAXCHAN];
static uint8_t instmap[128];
static uint8_t channelnum;
static uint8_t drumchannel2=16;

static int looped;
static int donotloop;

static inline uint32_t getvlnum(uint8_t *ptr)
{
	uint32_t num=0;
	while (1)
	{
		num=(num<<7)|(*ptr&0x7F);
		if (!(*ptr++&0x80))
			break;
	}
	return num;
}

static inline uint32_t readvlnum(uint8_t **ptr)
{
	uint32_t num=0;
	while (1)
	{
		num=(num<<7)|(*(*ptr)&0x7F);
		if (!(*(*ptr)++&0x80))
			break;
	}
	return num;
}

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

static inline uint8_t getlinvolg(uint16_t v)
{
	v>>=4;
	return ((v&0xFF)|0x100)>>(16-(v>>8));
}

static uint16_t logvoltab[16]=
{
	32768, 34219, 35734, 37316,
	38968, 40693, 42495, 44376,
	46341, 48393, 50535, 52773,
	55109, 57549, 60097, 62757
};
static uint16_t logvoltabf[16]=
{
	32768, 32857, 32946, 33035,
	33125, 33215, 33305, 33395,
	33486, 33576, 33667, 33759,
	33850, 33942, 34034, 34126
};

static inline uint32_t getlinvol(int16_t v)
{
	uint16_t m=(logvoltab[(v>>4)&0xF]*logvoltabf[v&0xF])>>15;
	if (v<0)
		return m>>(-(v>>8));
	else
		return m<<(v>>8);
}


static void playnote(uint8_t chan,
                     uint8_t note,
                     uint8_t vol)
{
	struct mchandata *mc=&mchan[chan];
	struct pchandata *pc;
	struct msample *samp;
	uint8_t nextchan=0xFF;
	int i;
	int16_t noteval;

	if (note>=sizeof(instr[mc->ins].note))
	{
		fprintf(stderr, "[gmiplay] #1 note out of range: %d\n", note);
		return;
	}

	if (instr[mc->ins].note[note]==0xFF)
		return;

	nextchan=0xFF;

	for (i=0; i<MAXCHANNOTE; i++)
		if (mc->note[i]==note)
			break;
	if (i!=MAXCHANNOTE)
		nextchan=mc->pch[i];
	else {
		for (i=0; i<channelnum; i++)
			if (pchan[i].mch==0xFF)
				break;
		if (i!=channelnum)
			nextchan=i;
		else {
			uint32_t minvol=0xFFFFFFFF;
			for (i=0; i<channelnum; i++)
				if (!pchan[i].sus&&(pchan[i].resvol<minvol))
				{
					nextchan=i;
					minvol=pchan[i].resvol;
				}
			if (nextchan!=0xFF)
			{
				for (i=0; i<channelnum; i++)
					if (pchan[i].resvol<minvol)
					{
						nextchan=i;
						minvol=pchan[i].resvol;
					}
			}
		}
	}

	pc=&pchan[nextchan];

	samp=&instr[mc->ins].samples[instr[mc->ins].note[note]];
	mcpSet(nextchan, mcpCInstrument, samp->handle);

	noteval=(note-samp->sclbas)*samp->sclfac+samp->sclbas*256-samp->normnote+12*256;
	mcpSet(nextchan, mcpCPosition, 0);
	mcpSet(nextchan, mcpCLoop, 1);
	mcpSet(nextchan, mcpCDirect, 0);
	mcpSet(nextchan, mcpCPitch, noteval);
	mcpSet(nextchan, mcpCStatus, 1);

	if (pc->mch!=0xFF)
		mchan[pc->mch].note[pc->notenum]=0xFF;
	for (i=0; i<MAXCHANNOTE; i++)
		if (mc->note[i]==0xFF)
			break;
	mc->note[i]=note;
	mc->noteval[i]=noteval;
	mc->pch[i]=nextchan;
	mc->vol[i]=vol;
	pc->mch=chan;
	pc->notenum=i;
	pc->samp=samp;
	pc->epos=0;
	pc->curvol=0;
	pc->sus=1;
	pc->hold=0;
	pc->resvol=0xFFFFFFFE;
	pc->trempos=0;
	pc->tremswp=0;
	pc->vibpos=0;
	pc->vibswp=0;
}




static void noteoff(uint8_t chan, uint8_t note)
{
	struct mchandata *mc=&mchan[chan];
	int i;

	if (note>=sizeof(instr[mc->ins].note))
	{
		fprintf(stderr, "[gmiplay] #2 note out of range: %d\n", note);
		return;
	}

	for (i=0; i<MAXCHANNOTE; i++)
		if ((mc->note[i]==note)&&pchan[mc->pch[i]].sus)
			break;
	if (i==MAXCHANNOTE)
		return;

	if (!mc->susp)
	{
		if (pchan[mc->pch[i]].samp->sustain!=7)
			pchan[mc->pch[i]].epos=pchan[mc->pch[i]].samp->sustain;
	} else
		pchan[mc->pch[i]].hold=1;

	pchan[mc->pch[i]].sus=0;
}




static void holdnotesoff(uint8_t chan)
{
	struct mchandata *mc=&mchan[chan];
	int i;

	for (i=0; i<MAXCHANNOTE; i++)
		if (mc->pch[i]!=0xFF && pchan[mc->pch[i]].hold)
		{
			if (pchan[mc->pch[i]].samp->sustain!=7)
				pchan[mc->pch[i]].epos=pchan[mc->pch[i]].samp->sustain;
			pchan[mc->pch[i]].hold=pchan[mc->pch[i]].sus=0;
		}
}




static void setnotevol(uint8_t chan,
                       uint8_t note,
                       uint8_t vol)
{
	struct mchandata *mc=&mchan[chan];
	int i;

	for (i=0; i<MAXCHANNOTE; i++)
		if (mc->note[i]==note)
			break;
	if (i==MAXCHANNOTE)
		return;

	mc->vol[i]=vol;
}

static void playticks(struct mtrackdata *t, uint32_t ticks)
{
	int i;
	while (t->pos.trk<t->pos.trkend)
	{
		if ((ticks+t->tickpos)<getvlnum(t->pos.trk))
		{
			t->tickpos+=ticks;
			break;
		}
		/* these three readvlnum should be safe, since gmiload.c accepted them */
		ticks-=readvlnum(&t->pos.trk)-t->tickpos;
		t->tickpos=0;
		if (*t->pos.trk&0x80)
			t->cmd=*t->pos.trk++;
		if ((t->cmd==0xF0)||(t->cmd==0xF7))
		{
			t->pos.trk+=readvlnum(&t->pos.trk);
		} else
			if (t->cmd==0xFF)
			{
				uint8_t cmd=*t->pos.trk++;
				uint32_t len=readvlnum(&t->pos.trk);
				switch (cmd)
				{
					case 0x51:
						tempo=((t->pos.trk[0]<<16)|(t->pos.trk[1]<<8)|t->pos.trk[2])/quatertick;
						break;
				}
				t->pos.trk+=len;
			} else {
				uint8_t cc=t->cmd&15;
				switch (t->cmd&0xF0)
				{
					case 0x80:
						noteoff(cc, t->pos.trk[0]);
						t->pos.trk+=2;
						break;
					case 0x90:
						if (!t->pos.trk[1])
							noteoff(cc, t->pos.trk[0]);
						else
							playnote(cc, t->pos.trk[0], t->pos.trk[1]);
						t->pos.trk+=2;
						break;
					case 0xA0:
						setnotevol(cc, t->pos.trk[0], t->pos.trk[1]);
						t->pos.trk+=2;
						break;
					case 0xB0:
						switch (*t->pos.trk++)
						{
							case 0x06:
								switch (mchan[cc].rpn)
								{
									case 0:
										mchan[cc].pitchsens=*t->pos.trk;
										break;
								}
								break;
							case 0x26:
								switch (mchan[cc].rpn)
								{
								}
								break;
							case 0x07:
								mchan[cc].gvol=*t->pos.trk;
								break;
							case 0x40:
								if (*t->pos.trk>0x3f)
									mchan[cc].susp=1;
								else {
									mchan[cc].susp=0;
									holdnotesoff(cc);
								}
								break;
							case 0x0A:
								mchan[cc].pan=*t->pos.trk<<1;
								break;
							case 0x5B:
								mchan[cc].reverb=*t->pos.trk;
								break;
							case 0x5D:
								mchan[cc].chorus=*t->pos.trk;
								break;
							case 0x65:
								mchan[cc].rpn=(mchan[cc].rpn&0xFF)|(*t->pos.trk<<8);
								break;
							case 0x64:
								mchan[cc].rpn=(mchan[cc].rpn&0xFF00)|*t->pos.trk;
								break;
							case 0x78:
								for (i=0; i<MAXCHANNOTE; i++)
									if (mchan[cc].note[i]!=0xFF)
									{
										mcpSet(mchan[cc].pch[i], mcpCStatus, 0);
										pchan[mchan[cc].pch[i]].mch=0xFF;
										mchan[cc].note[i]=0xFF;
									}
								break;
							case 0x7B:
							case 0x7C:
							case 0x7D:
							case 0x7E:
							case 0x7F:
								mchan[cc].susp=0;
								for (i=0; i<MAXCHANNOTE; i++)
									if (mchan[cc].note[i]!=0xFF)
										noteoff(cc, mchan[cc].note[i]);
								break;
						}
						t->pos.trk++;
						break;
					case 0xC0:
						if ((cc!=9)&&(cc!=drumchannel2))
							mchan[cc].ins=instmap[*t->pos.trk];
						t->pos.trk++;
						break;
					case 0xD0:
						t->pos.trk+=1;
						break;
					case 0xE0:
						mchan[cc].pitch=((t->pos.trk[1]-0x40)<<7)|t->pos.trk[0];
						t->pos.trk+=2;
						break;
				}
			}
	}
}

static void _rewind(void)
{
	int i;

	curtick=0;
	for (i=0; i<tracknum; i++)
	{
		trk[i].pos=tracks[i];
		trk[i].tickpos=0;
	}
	for (i=0; i<channelnum; i++)
		if (pchan[i].mch!=0xFF)
			noteoff(pchan[i].mch, mchan[pchan[i].mch].note[pchan[i].notenum]);
}

static void PlayTicks(uint32_t ticks)
{
	while (ticks)
	{
		uint32_t c;
		int i;

		if ((curtick+ticks)>=ticknum)
			c=ticknum-curtick;
		else
			c=ticks;
		for (i=0; i<tracknum; i++)
			playticks(&trk[i], c);
		ticks-=c;
		curtick+=c;
		if (curtick==ticknum)
		{
			looped=1;
			_rewind();
		}
	}
}

static void PlayTick(void)
{
	static uint32_t tickmod;
	uint32_t tickwidth=(1000000+tickmod)/(tempo*64);
	int i;

	tickmod=(1000000+tickmod)%(tempo*64);
	if (curtick!=outtick)
	{
		if (outtick>curtick)
			PlayTicks(outtick-curtick);
		else
			if (!outtick)
				_rewind();
	}
	PlayTicks(tickwidth);
	outtick=curtick;
	for (i=0; i<channelnum; i++)
		if (pchan[i].mch!=0xFF)
		{
			struct pchandata *pc=&pchan[i];
			struct mchandata *mc=&mchan[pc->mch];
			uint8_t stepover=0;
			struct msample *e=pc->samp;

			mcpSet(i, mcpCMute, mc->mute);
			e=pc->samp;
			if (pc->curvol>e->volpos[pc->epos])
			{
				pc->curvol-=e->volrte[pc->epos];
				if (pc->curvol<=e->volpos[pc->epos])
				{
					pc->curvol=e->volpos[pc->epos];
					stepover=1;
				}
			} else {
				pc->curvol+=e->volrte[pc->epos];
				if (pc->curvol>=e->volpos[pc->epos])
				{
					pc->curvol=e->volpos[pc->epos];
					stepover=1;
				}
			}
/*
			pc->resvol=(getlinvolg(pc->curvol)*(getlinvol((mc->vol[pc->notenum]-0x80)*8)>>8)*mc->gvol)>>14; */
			pc->resvol=(((uint32_t)getlinvolg(pc->curvol))*((int)mc->vol[pc->notenum])*((int)mc->gvol))>>14;
			pc->resfrq=((int)mc->noteval[pc->notenum])+((((int)mc->pitch)*((int)mc->pitchsens))>>5);
			if ((pc->epos+1)>=e->sustain)
			{
				uint16_t curvdep;
				uint16_t curtdep;
				if (pc->vibswp<e->vibswp)
					curvdep=((uint32_t)e->vibdep)*((int)pc->vibswp++)/(int)e->vibswp;
				else
					curvdep=e->vibdep;
				if (pc->tremswp<e->tremswp)
					curtdep=((uint32_t)e->tremdep)*((int)pc->tremswp++)/(int)e->tremswp;
				else
					curtdep=e->tremdep;
				pc->resfrq+=((uint32_t)curvdep)*((int)sintab[(pc->vibpos>>8)&0xFF])/2048;
				pc->resvol=((uint32_t)pc->resvol)*((int)getlinvol(((uint32_t)curtdep)*sintab[(pc->trempos>>8)&0xFF]/2048))/32768;
				pc->vibpos+=e->vibrte;
				pc->trempos+=e->tremrte;
			}

			mcpSet(i, mcpCVolume, (looped&&donotloop)?0:pc->resvol);
			mcpSet(i, mcpCPanning, mc->pan-0x80);

			mcpSet(i, mcpCPitch, pc->resfrq);
			mcpSet(i, mcpCReverb, mc->reverb<<1);
			mcpSet(i, mcpCChorus, mc->chorus<<1);

			if (stepover)
			{
				if ((pc->epos+1)!=e->sustain)
				{
					pc->epos++;
					if (pc->epos==e->end)
					{
						mcpSet(i, mcpCStatus, 0);
						pc->mch=0xFF;
						mc->note[pc->notenum]=0xFF;
					}
				}
			}
		}
}

int __attribute__ ((visibility ("internal"))) midPlayMidi(const struct midifile *m, uint8_t voices)
{
	int i;

	if (voices>MAXCHAN)
		voices=MAXCHAN;

	for (i=65; i<=128; i++)
		sintab[i]=sintab[128-i];
	for (i=129; i<256; i++)
		sintab[i]=-sintab[256-i];

	drumchannel2=(m->opt&MID_DRUMCH16)?15:16;

	looped=0;
	instr=m->instruments;
	quatertick=m->tempo;
	tracknum=m->tracknum;
	tracks=m->tracks;
	tempo=500000/quatertick;
	outtick=0;
	outtick=0;
	ticknum=m->ticknum;
	for (i=0; i<tracknum; i++)
	{
		trk[i].pos=tracks[i];
		trk[i].tickpos=0;
	}
	memcpy(instmap, m->instmap, 128);
	for (i=0; i<MAXCHAN; i++)
		pchan[i].mch=0xFF;
	for (i=0; i<16; i++)
	{
		memset(mchan[i].note, 0xFF, MAXCHANNOTE);
		mchan[i].gvol=0x7F;
		mchan[i].pan=0x80;
		mchan[i].reverb=0;
		mchan[i].chorus=0;
		mchan[i].ins=((i==9)||(i==drumchannel2))?m->instmap[128]:0;
		mchan[i].pitch=0;
		mchan[i].mute=0;
		mchan[i].rpn=0x7F7F;
		mchan[i].pitchsens=2;
	}

	channelnum=1;
	if (!mcpOpenPlayer(voices, PlayTick))
		return 0;
	channelnum=mcpNChan;
	mcpSet(-1, mcpGSpeed, 256*64);
	return 1;
}

void __attribute__ ((visibility ("internal"))) midStopMidi(void)
{
	mcpClosePlayer();
}

uint32_t __attribute__ ((visibility ("internal"))) midGetPosition(void)
{
	return outtick;
}

void __attribute__ ((visibility ("internal"))) midSetPosition(uint32_t pos)
{
	if (pos>=ticknum)
		pos=0;
	outtick=pos;
}

void __attribute__ ((visibility ("internal"))) midGetChanInfo(uint8_t ch, struct mchaninfo *ci)
{
	int i,j;

	ci->ins=mchan[ch].ins;
	ci->pan=mchan[ch].pan;
	ci->gvol=mchan[ch].gvol;
	ci->reverb=mchan[ch].reverb;
	ci->chorus=mchan[ch].chorus;
	ci->pedal=mchan[ch].susp;
	ci->pitch=((mchan[ch].pitch*mchan[ch].pitchsens)>>5);
	ci->notenum=0;
	for (i=0; i<MAXCHANNOTE; i++)
		if (mchan[ch].note[i]!=0xFF)
		{
			ci->note[ci->notenum]=mchan[ch].note[i];
			ci->opt[ci->notenum]=pchan[mchan[ch].pch[i]].sus;
			ci->vol[ci->notenum++]=mchan[ch].vol[i];
		}
	for (i=0; i<ci->notenum; i++)
		for (j=i+1; j<ci->notenum; j++)
			if ( ((ci->note[i]>ci->note[j])&&((ci->opt[j]&1)==(ci->opt[i]&1))) || ((ci->opt[i]&1)<(ci->opt[j]&1)) )
			{
				uint8_t a;
				a=ci->note[i]; ci->note[i]=ci->note[j]; ci->note[j]=a;
				a=ci->opt[i]; ci->opt[i]=ci->opt[j]; ci->opt[j]=a;
				a=ci->vol[i]; ci->vol[i]=ci->vol[j]; ci->vol[j]=a;
			}
}

void __attribute__ ((visibility ("internal"))) midGetRealNoteVol(uint8_t ch, struct mchaninfo2 *ci)
{
	struct mchandata *mc=&mchan[ch];
	int i;

	ci->notenum=0;
	ci->mute=mc->mute;
	for (i=0; i<MAXCHANNOTE; i++)
		if (mc->note[i]!=0xFF)
		{
			int l,r;
			struct pchandata *pc=&pchan[mc->pch[i]];
			mcpGetRealVolume(mc->pch[i], &l, &r);
			ci->voll[ci->notenum]=l;
			ci->volr[ci->notenum]=r;
			ci->opt[ci->notenum]=pc->sus;
			ci->note[ci->notenum]=pc->resfrq+pc->samp->normnote-12*256;
			ci->ins[ci->notenum++]=(instr[mc->ins].prognum==0x80)?(0x80+pc->samp->sampnum):instr[mc->ins].prognum;
		}
}

void __attribute__ ((visibility ("internal"))) midGetGlobInfo(struct mglobinfo *gi)
{
	gi->curtick=curtick;
	gi->ticknum=ticknum;
	if (tempo)
		gi->speed=1000000/tempo;
	else
		gi->speed=0;
}

void __attribute__ ((visibility ("internal"))) midSetMute(int ch, int p)
{
	mchan[ch].mute=p;
}

uint8_t __attribute__ ((visibility ("internal"))) midGetMute(uint8_t ch)
{
	return mchan[ch].mute;
}

int __attribute__ ((visibility ("internal"))) midLooped(void)
{
	return looped;
}

void __attribute__ ((visibility ("internal"))) midSetLoop(int s)
{
	donotloop=!s;
}

int __attribute__ ((visibility ("internal"))) midGetChanSample(unsigned int ch, int16_t *buf, unsigned int len, uint32_t rate, int opt)
{
	int i,n;
	unsigned int chn[MAXCHAN];
	n=0;
	for (i=0; i<MAXCHANNOTE; i++)
		if (mchan[ch].note[i]!=0xFF)
			chn[n++]=mchan[ch].pch[i];
	mcpMixChanSamples(chn, n, buf, len, rate, opt);
	return 1;
}

int __attribute__ ((visibility ("internal"))) mid_loadsamples(struct midifile *m)
{
	return mcpLoadSamples(m->samples, m->sampnum);
}
