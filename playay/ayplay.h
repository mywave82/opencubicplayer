#ifndef _AYPLAY_H
#define _AYPLAY_H

struct ayinfo
{
	unsigned char filever, playerver;
	int track;
	int numtracks;
	const char *trackname;
};

struct ay_driver_frame_state_t; /* sound.h */
struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;
extern int __attribute__ ((visibility ("internal"))) ayOpenPlayer(struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) ayClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) ayIdle (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) aySetLoop(uint8_t s);
extern int __attribute__ ((visibility ("internal"))) ayIsLooped(void);
extern void __attribute__ ((visibility ("internal"))) ayPause(uint8_t p);
extern void __attribute__ ((visibility ("internal"))) aySetMute (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int mute);
extern void __attribute__ ((visibility ("internal"))) ayGetInfo(struct ayinfo *);
extern void __attribute__ ((visibility ("internal"))) ayGetChans(struct ay_driver_frame_state_t *);
extern void __attribute__ ((visibility ("internal"))) ayStartSong (struct cpifaceSessionAPI_t *cpifaceSession, int song);
extern void __attribute__ ((visibility ("internal"))) ayChanSetup (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
