#ifndef __MIX_H
#define __MIX_H

struct mixchannel
{
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
		int16_t vols[2];
		float volfs[2];
	} vol;
	uint32_t *voltabs[2]; /* internal use by dev/mix.c and dev/mixasm.c */
};

struct cpifaceSessionAPI_t; /* cpiface.h */
struct PostProcFPRegStruct;
struct PostProcIntegerRegStruct;

struct mixAPI_t
{
	int  (*mixInit)      (struct cpifaceSessionAPI_t *cpifaceSession, void (*getchan)(unsigned int ch, struct mixchannel *chn, uint32_t rate), int resamp, unsigned int chan, int amp);
	void (*mixClose)     (struct cpifaceSessionAPI_t *cpifaceSession);
	void (*mixSetAmplify)(struct cpifaceSessionAPI_t *cpifaceSession, int amp);
	const struct PostProcFPRegStruct *(*mcpFindPostProcFP) (const char *name);
	const struct PostProcIntegerRegStruct *(*mcpFindPostProcInteger) (const char *name);
};
extern const struct mixAPI_t *mixAPI;

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
