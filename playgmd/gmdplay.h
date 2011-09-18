#ifndef __MP_H
#define __MP_H

#include "gmdinst.h"

#define MP_MAXCHANNELS 32

struct sampleinfo;

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
	uint16_t tracks[MP_MAXCHANNELS];
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

extern void mpReset(struct gmdmodule *m);
extern void mpFree(struct gmdmodule *m);
extern int mpAllocInstruments(struct gmdmodule *m, int n);
extern int mpAllocSamples(struct gmdmodule *m, int n);
extern int mpAllocModSamples(struct gmdmodule *m, int n);
extern int mpAllocTracks(struct gmdmodule *m, int n);
extern int mpAllocPatterns(struct gmdmodule *m, int n);
extern int mpAllocEnvelopes(struct gmdmodule *m, int n);
extern int mpAllocOrders(struct gmdmodule *m, int n);
extern void __attribute__ ((visibility ("internal"))) mpOptimizePatLens(struct gmdmodule *m);
extern void __attribute__ ((visibility ("internal"))) mpReduceInstruments(struct gmdmodule *m);
extern void __attribute__ ((visibility ("internal"))) mpReduceMessage(struct gmdmodule *m);
extern int __attribute__ ((visibility ("internal"))) mpReduceSamples(struct gmdmodule *m);
extern int __attribute__ ((visibility ("internal"))) mpLoadSamples(struct gmdmodule *m);
extern void __attribute__ ((visibility ("internal"))) mpRemoveText(struct gmdmodule *m);

extern char __attribute__ ((visibility ("internal"))) mpPlayModule(const struct gmdmodule *);
extern void __attribute__ ((visibility ("internal"))) mpStopModule(void);
extern void __attribute__ ((visibility ("internal"))) mpSetPosition(int16_t pat, int16_t row);
extern void __attribute__ ((visibility ("internal"))) mpGetPosition(uint16_t *pat, uint8_t *row);
extern int __attribute__ ((visibility ("internal"))) mpGetRealPos(void);
extern void __attribute__ ((visibility ("internal"))) mpGetChanInfo(uint8_t ch, struct chaninfo *ci);
extern uint16_t __attribute__ ((visibility ("internal"))) mpGetRealNote(uint8_t ch);
extern void __attribute__ ((visibility ("internal"))) mpGetGlobInfo(struct globinfo *gi);
extern char __attribute__ ((visibility ("internal"))) mpLooped(void );
extern void __attribute__ ((visibility ("internal"))) mpSetLoop(unsigned char s);
extern void __attribute__ ((visibility ("internal"))) mpLockPat(int st);
extern int __attribute__ ((visibility ("internal"))) mpGetChanSample(unsigned int ch, int16_t *buf, unsigned int len, uint32_t rate, int opt);
extern void __attribute__ ((visibility ("internal"))) mpMute(int ch, int m);
extern void __attribute__ ((visibility ("internal"))) mpGetRealVolume(int ch, int *l, int *r);
extern int __attribute__ ((visibility ("internal"))) mpGetChanStatus(int ch);
extern int __attribute__ ((visibility ("internal"))) mpGetMute(int ch);

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
	cmdDelay,
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

struct gmdloadstruct {
	int (*load)(struct gmdmodule *m, FILE *file);
};

#endif
