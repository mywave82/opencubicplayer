#ifndef _DEV_DEVIPLAY_H
#define _DEV_DEVIPLAY_H 1

struct plrDevAPI_t;
struct ringbufferAPI_t;

struct plrDriverAPI_t
{
	const struct ringbufferAPI_t *ringbufferAPI;
	void (*GetRealMasterVolume) (int *l, int *r); /* default functions that can be used */
	void (*GetMasterSample) (int16_t *s, uint32_t len, uint32_t rate, int opt); /* default functions that can be used */
	void (*ConvertBufferFromStereo16BitSigned) (void *dstbuf, int16_t *srcbuf, int samples, int to16bit, int tosigned, int tostereo, int revstereo);
};

struct plrDriver_t
{
	char name[32];        /* includes the NULL termination */
	char description[64]; /* includes the NULL termination */

	int                       (*Detect) (const struct plrDriver_t *driver); /* 0 = driver not functional, 1 = driver is functional */
	const struct plrDevAPI_t *(*Open)   (const struct plrDriver_t *driver, const struct plrDriverAPI_t *plrDriverAPI);
	void                      (*Close)  (const struct plrDriver_t *driver);
};
extern void plrRegisterDriver (const struct plrDriver_t *driver);
extern void plrUnregisterDriver(const struct plrDriver_t *driver);

#endif
