#ifndef _SNDHPLAY_H
#define _SNDHPLAY_H

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;
OCP_INTERNAL int sndhOpenPlayer (struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void sndhClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void sndhIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void sndhStartTune (struct cpifaceSessionAPI_t *cpifaceSession, int tune);

OCP_INTERNAL int sndhIsLooped (void);
OCP_INTERNAL void sndhSetLoop (unsigned char s);

struct sndhStat_t
{
	int SubTune_active;  /* 1-based */
	int SubTune_onqueue;  /* 1-based */
	int SubTunes; /* 1-based */
};
OCP_INTERNAL void sndhStat(struct sndhStat_t *stat);

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
	int dma_active_and_repeat_and_mono;
	int dma_samplerate;
	uint8_t power_a, power_b, power_c, power_l, power_r;
};

OCP_INTERNAL struct channel_info_t *sndhRegisters(void);

OCP_INTERNAL int sndhGetPChanSample (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);

OCP_INTERNAL void sndhTrackInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void sndhTrackDone (struct cpifaceSessionAPI_t *cpifaceSession);

struct sndhMeta_t;
OCP_INTERNAL struct sndhMeta_t *sndhGetMeta (void);

#endif
