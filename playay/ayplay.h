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
OCP_INTERNAL int ayOpenPlayer (struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void ayClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void ayIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void aySetLoop (uint8_t s);
OCP_INTERNAL int ayIsLooped (void);
OCP_INTERNAL void ayPause (uint8_t p);
OCP_INTERNAL void aySetMute (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int mute);
OCP_INTERNAL void ayGetInfo (struct ayinfo *);
OCP_INTERNAL void ayGetChans (struct ay_driver_frame_state_t *);
OCP_INTERNAL void ayStartSong (struct cpifaceSessionAPI_t *cpifaceSession, int song);
OCP_INTERNAL void ayChanSetup (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
