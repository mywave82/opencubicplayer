#ifndef YMPLAY_H
#define YMPLAY_H 1

extern void __attribute__ ((visibility ("internal"))) ymClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) ymMute(int i, int m);
extern int __attribute__ ((visibility ("internal"))) ymOpenPlayer(FILE *file);
extern void __attribute__ ((visibility ("internal"))) ymSetLoop(int loop);
extern int __attribute__ ((visibility ("internal"))) ymIsLooped(void);

extern void __attribute__ ((visibility ("internal"))) ymPause(uint8_t p);
extern void __attribute__ ((visibility ("internal"))) ymSetAmplify(uint32_t amp);
extern void __attribute__ ((visibility ("internal"))) ymSetSpeed(uint16_t sp);

extern void __attribute__ ((visibility ("internal"))) ymIdle(void);
extern __attribute__ ((visibility ("internal"))) uint16_t vol;
extern __attribute__ ((visibility ("internal"))) int16_t bal;
extern __attribute__ ((visibility ("internal"))) int pan;
extern __attribute__ ((visibility ("internal"))) int srnd;
extern __attribute__ ((visibility ("internal"))) uint32_t ymbufrate;

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
