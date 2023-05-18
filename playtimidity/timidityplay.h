#ifndef _TIMIDITYPLAY_H
#define _TIMIDITYPLAY_H

struct ocpfilehandle_t;
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

struct cpifaceSessionAPI_t;
OCP_INTERNAL int timidityOpenPlayer (const char *path, uint8_t *buffer, size_t bufferlen, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void timidityClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void timidityIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void timiditySetLoop (uint8_t s);
OCP_INTERNAL int timidityIsLooped (void);
OCP_INTERNAL void timiditySetRelPos (int pos);
OCP_INTERNAL void timidityRestart (void);
OCP_INTERNAL void timidityGetGlobInfo (struct mglobinfo *);

OCP_INTERNAL void timidityGetChanInfo (uint8_t ch, struct mchaninfo *ci);

OCP_INTERNAL void timidityChanSetup (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL int timidityGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *d, int max);
OCP_INTERNAL void timidityMute (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int m);


#endif
