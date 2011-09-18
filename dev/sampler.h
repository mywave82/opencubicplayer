#ifndef __SAMPLER_H
#define __SAMPLER_H

#define SMP_STEREO 1
#define SMP_16BIT 2
#define SMP_SIGNEDOUT 4
#define SMP_REVERSESTEREO 8

#define SMP_MIC 0
#define SMP_LINEIN 1
#define SMP_CD 2

enum
{
	smpGetSampleStereo=1
};

extern int smpRate;
extern int smpOpt;
extern int smpBufSize;
extern int (*smpSample)(unsigned char **buf, int *len);
extern void (*smpStop)(void);
extern void (*smpSetOptions)(int rate, int opt);
extern void (*smpSetSource)(int src);
extern int (*smpGetBufPos)(void);

extern int smpOpenSampler(void **buf, int *len, int blen);
extern void smpCloseSampler(void);
extern void smpGetRealMasterVolume(int *l, int *r);
extern void smpGetMasterSample(int16_t *s, unsigned int len, uint32_t rate, int opt);

#endif
