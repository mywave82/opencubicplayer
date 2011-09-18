#ifndef _CDAUDIO_H
#define _CDAUDIO_H

struct cdStat
{
	int paused; /* boolean */
	int error;  /* boolean */
	int looped; /* boolean */
	uint32_t position;
	int speed;
};

extern int __attribute__ ((visibility ("internal"))) cdIsCDDrive(int fd);
extern void __attribute__ ((visibility ("internal"))) cdStop(int fd);
extern void __attribute__ ((visibility ("internal"))) cdRestart(int fd);
extern void __attribute__ ((visibility ("internal"))) cdRestartAt(int fd, unsigned long start);
extern void __attribute__ ((visibility ("internal"))) cdPause(int fd);
extern unsigned short __attribute__ ((visibility ("internal"))) cdGetTracks(int fd, unsigned long *starts, unsigned char *first, unsigned short maxtracks);
extern int __attribute__ ((visibility ("internal"))) cdPlay(int fd, unsigned long start, unsigned long len);
extern void __attribute__ ((visibility ("internal"))) cdGetStatus(int fd, struct cdStat *stat);
extern void __attribute__ ((visibility ("internal"))) cdSetSpeed(unsigned short sp);
extern void __attribute__ ((visibility ("internal"))) cdSetLoop(int loop);
extern void __attribute__ ((visibility ("internal"))) cdIdle(void);

#endif
