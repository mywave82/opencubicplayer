#ifndef _DEV_DEVIPLAY_H
#define _DEV_DEVIPLAY_H 1

struct plrDevAPI_t;
struct ringbufferAPI_t;

struct plrDriver_t
{
	char name[32];        /* includes the NULL termination */
	char description[64]; /* includes the NULL termination */

	int                       (*Detect) (const struct plrDriver_t *driver); /* 0 = driver not functional, 1 = driver is functional */
	const struct plrDevAPI_t *(*Open)   (const struct plrDriver_t *driver, const struct ringbufferAPI_t *ringbufferAPI);
	void                      (*Close)  (const struct plrDriver_t *driver);
};
extern void plrRegisterDriver (const struct plrDriver_t *driver);
extern void plrUnregisterDriver(const struct plrDriver_t *driver);

#endif
