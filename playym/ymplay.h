#ifndef YMPLAY_H
#define YMPLAY_H 1

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;
extern void __attribute__ ((visibility ("internal"))) ymClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) ymMute (struct cpifaceSessionAPI_t *cpifaceSession, int i, int m);
extern int __attribute__ ((visibility ("internal"))) ymOpenPlayer(struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) ymSetLoop(int loop);
extern int __attribute__ ((visibility ("internal"))) ymIsLooped(void);

extern void __attribute__ ((visibility ("internal"))) ymPause(uint8_t p);

extern void __attribute__ ((visibility ("internal"))) ymIdle(void);

class CYmMusic;
extern __attribute__ ((visibility ("internal"))) CYmMusic *pMusic;

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
extern __attribute__ ((visibility ("internal"))) struct channel_info_t *ymRegisters();
extern uint32_t __attribute__ ((visibility ("internal"))) ymGetPos(void);
extern void __attribute__ ((visibility ("internal"))) ymSetPos(uint32_t pos);

#endif
