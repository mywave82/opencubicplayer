#ifndef __SETS_H
#define __SETS_H

struct settings
{
	int16_t amp;    /* [4..508]   64=nominal */
	int16_t speed;  /* [16..2048] 256=nominal */
	int16_t pitch;  /* [16..2048] 256=nominal */
	int16_t pan;    /* [-64..64] 64=nominal */
	int16_t bal;    /* [-64..64] 0=nomimal */
	int16_t vol;    /* [0..64] 64=nominal */
	int16_t srnd;   /* 0: normal stereo                    1: one channel is inverted */
	int16_t reverb; /* [-64..64] 0=nominal */
	int16_t chorus; /* [-64..64] 0=nominal */
	uint8_t filter;
	uint8_t useecho;
	uint8_t splock; /* 0: speed,pitch are independent      1: speed,pitch are locked */
	uint8_t viewfx; /* 0: volume,panning,balance,surround  1: echo,revert,chorous (not implemented yet) */
};

extern struct settings set;

#endif
