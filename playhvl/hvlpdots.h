#ifndef _HVLPDOTS_H
#define _HVLPDOTS_H 1

struct cpifaceSessionAPI_t;
int __attribute__ ((visibility ("internal"))) hvlGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *d, int max);

#endif
