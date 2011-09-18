#ifndef _FLACPLAY_H
#define _FLACPLAY_H

struct flacinfo
{
	uint64_t pos; /* in source samples */
	uint64_t len; /* in source samples */
	uint32_t timelen; /* in seconds ETA */
	uint32_t rate; /* output rate */
	int	 stereo;
	int      bits;
};

extern int __attribute__ ((visibility ("internal"))) flacOpenPlayer(FILE *);
extern void __attribute__ ((visibility ("internal"))) flacClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) flacIdle(void);
extern void __attribute__ ((visibility ("internal"))) flacSetLoop(uint8_t s);
extern int __attribute__ ((visibility ("internal"))) flacIsLooped(void);
extern void __attribute__ ((visibility ("internal"))) flacPause(int p);
extern void __attribute__ ((visibility ("internal"))) flacSetAmplify(uint32_t amp);
extern void __attribute__ ((visibility ("internal"))) flacSetSpeed(uint16_t sp);
extern void __attribute__ ((visibility ("internal"))) flacSetVolume(uint8_t vol, int8_t bal, int8_t pan, uint8_t opt);
extern void __attribute__ ((visibility ("internal"))) flacGetInfo(struct flacinfo *);
extern uint64_t __attribute__ ((visibility ("internal"))) flacGetPos(void);
extern void __attribute__ ((visibility ("internal"))) flacSetPos(uint64_t pos);

#endif
