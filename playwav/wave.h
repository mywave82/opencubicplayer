#ifndef __WAVE_H
#define __WAVE_H

struct waveinfo
{
	unsigned long pos;
	unsigned long len;
	uint32_t rate;
	int stereo;
	int bit16;
	const char *opt25;
	const char *opt50;
};

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;

OCP_INTERNAL unsigned char wpOpenPlayer (struct ocpfilehandle_t *fp, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wpClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wpIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wpSetLoop (unsigned char s);
OCP_INTERNAL char wpLooped (void);
OCP_INTERNAL void wpPause (unsigned char p);
OCP_INTERNAL void wpGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct waveinfo *);
OCP_INTERNAL uint32_t wpGetPos (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wpSetPos (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t pos);

#endif
