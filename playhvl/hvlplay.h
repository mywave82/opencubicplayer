#ifndef __HLVPLAY_H
#define __HLVPLAY_H 1

struct ocpfilehandle_t;
struct hvl_tune;

#define current_hvl_tune ht
struct cpifaceSessionAPI_t;
extern struct hvl_tune __attribute__ ((visibility ("internal"))) *current_hvl_tune;
extern struct hvl_tune __attribute__ ((visibility ("internal"))) *hvlOpenPlayer (const uint8_t *mem, size_t memlen, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpiSessionAPI);
extern void            __attribute__ ((visibility ("internal")))  hvlClosePlayer (void);
extern void            __attribute__ ((visibility ("internal")))  hvlIdle (void);
extern void            __attribute__ ((visibility ("internal")))  hvlSetLoop (uint8_t s);
extern char            __attribute__ ((visibility ("internal")))  hvlLooped (void);
extern void            __attribute__ ((visibility ("internal")))  hvlPause (uint8_t p);
extern void            __attribute__ ((visibility ("internal")))  hvlPrevSubSong ();
extern void            __attribute__ ((visibility ("internal")))  hvlRestartSong ();
extern void            __attribute__ ((visibility ("internal")))  hvlNextSubSong ();
extern void            __attribute__ ((visibility ("internal")))  hvlGetStats (int *row, int *rows, int *order, int *orders, int *subsong, int *subsongs, int *tempo, int *speedmult);
extern void            __attribute__ ((visibility ("internal")))  hvlMute (int ch, int m);

/* This is for hvlpinst.c */
extern __attribute__ ((visibility ("internal"))) uint8_t plInstUsed[256];


/* This is for hvlpchan.c */
struct hvl_chaninfo
{
	const char *name; /* should only be set if name is non-zero-length */
	uint8_t vol;
	uint8_t notehit; /* none-zero if note is fresh */
	uint8_t note; /* need to match syntax with plNoteStr[] */
	uint16_t noteperiod;
	uint8_t pan;
	uint8_t pitchslide;
	uint8_t volslide;
	int16_t ins;
	uint8_t fx,   fxparam; /* effect for given row */
	uint8_t fxB,  fxBparam; /* effect B for given row (HVL files only) */
	uint8_t pfx,  pfxparam; /* current effect for instrument internal playlist */
	uint8_t pfxB, pfxBparam; /* current effect B for instrument internal playlist (HVL files only) */
	uint8_t waveform; /* 0-3 */
	uint8_t filter;   /* 0x00 - 0x3f */

	int muted; /* force-muted - TODO */
};
void hvlGetChanInfo (int chan, struct hvl_chaninfo *ci);
void hvlGetChanVolume (int chan, int *l, int *r);

int __attribute__ ((visibility ("internal"))) hvlGetChanSample(unsigned int ch, int16_t *s, unsigned int len, uint32_t rate, int opt);

#endif
