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

extern void __attribute__ ((visibility ("internal"))) cdClose (void);
extern void __attribute__ ((visibility ("internal"))) cdUnpause (void);
extern void __attribute__ ((visibility ("internal"))) cdJump (unsigned long start);
extern void __attribute__ ((visibility ("internal"))) cdPause (void);
#if 0
extern unsigned short __attribute__ ((visibility ("internal"))) cdGetTracks (unsigned long *starts, unsigned char *first, unsigned short maxtracks);
#endif
extern int __attribute__ ((visibility ("internal"))) cdOpen (unsigned long start, unsigned long len, struct ocpfilehandle_t *file);
extern void __attribute__ ((visibility ("internal"))) cdGetStatus (struct cdStat *stat);
//extern void __attribute__ ((visibility ("internal"))) cdSetAmplify(uint32_t amp);
extern void __attribute__ ((visibility ("internal"))) cdSetSpeed (unsigned short sp);
extern void __attribute__ ((visibility ("internal"))) cdSetVolume(uint8_t vol, int8_t bal, int8_t pan, uint8_t opt);
extern void __attribute__ ((visibility ("internal"))) cdSetLoop (int loop);
extern void __attribute__ ((visibility ("internal"))) cdIdle (void);

#endif
