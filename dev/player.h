#ifndef __PLAYER_H
#define __PLAYER_H

#define PLR_STEREO 1
#define PLR_16BIT 2
#define PLR_SIGNEDOUT 4
#define PLR_REVERSESTEREO 8
#define PLR_RESTRICTED 16

enum
{
	plrGetSampleStereo=1
};

extern unsigned int plrRate;
extern int plrOpt;
extern int (*plrPlay)(void **buf, unsigned int *len);
extern void (*plrStop)(void);
extern void (*plrSetOptions)(uint32_t rate, int opt);
extern int (*plrGetBufPos)(void);
extern int (*plrGetPlayPos)(void);
extern void (*plrAdvanceTo)(unsigned int pos);
extern uint32_t (*plrGetTimer)(void);
extern void (*plrIdle)(void);
#ifdef PLR_DEBUG
extern char *(*plrDebug)(void);
#endif

extern int plrOpenPlayer(void **buf, uint32_t *len, uint32_t blen);
extern void plrClosePlayer(void);
extern void plrGetRealMasterVolume(int *l, int *r);
extern void plrGetMasterSample(int16_t *s, uint32_t len, uint32_t rate, int opt);

#endif
