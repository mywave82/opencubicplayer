#ifndef _DWMIX_H
#define _DWMIX_H

/* This is channel options */
#define MIXRQ_PLAYING 1
#define MIXRQ_MUTE 2
#define MIXRQ_LOOPED 4
#define MIXRQ_PINGPONGLOOP 8
#define MIXRQ_PLAY16BIT 16
#define MIXRQ_INTERPOLATE 32
#define MIXRQ_INTERPOLATEMAX 64
#define MIXRQ_PLAYSTEREO 128 /* this flag is local to devwmix.c only */

#define MIXQ_PLAYING		MIXRQ_PLAYING
/*#define MIXQ_PAUSED		MIXRQ_MUTE       not used */
#define MIXQ_LOOPED		MIXRQ_LOOPED
#define MIXQ_PINGPONGLOOP	MIXRQ_PINGPONGLOOP
#define MIXQ_PLAY16BIT		MIXRQ_PLAY16BIT
#define MIXQ_INTERPOLATE	MIXRQ_INTERPOLATE
#define MIXQ_INTERPOLATEMAX	MIXRQ_INTERPOLATEMAX
#define MIXQ_PLAYSTEREO		MIXRQ_PLAYSTEREO /* this flag is local to devwmix.c only */

/* This is not a channel option, but a mixer option */
#define MIXRQ_RESAMPLE 1

struct channel
{
	void *samp;
	union
	{
		int8_t *bit8;
		int16_t *bit16;
		int32_t *bit32;
	} realsamp;
	uint32_t length;
	uint32_t loopstart;
	uint32_t loopend;
	uint32_t replen;
	int32_t step;
	uint32_t pos;
	uint16_t fpos;
	uint16_t status;
	int32_t curvols[4]; /* negativ volume = panning */
	int32_t dstvols[4]; /* -- " -- */

	int32_t vol[2];     /* -- " -- */
	int32_t orgvol[2];  /* -- " -- */
	uint32_t orgrate;
	uint32_t orgfrq;
	uint32_t orgdiv;
	int volopt;   /* only one flag:   0x01 srnd */
	int orgvolx;  /* 0 - 0x100 */
	int orgpan;   /* -0x80 - +0x7f */
	int samptype; /* samptype&mcpSampLoop mcpSampSLoop mcpSampSBiDi mcpSamp16Bit mcpSampStereo */
	uint32_t orgloopstart;
	uint32_t orgloopend;
	uint32_t orgsloopstart;
	uint32_t orgsloopend;
};

#endif
