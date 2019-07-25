#ifndef _TIMIDITYPLAY_H
#define _TIMIDITYPLAY_H

struct notedotsdata;

struct mglobinfo
{
	uint32_t curtick;
	uint32_t ticknum;
};

struct mchaninfo
{
	char instrument[32];
	uint8_t program;
	uint8_t bank_msb;
	uint8_t bank_lsb; 

	uint8_t pan;
	uint8_t gvol;
	int16_t pitch;
	uint8_t reverb;
	uint8_t chorus;
	uint8_t notenum;
	uint8_t pedal;
	uint8_t note[32];
	uint8_t vol[32];
	uint8_t opt[32];
};

extern int __attribute__ ((visibility ("internal"))) timidityOpenPlayer(const char *path, uint8_t *buffer, size_t bufferlen);
extern void __attribute__ ((visibility ("internal"))) timidityClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) timidityIdle(void);
extern void __attribute__ ((visibility ("internal"))) timiditySetLoop(uint8_t s);
extern int __attribute__ ((visibility ("internal"))) timidityIsLooped(void);
extern void __attribute__ ((visibility ("internal"))) timidityPause(uint8_t p);
/* extern void __attribute__ ((visibility ("internal"))) timiditySetAmplify(uint32_t amp); */
extern void __attribute__ ((visibility ("internal"))) timiditySetRelPos(int pos);

extern void __attribute__ ((visibility ("internal"))) timiditySetSpeed(uint16_t sp);
extern void __attribute__ ((visibility ("internal"))) timiditySetPitch(int16_t sp);
extern void __attribute__ ((visibility ("internal"))) timiditySetVolume(uint8_t vol, int8_t bal, int8_t pan, uint8_t opt);
extern void __attribute__ ((visibility ("internal"))) timidityGetGlobInfo(struct mglobinfo *);

extern void __attribute__ ((visibility ("internal"))) timidityGetChanInfo(uint8_t ch, struct mchaninfo *ci);

extern void __attribute__ ((visibility ("internal"))) timidityChanSetup(/*const struct midifile *mid*/);
extern int __attribute__ ((visibility ("internal"))) timidityGetDots(struct notedotsdata *d, int max);

#endif
