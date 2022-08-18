#ifndef _CDAUDIO_H
#define _CDAUDIO_H

struct ocpfilehandle_t;

struct cdStat
{
	int paused; /* boolean */
	int error;  /* boolean */
	int looped; /* boolean */
	uint32_t position;
	int speed;
};

struct cpifaceSessionAPI_t;
extern void __attribute__ ((visibility ("internal"))) cdClose (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) cdUnpause (void);
extern void __attribute__ ((visibility ("internal"))) cdJump (struct cpifaceSessionAPI_t *cpifaceSession, unsigned long start);
extern void __attribute__ ((visibility ("internal"))) cdPause (void);
extern int __attribute__ ((visibility ("internal"))) cdOpen (unsigned long start, unsigned long len, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) cdGetStatus (struct cdStat *stat);
extern void __attribute__ ((visibility ("internal"))) cdSetLoop (int loop);
extern void __attribute__ ((visibility ("internal"))) cdIdle (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
