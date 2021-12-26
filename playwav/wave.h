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
extern unsigned char __attribute__ ((visibility ("internal"))) wpOpenPlayer(struct ocpfilehandle_t *fp);
extern void __attribute__ ((visibility ("internal"))) wpClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) wpIdle(void);
extern void __attribute__ ((visibility ("internal"))) wpSetLoop(unsigned char s);
extern char __attribute__ ((visibility ("internal"))) wpLooped(void);
extern void __attribute__ ((visibility ("internal"))) wpPause(unsigned char p);
extern void __attribute__ ((visibility ("internal"))) wpGetInfo(struct waveinfo *);
extern uint32_t __attribute__ ((visibility ("internal"))) wpGetPos(void);
extern void __attribute__ ((visibility ("internal"))) wpSetPos(uint32_t pos);

#endif
