#ifndef _MPPLAY_H
#define _MPPLAY_H

struct mpeginfo
{
	uint32_t pos;
	uint32_t len;
	uint32_t timelen;
	uint32_t rate;
	int	 stereo;
	int      bit16;
};

extern int __attribute__ ((visibility ("internal"))) mpeg_Bitrate; /* bitrate of the last decoded frame */
extern unsigned char __attribute__ ((visibility ("internal"))) mpegOpenPlayer(FILE *, size_t offset, size_t fsize);
extern void __attribute__ ((visibility ("internal"))) mpegClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) mpegIdle(void);
extern void __attribute__ ((visibility ("internal"))) mpegSetLoop(uint8_t s);
extern char __attribute__ ((visibility ("internal"))) mpegIsLooped(void);
extern void __attribute__ ((visibility ("internal"))) mpegPause(uint8_t p);
extern void __attribute__ ((visibility ("internal"))) mpegSetAmplify(uint32_t amp);
extern void __attribute__ ((visibility ("internal"))) mpegSetSpeed(uint16_t sp);
extern void __attribute__ ((visibility ("internal"))) mpegSetVolume(uint8_t vol, int8_t bal, int8_t pan, uint8_t opt);
extern void __attribute__ ((visibility ("internal"))) mpegGetInfo(struct mpeginfo *);
extern int32_t __attribute__ ((visibility ("internal"))) mpegGetPos(void);
extern void __attribute__ ((visibility ("internal"))) mpegSetPos(uint32_t pos);

#endif
