#ifndef __MIX_H
#define __MIX_H

struct mixchannel
{
	void *samp;
	union
	{
		int8_t *fmt8;
		int16_t *fmt16;
		float *fmtfloat;
		void *fmt;
	} realsamp;
	uint32_t length;
	uint32_t loopstart;
	uint32_t loopend;
	uint32_t replen;
	int32_t step;
	uint32_t pos;
	uint16_t fpos;
	uint16_t status;
	union
	{
		uint32_t *voltabs[2];
		int16_t vols[2];
		float volfs[2];
	} vol;
};

extern int mixInit(void (*getchan)(unsigned int ch, struct mixchannel *chn, uint32_t rate), int resamp, unsigned int chan, int amp);
extern void mixClose(void);
extern void mixSetAmplify(int amp);
extern void mixGetRealVolume(int ch, int *l, int *r);
extern void mixGetMasterSample(int16_t *s, unsigned int len, uint32_t rate, int opt);
extern int mixGetChanSample(unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);
extern int mixAddChanSample(unsigned int ch, int16_t *s, unsigned int len, uint32_t rate);
extern void mixGetRealMasterVolume(int *l, int *r);

#define MIX_PLAYING 1
#define MIX_MUTE 2
#define MIX_LOOPED 4
#define MIX_PINGPONGLOOP 8
#define MIX_PLAY16BIT 16
#define MIX_INTERPOLATE 32
#define MIX_MAX 64
#define MIX_PLAYFLOAT 128
#define MIX_ALL 255

#endif
