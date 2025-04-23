#ifndef _DWMIX_H
#define _DWMIX_H

/* This is channel options */
#define MIXRQ_INTERPOLATE 1
#define MIXRQ_INTERPOLATEMAX 2
#define MIXRQ_PLAY16BIT 4
#define MIXRQ_PLAYSTEREO 8

#define MIXRQ_LOOPED 32
#define MIXRQ_PINGPONGLOOP 64

#define MIXRQ_PLAYING 128
#define MIXRQ_MUTE 512

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
	int samptype; /* samptype&mcpSampLoop mcpSampSLoop mcpSampSBiDi mcpSamp16Bit mcpSampInterleavedStereo */
	uint32_t orgloopstart;
	uint32_t orgloopend;
	uint32_t orgsloopstart;
	uint32_t orgsloopend;
};

#endif
