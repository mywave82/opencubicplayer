#ifndef _PLAYXM_XMPLAY_H
#define _PLAYXM_XMPLAY_H

#include <stdio.h>

struct sampleinfo;

enum
{
	xmpEnvLoop=1, xmpEnvBiDi=2, xmpEnvSLoop=4
};

struct xmpenvelope
{
	uint8_t *env;
	uint16_t len;
	uint16_t loops, loope;
	uint16_t sustain;
	uint8_t type;
	uint8_t speed;
};

struct xmpsample
{
	char name[32];
	uint16_t handle;
	int16_t normnote;
	int16_t normtrans;
	int16_t stdvol;
	int16_t stdpan;
	uint16_t opt;
#define MP_OFFSETDIV2 1
	uint16_t volfade;
	uint8_t pchint;
	uint16_t volenv;
	uint16_t panenv;
	uint16_t pchenv;
	uint8_t vibspeed;
	uint8_t vibtype;
	uint16_t vibrate;
	uint16_t vibdepth;
	uint16_t vibsweep;
};

struct xmpinstrument
{
	char name[32];
	uint16_t samples[128];
};

struct xmodule
{
	char name[21];
	char ismod;
	char ft2_e60bug;
	int linearfreq;
	unsigned int nchan;
	unsigned int ninst;
	unsigned int nenv;
	unsigned int npat;
	unsigned int nord;
	unsigned int nsamp;
	unsigned int nsampi;
	int loopord;
	uint8_t initempo;
	uint8_t inibpm;

	struct xmpenvelope *envelopes;
	struct xmpsample *samples;
	struct xmpinstrument *instruments;
	struct sampleinfo *sampleinfos;
	uint16_t *patlens;
	uint8_t (**patterns)[5];
	uint16_t *orders;
	uint8_t panpos[256];
};

struct xmpglobinfo
{
	uint8_t globvol;
	uint8_t globvolslide;
};

struct xmpchaninfo
{
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

enum
{
	xmpFXIns=0,
	xmpFXNote=1,
	xmpFXVol=2,
	xmpFXCmd=3,
	xmpFXData=4
};

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;

OCP_INTERNAL int xmpLoadSamples (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m);
OCP_INTERNAL int xmpLoadModule (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadMOD  (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadMODt (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadMODd (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadMODf (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadM31  (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadM15  (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadM15t (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadWOW  (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL int xmpLoadMXM  (struct cpifaceSessionAPI_t *cpifaceSession, struct xmodule *m, struct ocpfilehandle_t *f);
OCP_INTERNAL void xmpFreeModule (struct xmodule *m);

OCP_INTERNAL int xmpPlayModule (struct xmodule *m, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void xmpStopModule (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void xmpSetPos (struct cpifaceSessionAPI_t *cpifaceSession, int ord, int row);

OCP_INTERNAL void xmpGetRealVolume (struct cpifaceSessionAPI_t *cpifaceSession, int i, int *l, int *r);
OCP_INTERNAL void xmpMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m);
OCP_INTERNAL uint16_t xmpGetPos (void);
OCP_INTERNAL int xmpGetRealPos (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL int xmpGetDotsData (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int *smp, int *frq, int *l, int *r, int *sus);
OCP_INTERNAL int xmpPrecalcTime (struct xmodule *m, int startpos, int (*calc)[2], int n, int ite);
OCP_INTERNAL int xmpLoop (void);
OCP_INTERNAL void xmpSetLoop (int);
OCP_INTERNAL int xmpGetChanIns (int);
OCP_INTERNAL int xmpGetChanSamp (int);
OCP_INTERNAL int xmpChanActive (struct cpifaceSessionAPI_t *cpifaceSession, int ch);
OCP_INTERNAL void xmpGetGlobInfo (int *tmp, int *bpm, int *gvol);
OCP_INTERNAL void xmpOptimizePatLens (struct xmodule *m);

OCP_INTERNAL void xmpGetChanInfo (uint8_t ch, struct xmpchaninfo *ci);
OCP_INTERNAL void xmpGetGlobInfo2 (struct xmpglobinfo *gi);

enum
{
	xmpCmdArpeggio=0,xmpCmdPortaU=1,xmpCmdPortaD=2,xmpCmdPortaNote=3,
	xmpCmdVibrato=4,xmpCmdPortaVol=5,xmpCmdVibVol=6,xmpCmdTremolo=7,
	xmpCmdPanning=8,xmpCmdOffset=9,xmpCmdVolSlide=10,xmpCmdJump=11,
	xmpCmdVolume=12,xmpCmdBreak=13,xmpCmdSpeed=15,xmpCmdGVolume=16,
	xmpCmdGVolSlide=17,xmpCmdKeyOff=20,xmpCmdEnvPos=21,xmpCmdPanSlide=25,
	xmpCmdMRetrigger=27,xmpCmdTremor=29,xmpCmdXPorta=33,xmpCmpSFilter=36,
	xmpCmdFPortaU=37,xmpCmdFPortaD=38,xmpCmdGlissando=39,xmpCmdVibType=40,
	xmpCmdSFinetune=41,xmpCmdPatLoop=42,xmpCmdTremType=43,xmpCmdSPanning=44,
	xmpCmdRetrigger=45,xmpCmdFVolSlideU=46, xmpCmdFVolSlideD=47,
	xmpCmdNoteCut=48,xmpCmdDelayNote=49,xmpCmdPatDelay=50, xmpCmdSync1=28,
	xmpCmdSync2=32, xmpCmdSync3=51,
	xmpVCmdVol0x=1,xmpVCmdVol1x=2,xmpVCmdVol2x=3,xmpVCmdVol3x=4,xmpVCmdVol40=5,
	xmpVCmdVolSlideD=6,xmpVCmdVolSlideU=7,xmpVCmdFVolSlideD=8,
	xmpVCmdFVolSlideU=9,xmpVCmdVibRate=10,xmpVCmdVibDep=11,xmpVCmdPanning=12,
	xmpVCmdPanSlideL=13,xmpVCmdPanSlideR=14,xmpVCmdPortaNote=15,
	xmpCmdMODtTempo=128
};

enum
{
	xfxGVSUp=1, xfxGVSDown,
	xfxVSUp=1, xfxVSDown,
	xfxPSUp=1, xfxPSDown, xfxPSToNote,
	xfxPnSRight=1, xfxPnSLeft,
	xfxVXVibrato=1, xfxVXTremor,
	xfxPXVibrato=1, xfxPXArpeggio,
	xfxPnXVibrato=1,
	xfxNXNoteCut=1, xfxNXRetrig, xfxNXDelay,

	xfxVolSlideUp=1, xfxVolSlideDown,
	xfxRowVolSlideUp, xfxRowVolSlideDown,
	xfxPitchSlideUp, xfxPitchSlideDown, xfxPitchSlideToNote,
	xfxRowPitchSlideUp, xfxRowPitchSlideDown,
	xfxPanSlideRight, xfxPanSlideLeft,
	xfxVolVibrato, xfxTremor,
	xfxPitchVibrato, xfxArpeggio,
	xfxNoteCut, xfxRetrig,
	xfxOffset, xfxEnvPos,
	xfxDelay, xfxSetFinetune
};

struct cpifaceSessionAPI_t;

OCP_INTERNAL void xmpInstSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct xmpinstrument *ins, int nins, const struct xmpsample *smp, int nsmp, const struct sampleinfo *smpi, int nsmpi, int type, void (*MarkyBoy)(struct cpifaceSessionAPI_t *cpifaceSession, char *, char *));
OCP_INTERNAL void xmTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct xmodule *mod);
OCP_INTERNAL void xmpInstClear (struct cpifaceSessionAPI_t *cpifaceSession);

extern OCP_INTERNAL struct xmodule mod;

#endif
