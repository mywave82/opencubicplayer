#ifndef __HLVPLAY_H
#define __HLVPLAY_H 1

struct hvl_tune;

#define current_hvl_tune ht
       struct hvl_tune __attribute__ ((visibility ("internal"))) *current_hvl_tune;
extern struct hvl_tune __attribute__ ((visibility ("internal"))) *hvlOpenPlayer (const uint8_t *mem, size_t memlen);
extern void            __attribute__ ((visibility ("internal")))  hvlClosePlayer (void);
extern void            __attribute__ ((visibility ("internal")))  hvlIdle (void);
extern void            __attribute__ ((visibility ("internal")))  hvlSetLoop (uint8_t s);
extern char            __attribute__ ((visibility ("internal")))  hvlLooped (void);
extern void            __attribute__ ((visibility ("internal")))  hvlPause (uint8_t p);
extern void            __attribute__ ((visibility ("internal")))  hvlSetAmplify (uint32_t amp);
extern void            __attribute__ ((visibility ("internal")))  hvlSetSpeed (uint16_t sp);
extern void            __attribute__ ((visibility ("internal")))  hvlSetPitch (uint16_t sp);
extern void            __attribute__ ((visibility ("internal")))  hvlSetPausePitch (uint32_t sp);
extern void            __attribute__ ((visibility ("internal")))  hvlSetVolume (uint8_t vol, int8_t bal, int8_t pan, uint8_t opt);
extern void            __attribute__ ((visibility ("internal")))  hvlPrevSubSong ();
extern void            __attribute__ ((visibility ("internal")))  hvlNextSubSong ();
extern void            __attribute__ ((visibility ("internal")))  hvlGetStats (int *row, int *rows, int *order, int *orders, int *subsong, int *subsongs, int *tempo, int *speedmult);


#endif
