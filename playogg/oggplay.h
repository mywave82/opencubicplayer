#ifndef __OGG_H
#define __OGG_H

struct ogginfo
{
	uint32_t pos;
	uint32_t len;
	uint32_t rate;
	uint8_t stereo;
	uint8_t bit16;
	int bitrate;
};

extern int __attribute__ ((visibility ("internal"))) oggOpenPlayer(FILE *);
extern void __attribute__ ((visibility ("internal"))) oggClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) oggIdle(void);
extern void __attribute__ ((visibility ("internal"))) oggSetLoop(uint8_t s);
extern char __attribute__ ((visibility ("internal"))) oggLooped(void);
extern void __attribute__ ((visibility ("internal"))) oggPause(uint8_t p);
extern void __attribute__ ((visibility ("internal"))) oggSetAmplify(uint32_t amp);
extern void __attribute__ ((visibility ("internal"))) oggSetSpeed(uint16_t sp);
extern void __attribute__ ((visibility ("internal"))) oggSetVolume(uint8_t vol, int8_t bal, int8_t pan, uint8_t opt);
extern void __attribute__ ((visibility ("internal"))) oggGetInfo(struct ogginfo *);
extern uint32_t __attribute__ ((visibility ("internal"))) oggGetPos(void);
extern void __attribute__ ((visibility ("internal"))) oggSetPos(uint32_t pos);

#endif
