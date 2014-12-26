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
		active=1, looped=2, slooped=4, carry=8, filter=128
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

extern int __attribute__ ((visibility ("internal"))) it_load(struct it_module *, FILE *); /* done */
extern void __attribute__ ((visibility ("internal"))) it_free(struct it_module *); /* done */
extern void __attribute__ ((visibility ("internal"))) it_optimizepatlens(struct it_module *); /* done */
extern int __attribute__ ((visibility ("internal"))) it_precalctime(struct it_module *, int startpos, int (*calctimer)[2], int calcn, int ite); /* done */

extern int __attribute__ ((visibility ("internal"))) decompress8 (FILE *, void *dst, int len, char it215); /* done */
extern int __attribute__ ((visibility ("internal"))) decompress16(FILE *, void *dst, int len, char it215); /* done */

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

extern int __attribute__ ((visibility ("internal"))) loadsamples(struct it_module *m);/* - done */
extern int __attribute__ ((visibility ("internal"))) play(struct itplayer *this, const struct it_module *m, int ch); /* - done */
extern void __attribute__ ((visibility ("internal"))) stop(struct itplayer *this); /* - done */

extern int __attribute__ ((visibility ("internal"))) getsync(struct itplayer *this, int ch, int *time); /* - done */
extern int __attribute__ ((visibility ("internal"))) getpos(struct itplayer *this); /* - done */
extern int __attribute__ ((visibility ("internal"))) getrealpos(struct itplayer *this); /* - done */
/* extern int __attribute__ ((visibility ("internal"))) getticktime(struct itplayer *this); */ /* - done */
/* extern int __attribute__ ((visibility ("internal"))) getrowtime(struct itplayer *this); */ /* - done */
/* extern void __attribute__ ((visibility ("internal"))) setevpos(struct itplayer *this, int ch, int pos, int modtype, int mod); */ /* - done */
/* extern int __attribute__ ((visibility ("internal"))) getevpos(struct itplayer *this, int ch, int *time); */ /* - done */
/* extern int __attribute__ ((visibility ("internal"))) findevpos(struct itplayer *this, int pos, int *time); */ /* - done */
extern void __attribute__ ((visibility ("internal"))) setpos(struct itplayer *this, int ord, int row); /* - done */
extern void __attribute__ ((visibility ("internal"))) mutechan(struct itplayer *this, int c, int m); /* - done */
extern void __attribute__ ((visibility ("internal"))) getglobinfo(struct itplayer *this, int *speed, int *bpm, int *gvol, int *gs); /* - done */
extern int __attribute__ ((visibility ("internal"))) getchansample(struct itplayer *this, int ch, int16_t *buf, int len, uint32_t rate, int opt); /* - done */
extern int __attribute__ ((visibility ("internal"))) getdotsdata(struct itplayer *this, int ch, int pch, int *smp, int *note, int *voll, int *volr, int *sus); /* - done */
extern void __attribute__ ((visibility ("internal"))) itplayer_getrealvol(struct itplayer *this, int ch, int *l, int *r); /* - done */
extern void __attribute__ ((visibility ("internal"))) setloop(struct itplayer *this, int s); /* - done */
extern int __attribute__ ((visibility ("internal"))) getloop(struct itplayer *this); /* - done */
extern int __attribute__ ((visibility ("internal"))) chanactive(struct itplayer *this, int ch, int *lc); /* - done */
extern int __attribute__ ((visibility ("internal"))) getchanins(struct itplayer *this, int ch); /* - done */
extern int __attribute__ ((visibility ("internal"))) getchansamp(struct itplayer *this, int ch); /* - done */
extern int __attribute__ ((visibility ("internal"))) lchanactive(struct itplayer *, int lc); /* - done */
extern void __attribute__ ((visibility ("internal"))) getchaninfo(struct itplayer *this, uint8_t ch, struct it_chaninfo *ci); /* - done */
extern int __attribute__ ((visibility ("internal"))) getchanalloc(struct itplayer *this, uint8_t ch); /* - done */

/*
#####static void playtickstatic(struct itplayer *); - done
#####static itplayerclass *staticthis; #            - done
static void playtick(struct itplayer *this); - done

static void readque(struct itplayer *this); - done
static void putque(struct itplayer *this, int type, int val1, int val2); - done

static void playnote(struct itplayer *, it_logchan *c, const uint8_t *cmd); - done
static void playvcmd(struct itplayer *, struct it_logchan *c, int vcmd); - done
static void playcmd(struct itplayer *this, struct it_logchan *c, int cmd, int data);
static void inittickchan(struct it_physchan *p); - done
static void inittick(struct it_logchan *c); - done
static void initrow(struct it_logchan *c); - done
static void updatechan(struct logchan *c); - done
static void processfx(struct itplayer *this, struct it_logchan *c); - done
static void processchan(struct itplayer *this, struct it_physchan *p); - done
static void allocatechan(struct itplayer *this, struct it_logchan *c); - done
static void putchandata(struct itplayer *this, struct it_physchan *p); - done
static void putglobdata(struct itplayer *this); - done
static void getproctime(struct itplayer *this); - done
static void checkchan(struct itplayer *this, struct it_physchan *p); - done
static int range64(int v); - done
static int range128(int v); - done
static int rangepitch(struct itplayer *this, int p); - done
static int rowslide(int data); - done
static int rowvolslide(int data); - done
static int tickslide(int data); - done
static int rowudslide(int data); - done
static void dovibrato(struct itplayer *this, struct it_logchan *c); - done
static void dotremolo(struct itplayer *this, struct it_logchan *c); - done
static void dopanbrello(struct itplayer *this, struct it_logchan *c); - done
static void doportanote(struct itplayer *this, struct it_logchan *c); - done
static void doretrigger(struct it_logchan *c); - done
static void dotremor(struct it_logchan *c); - done
static void dodelay(struct itplayer *this, struct it_logchan *c); - done
static int  ishex(char c); - done
static void parsemidicmd(struct it_logchan *c, char *cmd, int z); - done
static int random(struct itplayer *this); - done
static int processenvelope(const struct it_envelope *env, int *pos, int noteoff, int active); - done
*/

extern void __attribute__ ((visibility ("internal"))) itpInstSetup(const struct it_instrument *ins, int nins, const struct it_sample *smp, int nsmp, const struct it_sampleinfo *smpi,/* int unused,*/ int type, void (*MarkyBoy)(uint8_t *, uint8_t *)); /* private, done */

extern void __attribute__ ((visibility ("internal"))) itTrkSetup(const struct it_module *mod); /* private, done */

extern __attribute__ ((visibility ("internal"))) struct itplayer itplayer;
#endif
