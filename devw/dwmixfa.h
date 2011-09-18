#define MIXF_MIXBUFLEN 4096
#define MIXF_MAXCHAN 255

//#define MIXF_PLAYSTEREO 0     Floating point mixer doesn't support stereo samples, only mono to stereo output
#define MIXF_INTERPOLATE 2
#define MIXF_INTERPOLATEQ 4
#define MIXF_FILTER 8
#define MIXF_QUIET 16
#define MIXF_LOOPED 32

#define MIXF_PLAYING 256
#define MIXF_MUTE 512

#define MIXF_UNSIGNED 1
#define MIXF_16BITOUT 2

#define MIXF_VOLRAMP  256
#define MIXF_DECLICK  512

extern void mixer (void);
extern void prepare_mixer (void);
extern void getchanvol (int n, int len);

#define MAXVOICES MIXF_MAXCHAN

typedef struct
{
	float    *tempbuf;         /* ptr to 32 bit temp-buffer */
	void     *outbuf;          /* ptr to 16 bit mono-buffer */
	uint32_t  nsamples;        /* # of samples to generate */
	uint32_t  nvoices;         /* # of voices */

	uint32_t  freqw[MIXF_MAXCHAN]; /* frequency (whole part) */
	uint32_t  freqf[MIXF_MAXCHAN]; /* frequency (fractional part) */

	float    *smpposw[MIXF_MAXCHAN]; /* sample position (whole part (pointer!)) */
	uint32_t  smpposf[MIXF_MAXCHAN]; /* sample position (fractional part) */

	float    *loopend[MIXF_MAXCHAN]; /* pointer to loop end */
	uint32_t  looplen[MIXF_MAXCHAN]; /* loop length in samples */

	float   volleft[MIXF_MAXCHAN];   /* float: left volume (1.0=normal) */
	float   volright[MIXF_MAXCHAN];  /* float: right volume (1.0=normal) */
	float   rampleft[MIXF_MAXCHAN];  /* float: left volramp (dvol/sample) */
	float   rampright[MIXF_MAXCHAN]; /* float: right volramp (dvol/sample) */

	uint32_t voiceflags[MIXF_MAXCHAN]; /* voice status flags */

	float   ffreq[MIXF_MAXCHAN]; /* filter frequency (0<=x<=1) */
	float   freso[MIXF_MAXCHAN]; /* filter resonance (0<=x<1) */

	float   fadeleft,faderight; /* temp holding register - TODO, check if they should be local for channel */

	float   fl1[MIXF_MAXCHAN]; /* filter lp buffer */
	float   fb1[MIXF_MAXCHAN]; /* filter bp buffer */

	int     isstereo;   /* flag for stereo output */
	int     outfmt;     /* output format */
	float   voll, volr; /* output volume */

	float   ct0[256]; /* interpolation tab for s[-1] */
	float   ct1[256]; /* interpolation tab for s[0] */
	float   ct2[256]; /* interpolation tab for s[1] */
	float   ct3[256]; /* interpolation tab for s[2] */

	uint32_t samprate;

	struct mixfpostprocregstruct *postprocs; /* TODO */

	/* private to mixer, used be dwmixfa_8087.c */
	float volrl; /* volume current left */
	float volrr; /* volume current right */
	uint16_t clipval;    /* used in clippers in order to transfer into register */
	uint32_t mixlooplen; /* lenght of loop in samples*/
	uint32_t looptype;  /* local version of voiceflags[N] */
	float magic1;  /* internal dumping variable for filters */
	float ffrq;
	float frez;
	float __fl1;
	float __fb1;
} dwmixfa_state_t;

extern dwmixfa_state_t dwmixfa_state;



#ifdef I386_ASM

extern void start_dwmixfa(void);   /* these two are used to calculate memory remapping (self modifying code) */
extern void stop_dwmixfa(void);

#endif
