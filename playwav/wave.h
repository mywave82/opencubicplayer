#ifndef __WAVE_H
#define __WAVE_H

struct waveinfo
{
	unsigned long pos;
	unsigned long len;
	uint32_t rate;
	int stereo;
	int bit16;
};

extern unsigned char __attribute__ ((visibility ("internal"))) wpOpenPlayer(FILE *fp, int tostereo, int tolerance);
extern void __attribute__ ((visibility ("internal"))) wpClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) wpIdle(void);
extern void __attribute__ ((visibility ("internal"))) wpSetLoop(unsigned char s);
extern char __attribute__ ((visibility ("internal"))) wpLooped(void);
extern void __attribute__ ((visibility ("internal"))) wpPause(unsigned char p);
extern void __attribute__ ((visibility ("internal"))) wpSetAmplify(uint32_t amp);
extern void __attribute__ ((visibility ("internal"))) wpSetSpeed(uint16_t sp);
extern void __attribute__ ((visibility ("internal"))) wpSetVolume(unsigned char vol, signed char bal, signed char pan, unsigned char opt);
extern void __attribute__ ((visibility ("internal"))) wpGetInfo(struct waveinfo *);
extern uint32_t __attribute__ ((visibility ("internal"))) wpGetPos(void);
extern void __attribute__ ((visibility ("internal"))) wpSetPos(uint32_t pos);

#endif
