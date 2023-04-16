#ifndef _ITPLAY_H
#define _ITPLAY_H 1

struct sampleinfo; /* dev/mcp.h */
#define it_sampleinfo sampleinfo

struct it_envelope
{
	int len;
	int loops, loope;
	int sloops, sloope;
	int type;
	enum
	{
		env_type_active=1, env_type_looped=2, env_type_slooped=4, env_type_carry=8, env_type_filter=128
	} env_types;
	uint16_t x[26];
	int8_t y[28];
};

struct it_sample
{
	char name[32];
	char packed;
	uint16_t handle;
	int16_t normnote;
	uint8_t gvl;
	uint8_t vol;
	uint8_t vis;
	uint8_t vid;
	uint8_t vit;
	uint8_t vir;
	uint8_t dfp;
};

#define IT_KEYTABS 120
struct it_instrument
{
	char name[32];
	uint8_t handle;
	uint8_t keytab[IT_KEYTABS][2];
	int32_t fadeout;
	struct it_envelope envs[3];
	uint8_t nna;
	uint8_t dct;
	uint8_t dca;
	uint8_t pps;
	uint8_t ppc;
	uint8_t gbv;
	uint8_t dfp;
	uint8_t rv;
	uint8_t rp;
	uint8_t ifp;
	uint8_t ifr;
	uint8_t mch;
	uint8_t mpr;
	uint16_t midibnk;
};

struct it_chaninfo
{
	uint8_t ins;
	int smp;
	uint8_t note;
	uint8_t vol;
	uint8_t pan;
	uint8_t notehit;
	uint8_t volslide;
	uint8_t pitchslide;
	uint8_t panslide;
	uint8_t volfx;
	uint8_t pitchfx;
	uint8_t notefx;
	uint8_t fx;
};


struct it_module
{
	char name[32];
	int nchan;
	int ninst;
	int nsampi;
	int nsamp;
	int npat;
	int nord;
	int linearfreq;
	int endord;
	char **message;
	char **midicmds;
	uint16_t *orders;
	uint16_t *patlens;
	uint8_t **patterns;
	struct it_sample *samples;
	struct it_instrument *instruments;
	struct it_sampleinfo *sampleinfos;
	int deltapacked;
	int inispeed;
	int initempo;
	int inigvol;
	uint8_t inipan[64];
	uint8_t inivol[64];
	int chsep;
	int linear;
	int oldfx;
	int instmode;
	int geffect;
};

struct cpifaceSessionAPI_t;
struct ocpfilehandle_t;
OCP_INTERNAL int  it_load (struct cpifaceSessionAPI_t *cpifaceSession, struct it_module *, struct ocpfilehandle_t *); /* done */
OCP_INTERNAL void it_free (struct it_module *); /* done */
OCP_INTERNAL void it_optimizepatlens (struct it_module *); /* done */
OCP_INTERNAL int  it_precalctime (struct it_module *, int startpos, int (*calctimer)[2], int calcn, int ite); /* done */

OCP_INTERNAL int decompress8 (struct cpifaceSessionAPI_t *cpifaceSession, struct ocpfilehandle_t *, void *dst, int len, char it215); /* done */
OCP_INTERNAL int decompress16(struct cpifaceSessionAPI_t *cpifaceSession, struct ocpfilehandle_t *, void *dst, int len, char it215); /* done */

enum
{
	cmdSpeed=1, cmdJump=2, cmdBreak=3, cmdVolSlide=4, cmdPortaD=5,
	cmdPortaU=6, cmdPortaNote=7, cmdVibrato=8, cmdTremor=9, cmdArpeggio=10,
	cmdVibVol=11, cmdPortaVol=12, cmdChanVol=13, cmdChanVolSlide=14,
	cmdOffset=15, cmdPanSlide=16, cmdRetrigger=17, cmdTremolo=18,
	cmdSpecial=19, cmdTempo=20, cmdFineVib=21, cmdGVolume=22,
	cmdGVolSlide=23, cmdPanning=24, cmdPanbrello=25, cmdMIDI=26,

	cmdSVibType=3, cmdSTremType=4, cmdSPanbrType=5, cmdSPatDelayTick=6,
	cmdSInstFX=7, cmdSPanning=8, cmdSSurround=9, cmdSOffsetHigh=10,
	cmdSPatLoop=11, cmdSNoteCut=12, cmdSNoteDelay=13, cmdSPatDelayRow=14,
	cmdSSetMIDIMacro=15,

	cmdSIPastCut=0, cmdSIPastOff=1, cmdSIPastFade=2, cmdSINNACut=3,
	cmdSINNACont=4, cmdSINNAOff=5, cmdSINNAFade=6, cmdSIVolEnvOff=7,
	cmdSIVolEnvOn=8, cmdSIPanEnvOff=9, cmdSIPanEnvOn=10,
	cmdSIPitchEnvOff=11, cmdSIPitchEnvOn=12,

	cmdVVolume=1, cmdVFVolSlU=66, cmdVFVolSlD=76, cmdVVolSlU=86,
	cmdVVolSlD=96, cmdVPortaD=106, cmdVPortaU=116, cmdVPanning=129,
	cmdVPortaNote=194, cmdVVibrato=204,

	cmdNNote=1, cmdNNoteFade=121, cmdNNoteCut=254, cmdNNoteOff=255,
	cmdSync=30303
};

enum
{
	ifxGVSUp=1, ifxGVSDown,
	ifxVSUp=1, ifxVSDown,
	ifxPSUp=1, ifxPSDown, ifxPSToNote,
	ifxPnSRight=1, ifxPnSLeft,
	ifxVXVibrato=1, ifxVXTremor,
	ifxPXVibrato=1, ifxPXArpeggio,
	ifxPnXVibrato=1,
	ifxNXNoteCut=1, ifxNXRetrig, ifxNXDelay,

	ifxVolSlideUp=1, ifxVolSlideDown,
	ifxRowVolSlideUp, ifxRowVolSlideDown,
	ifxPitchSlideUp, ifxPitchSlideDown, ifxPitchSlideToNote,
	ifxRowPitchSlideUp, ifxRowPitchSlideDown,
	ifxPanSlideRight, ifxPanSlideLeft,
	ifxVolVibrato, ifxTremor,
	ifxPitchVibrato, ifxArpeggio,
	ifxNoteCut, ifxRetrig,
	ifxOffset, ifxEnvPos,
	ifxDelay,
	ifxChanVolSlideUp, ifxChanVolSlideDown,
	ifxRowChanVolSlideUp, ifxRowChanVolSlideDown,
	ifxPastCut, ifxPastOff, ifxPastFade,
	ifxVEnvOff, ifxVEnvOn, ifxPEnvOff, ifxPEnvOn, ifxFEnvOff, ifxFEnvOn,
	ifxPanBrello
};

struct it_logchan;

struct it_physchan
{
	int no;
	int lch;
	struct it_logchan *lchp;
	const struct it_sample *smp;
	const struct it_instrument *inst;
	int note;
	int newsamp;
	int newpos;
	int vol;
	int fvol;
	int pan;
	int fpan;
	int cutoff;
	int fcutoff;
	int reso;
	int srnd;
	int pitch;
	int fpitch;
	int fadeval;
	int fadespd;
	int notefade;
	int notecut;
	int noteoff;
	int dead;
	int looptype;
	int volenv;
	int panenv;
	int pitchenv;
	int filterenv;
	int penvtype;
	int panenvpos;
	int volenvpos;
	int pitchenvpos;
	int filterenvpos;
	int noteoffset;
	int avibpos;
	int avibdep;
};

struct it_logchan
{
	struct it_physchan *pch;
	struct it_physchan newchan;
	int lastins;
	int curnote;
	int lastnote;
	int cvol;
	int vol;
	int fvol;
	int cpan;
	int pan;
	int fpan;
	int srnd;
	int pitch;
	int fpitch;
	int dpitch;
	int cutoff;
	int fcutoff;
	int reso;
	int mute;
	int disabled;
	int vcmd;
	int command;
	int specialcmd;
	int specialdata;
	int volslide;
	int cvolslide;
	int panslide;
	int gvolslide;
	int vibspd;
	int vibdep;
	int vibtype;
	int vibpos;
	int tremspd;
	int tremdep;
	int tremtype;
	int trempos;
	int panbrspd;
	int panbrdep;
	int panbrtype;
	int panbrpos;
	int panbrrnd;
	int arpeggio1;
	int arpeggio2;
	int offset;
	int porta;
	int vporta; /* volume column */
	int eporta; /* effect column */
	int portanote;
	int vportanote; /* volume column */
	int eportanote; /* volume column */
	int vvolslide;
	int retrigpos;
	int retrigspd;
	int retrigvol;
	int tremoroff;
	int tremoron;
	int tremoroncounter;
	int tremoroffcounter;
	int patloopstart;
	int patloopcount;
	int nna;
	int realnote;
	int basenote;
	int realsync;
	int realsynctime;
	uint8_t delayed[8];
	int tempo;
	int evpos0;
	int evmodtype;
	int evmod;
	int evmodpos;
	int evpos;
	int evtime;
	int sfznum;
	uint8_t fnotehit;
	uint8_t fvolslide;
	uint8_t fpitchslide;
	uint8_t fpanslide;
	uint8_t fvolfx;
	uint8_t fpitchfx;
	uint8_t fnotefx;
	uint8_t fx;
};



struct itplayer
{
	int randseed;

	int pitchhigh;
	int pitchlow;
	int gotoord;
	int gotorow;
	int manualgoto;
	int patdelayrow;
	int patdelaytick;
	uint8_t *patptr;
	int linear;
	int oldfx;
	int instmode;
	int geffect;
	int chsep;
	int speed;
	int tempo;
	int gvol;
	int gvolslide;
	int curtick;
	int currow;
	int curord;
	int endord;
	int nord;
	int nchan;
	int npchan;
	int ninst;
	int nsamp;
	int nsampi;
	int noloop;
	int looped;
	struct it_logchan *channels;
	struct it_physchan *pchannels;
	const struct it_instrument *instruments;
	const struct it_sample *samples;
	const struct it_sampleinfo *sampleinfos;
	const uint16_t *orders;
	uint8_t **patterns;
	const uint16_t *patlens;
	char **midicmds;

	int (*que)[4];
	int querpos;
	int quewpos;
	int quelen;
	int proctime;
	int realpos;
	int realsync;
	int realsynctime;
	int realtempo;
	int realspeed;
	int realgvol;

	enum
	{
		quePos, queSync, queTempo, queSpeed, queGVol
	} que_types;
};

struct cpifaceSessionAPI_t;
OCP_INTERNAL int loadsamples (struct cpifaceSessionAPI_t *cpifaceSession, struct it_module *m);
OCP_INTERNAL int itplay (struct itplayer *this, const struct it_module *m, int ch, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void itstop (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this);

OCP_INTERNAL int getsync (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int *time);
OCP_INTERNAL int getpos (struct itplayer *this);
OCP_INTERNAL int getrealpos (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this);
/* OCP_INTERNAL int getticktime (struct itplayer *this); */
/* OCP_INTERNAL int getrowtime (struct itplayer *this); */
/* OCP_INTERNAL void setevpos (struct itplayer *this, int ch, int pos, int modtype, int mod); */
/* OCP_INTERNAL int getevpos (struct itplayer *this, int ch, int *time); */
/* OCP_INTERNAL int findevpos (struct itplayer *this, int pos, int *time); */
OCP_INTERNAL void setpos (struct itplayer *this, int ord, int row);
OCP_INTERNAL void mutechan (struct itplayer *this, int c, int m);
OCP_INTERNAL void getglobinfo (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int *speed, int *bpm, int *gvol, int *gs);
OCP_INTERNAL int getchansample (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int16_t *buf, int len, uint32_t rate, int opt);
OCP_INTERNAL int getdotsdata (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int pch, int *smp, int *note, int *voll, int *volr, int *sus);
OCP_INTERNAL void itplayer_getrealvol (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int *l, int *r);
OCP_INTERNAL void setloop (struct itplayer *this, int s);
OCP_INTERNAL int getloop (struct itplayer *this);
OCP_INTERNAL int chanactive (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *this, int ch, int *lc);
OCP_INTERNAL int getchanins (struct itplayer *this, int ch);
OCP_INTERNAL int getchansamp (struct itplayer *this, int ch);
OCP_INTERNAL int lchanactive (struct cpifaceSessionAPI_t *cpifaceSession, struct itplayer *, int lc);
OCP_INTERNAL void getchaninfo (struct itplayer *this, uint8_t ch, struct it_chaninfo *ci);
OCP_INTERNAL int getchanalloc (struct itplayer *this, uint8_t ch);

OCP_INTERNAL void itpInstClear (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void itpInstSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct it_instrument *ins, int nins, const struct it_sample *smp, int nsmp, const struct it_sampleinfo *smpi, int type, void (*MarkyBoy)(struct cpifaceSessionAPI_t *cpifaceSession, uint8_t *, uint8_t *));

OCP_INTERNAL void itTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct it_module *mod);

extern OCP_INTERNAL struct itplayer itplayer;
#endif
