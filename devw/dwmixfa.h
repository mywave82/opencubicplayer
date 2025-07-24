#define MIXF_MIXBUFLEN 4096
#define MIXF_MAXCHAN 255

#define MIXF_INTERPOLATE 1
#define MIXF_INTERPOLATEQ 2
#define MIXF_FILTER 4
#define MIXF_PLAYSTEREO 8
#define MIXF_QUIET 16
#define MIXF_LOOPED 32

#define MIXF_PLAYING 256
#define MIXF_MUTE 512

struct cpifaceSessionAPI_t;
extern void mixer (struct cpifaceSessionAPI_t *);
extern void prepare_mixer (void);
extern void getchanvol (int n, int len, float * const voll, float * const volr);

#define MAXVOICES MIXF_MAXCHAN

typedef struct
{
	uint32_t  freqw;     /* frequency (whole part) */
	uint32_t  freqf;     /* frequency (fractional part) */

	float    *smpposw;   /* sample position (whole part (pointer!)) */
	uint32_t  smpposf;   /* sample position (fractional part) */

	float    *loopend;   /* pointer to loop end */
	uint32_t  looplen;   /* loop length in samples */

	float   volleft;     /* float: left volume (1.0=normal) */
	float   volright;    /* float: right volume (1.0=normal) */
	float   rampleft;    /* float: left volramp (dvol/sample) */
	float   rampright;   /* float: right volramp (dvol/sample) */

	uint32_t voiceflags; /* voice status flags */

	float   ffreq;       /* filter frequency (0<=x<=1) */
	float   freso;       /* filter resonance (0<=x<1) */

	float   fl1;         /* filter lp buffer */
	float   fb1;         /* filter bp buffer */

	float   fl2;         /* filter lp buffer, right channel in stereo samples */
	float   fb2;         /* filter bp buffer, right channel in stereo samples */
} dwmixfa_channel_t;


typedef struct
{
	float    *tempbuf;         /* ptr to 32 bit temp-buffer */
	void     *outbuf;          /* ptr to 16 bit mono-buffer */
	uint32_t  nsamples;        /* # of samples to generate */
	uint32_t  nvoices;         /* # of voices */

	dwmixfa_channel_t ch[MIXF_MAXCHAN];

	float   fadeleft, faderight; /* global declicking. Last sample is accumulated into register, register has roll-off as it is applied to the output buffer */

	float   ct0[256]; /* interpolation tab for s[-1] */
	float   ct1[256]; /* interpolation tab for s[0] */
	float   ct2[256]; /* interpolation tab for s[1] */
	float   ct3[256]; /* interpolation tab for s[2] */

	uint32_t samprate;

#define MIXF_MAX_POSTPROC 10
	const struct PostProcFPRegStruct *postproc[MIXF_MAX_POSTPROC];
	int                               postprocs;
} dwmixfa_state_t;

extern dwmixfa_state_t dwmixfa_state;
