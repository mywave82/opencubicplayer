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
extern unsigned char __attribute__ ((visibility ("internal"))) wpOpenPlayer (struct ocpfilehandle_t *fp, struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) wpClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) wpIdle (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) wpSetLoop (unsigned char s);
extern char __attribute__ ((visibility ("internal"))) wpLooped (void);
extern void __attribute__ ((visibility ("internal"))) wpPause (unsigned char p);
extern void __attribute__ ((visibility ("internal"))) wpGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct waveinfo *);
extern uint32_t __attribute__ ((visibility ("internal"))) wpGetPos (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) wpSetPos (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t pos);

#endif
