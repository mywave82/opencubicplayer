#ifndef __DEVIWAVE_H
#define __DEVIWAVE_H

struct mcpDevAPI_t;

struct mcpDriver_t
{
	char name[32];        /* includes the NULL termination */
	char description[64]; /* includes the NULL termination */

	int                       (*Detect) (const struct mcpDriver_t *driver); /* 0 = driver not functional, 1 = driver is functional */
	const struct mcpDevAPI_t *(*Open)   (const struct mcpDriver_t *driver);
	void                      (*Close)  (const struct mcpDriver_t *driver);
};
extern void mcpRegisterDriver (const struct mcpDriver_t *driver);
extern void mcpUnregisterDriver(const struct mcpDriver_t *driver);

#endif
