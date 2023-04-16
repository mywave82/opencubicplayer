#ifndef YMPLAY_H
#define YMPLAY_H 1

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;
class CYmMusic;

OCP_INTERNAL void ymClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void ymMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m);
OCP_INTERNAL int ymOpenPlayer (struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void ymSetLoop (int loop);
OCP_INTERNAL int ymIsLooped (void);

OCP_INTERNAL void ymPause (uint8_t p);

OCP_INTERNAL void ymIdle (struct cpifaceSessionAPI_t *cpifaceSession);

extern OCP_INTERNAL CYmMusic *pMusic;

struct channel_info_t
{
	int frequency_a;
	int frequency_b;
	int frequency_c;
	int frequency_noise;
	int frequency_envelope;
	int envelope_shape;
	int mixer_control;
	int level_a;
	int level_b;
	int level_c;
};

OCP_INTERNAL struct channel_info_t *ymRegisters (void);
OCP_INTERNAL uint32_t ymGetPos (void);
OCP_INTERNAL void ymSetPos (uint32_t pos);

#endif
