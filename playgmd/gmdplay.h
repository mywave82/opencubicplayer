#ifndef __MP_H
#define __MP_H

#include "gmdinst.h"

#define GMD_MAXLCHAN 32

struct ocpfilehandle_t;

struct sampleinfo;

struct ocpfilehandle_t;

struct gmdtrack
{
	unsigned char *ptr;
	unsigned char *end;
};

struct gmdpattern
{
	char name[32];
	uint16_t patlen;
	uint16_t gtrack;
	uint16_t tracks[GMD_MAXLCHAN];
};


#define MOD_TICK0 1
#define MOD_EXPOFREQ 2
#define MOD_S3M 4
#define MOD_GUSVOL 8
#define MOD_EXPOPITCHENV 16
#define MOD_S3M30 32
#define MOD_MODPAN 0x10000

struct gmdmodule
{
	char name[32];
	char composer[32];
	uint32_t options;
	unsigned int channum;
	unsigned int instnum;
	unsigned int patnum;
	unsigned int ordnum;
	unsigned int endord;
	unsigned int loopord;
	unsigned int tracknum;
	unsigned int sampnum;
	unsigned int modsampnum;
	unsigned int envnum;
	struct gmdinstrument *instruments;
	struct gmdtrack *tracks;
	struct gmdenvelope *envelopes;
	struct sampleinfo *samples;
	struct gmdsample *modsamples;
	struct gmdpattern *patterns;
	char **message;
	uint16_t *orders;
};

struct globinfo
{
	uint8_t speed;
	uint8_t curtick;
	uint8_t tempo;
	uint8_t currow;
	uint16_t patlen;
	uint16_t curpat;
	uint16_t patnum;
	uint8_t globvol;
	uint8_t globvolslide;
};

struct chaninfo
{
	uint8_t ins;
	uint16_t smp;
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

OCP_INTERNAL void mpReset (struct gmdmodule *m);
OCP_INTERNAL void mpFree (struct gmdmodule *m);
OCP_INTERNAL int mpAllocInstruments (struct gmdmodule *m, int n);
OCP_INTERNAL int mpAllocSamples (struct gmdmodule *m, int n);
OCP_INTERNAL int mpAllocModSamples (struct gmdmodule *m, int n);
OCP_INTERNAL int mpAllocTracks (struct gmdmodule *m, int n);
OCP_INTERNAL int mpAllocPatterns (struct gmdmodule *m, int n);
OCP_INTERNAL int mpAllocEnvelopes (struct gmdmodule *m, int n);
OCP_INTERNAL int mpAllocOrders (struct gmdmodule *m, int n);
OCP_INTERNAL void mpOptimizePatLens (struct gmdmodule *m);
OCP_INTERNAL void mpReduceInstruments (struct gmdmodule *m);
OCP_INTERNAL void mpReduceMessage (struct gmdmodule *m);
OCP_INTERNAL int mpReduceSamples (struct gmdmodule *m);
OCP_INTERNAL int mpLoadSamples (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m);

struct cpifaceSessionAPI_t;
OCP_INTERNAL int mpPlayModule (const struct gmdmodule *, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void mpStopModule (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void mpSetPosition (struct cpifaceSessionAPI_t *cpifaceSession, int16_t pat, int16_t row);
OCP_INTERNAL void mpGetPosition (uint16_t *pat, uint8_t *row);
OCP_INTERNAL int mpGetRealPos (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void mpGetChanInfo (uint8_t ch, struct chaninfo *ci);
OCP_INTERNAL uint16_t mpGetRealNote (struct cpifaceSessionAPI_t *cpifaceSession, uint8_t ch);
OCP_INTERNAL void mpGetGlobInfo (struct globinfo *gi);
OCP_INTERNAL char mpLooped (void);
OCP_INTERNAL void mpSetLoop (unsigned char s);
OCP_INTERNAL void mpLockPat (int st);
OCP_INTERNAL int mpGetChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *buf, unsigned int len, uint32_t rate, int opt);
OCP_INTERNAL void mpMute (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int m);
OCP_INTERNAL void mpGetRealVolume (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int *l, int *r);
OCP_INTERNAL int mpGetChanStatus (struct cpifaceSessionAPI_t *cpifaceSession, int ch);

enum
{
	cmdTempo, cmdSpeed, cmdBreak, cmdGoto, cmdPatLoop, cmdPatDelay, cmdGlobVol, cmdGlobVolSlide, cmdSetChan, cmdFineSpeed
};

enum
{
	cmdVolSlideUp, cmdVolSlideDown, cmdRowVolSlideUp, cmdRowVolSlideDown,
	cmdPitchSlideUp, cmdPitchSlideDown, cmdPitchSlideToNote,
	cmdRowPitchSlideUp, cmdRowPitchSlideDown,
	cmdPanSlide, cmdRowPanSlide,
	cmdDelay_NotImplemented_Use_cmdPlayDelay,
	cmdVolVibrato, cmdVolVibratoSetWave, cmdTremor,
	cmdPitchVibrato, cmdPitchVibratoSetSpeed, cmdPitchVibratoFine,
	cmdPitchVibratoSetWave, cmdArpeggio,
	cmdNoteCut, cmdRetrig,
	cmdOffset,
	cmdPanSurround,
	cmdKeyOff,
	cmdSetEnvPos,

	cmdVolSlideUDMF, cmdVolSlideDDMF,
	cmdPanSlideLDMF, cmdPanSlideRDMF,
	cmdPitchSlideUDMF, cmdPitchSlideDDMF, cmdPitchSlideNDMF, cmdRowPitchSlideDMF,
	cmdVolVibratoSinDMF, cmdVolVibratoTrgDMF, cmdVolVibratoRecDMF,
	cmdPanVibratoSinDMF,
	cmdPitchVibratoSinDMF, cmdPitchVibratoTrgDMF, cmdPitchVibratoRecDMF,

	cmdPanDepth,
	cmdPanHeight,

	cmdChannelVol,

	cmdSpecial,
	cmdOffsetHigh,

	cmdOffsetEnd,
	cmdSetDir,
	cmdSetLoop,

	cmdPlayNote=0x80, cmdPlayIns=0x01, cmdPlayNte=0x02, cmdPlayVol=0x04, cmdPlayPan=0x08, cmdPlayDelay=0x10
};

enum
{
	cmdContVolSlide,
	cmdContRowVolSlide,
	cmdContMixVolSlide,
	cmdContMixVolSlideUp,
	cmdContMixVolSlideDown,
	cmdContMixPitchSlideUp,
	cmdContMixPitchSlideDown,
	cmdGlissOn,
	cmdGlissOff
};

enum
{
	fxGVSUp=1, fxGVSDown,
	fxVSUp=1, fxVSDown, fxVSUDMF, fxVSDDMF,
	fxPSUp=1, fxPSDown, fxPSToNote, fxPSUDMF, fxPSDDMF, fxPSNDMF,
	fxPnSRight=1, fxPnSLeft, fxPnSLDMF, fxPnSRDMF,
	fxVXVibrato=1, fxVXTremor,
	fxPXVibrato=1, fxPXArpeggio,
	fxPnXVibrato=1,
	fxNXNoteCut=1, fxNXRetrig,

	fxVolSlideUp=1, fxVolSlideDown,
	fxRowVolSlideUp, fxRowVolSlideDown,
	fxPitchSlideUp, fxPitchSlideDown, fxPitchSlideToNote,
	fxRowPitchSlideUp, fxRowPitchSlideDown,
	fxPanSlideRight, fxPanSlideLeft,
	fxVolVibrato, fxTremor,
	fxPitchVibrato, fxArpeggio,
	fxNoteCut, fxRetrig,
	fxOffset,
	fxDelay,
	fxPanVibrato
};

OCP_INTERNAL int Load669 (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadAMS (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadDMF (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadMDL (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadMTM (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadOKT (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadPTM (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadS3M (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadSTM (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);
OCP_INTERNAL int LoadULT (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file);

extern OCP_INTERNAL struct gmdmodule mod;

#endif
