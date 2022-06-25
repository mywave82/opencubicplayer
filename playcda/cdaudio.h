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
extern void __attribute__ ((visibility ("internal"))) cdClose (void);
extern void __attribute__ ((visibility ("internal"))) cdUnpause (void);
extern void __attribute__ ((visibility ("internal"))) cdJump (unsigned long start);
extern void __attribute__ ((visibility ("internal"))) cdPause (void);
extern int __attribute__ ((visibility ("internal"))) cdOpen (unsigned long start, unsigned long len, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpiSessionAPI);
extern void __attribute__ ((visibility ("internal"))) cdGetStatus (struct cdStat *stat);
extern void __attribute__ ((visibility ("internal"))) cdSetLoop (int loop);
extern void __attribute__ ((visibility ("internal"))) cdIdle (void);

#endif
