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
	const char *opt25;
	const char *opt50;
};

struct ID3_t;
struct ocpfilehandle_t;

struct cpifaceSessionAPI_t;
extern int __attribute__ ((visibility ("internal"))) mpeg_Bitrate; /* bitrate of the last decoded frame */
extern int __attribute__ ((visibility ("internal"))) mpegOpenPlayer(struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) mpegClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) mpegIdle (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) mpegSetLoop(uint8_t s);
extern char __attribute__ ((visibility ("internal"))) mpegIsLooped(void);
extern void __attribute__ ((visibility ("internal"))) mpegPause(uint8_t p);
extern void __attribute__ ((visibility ("internal"))) mpegGetInfo(struct mpeginfo *);
extern uint32_t __attribute__ ((visibility ("internal"))) mpegGetPos(void);
extern void __attribute__ ((visibility ("internal"))) mpegSetPos(uint32_t pos);
extern void __attribute__ ((visibility ("internal"))) mpegGetID3(struct ID3_t **ID3);
extern void __attribute__ ((visibility ("internal"))) ID3InfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) ID3InfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
