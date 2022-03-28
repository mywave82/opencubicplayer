#ifndef __PLAYER_H
#define __PLAYER_H

/* in the future we might add optional 5.1, 7.1, float etc - All devp drivers MUST atleast support PLR_STEREO_16BIT_SIGNED */
#define PLR_STEREO_16BIT_SIGNED 1

struct ocpfilehandle_t;

extern unsigned int plrRate;
extern int plrOpt;
extern int (*plrPlay)(void **buf, unsigned int *len, struct ocpfilehandle_t *source_file);
extern void (*plrStop)(void);
extern void (*plrSetOptions)(uint32_t rate, int opt);
extern int (*plrGetBufPos)(void);
extern int (*plrGetPlayPos)(void);
extern void (*plrAdvanceTo)(unsigned int pos);
extern uint32_t (*plrGetTimer)(void);
extern void (*plrIdle)(void);

extern int plrOpenPlayer(void **buf, uint32_t *len, uint32_t blen, struct ocpfilehandle_t *source_file);
extern void plrClosePlayer(void);
extern void plrGetRealMasterVolume(int *l, int *r);
extern void plrGetMasterSample(int16_t *s, uint32_t len, uint32_t rate, int opt);

#endif
