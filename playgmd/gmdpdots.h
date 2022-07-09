#ifndef __GMDPDOTS_H
#define __GMDPDOTS_H

struct cpifaceSessionAPI_t;
extern int __attribute__ ((visibility ("internal"))) gmdGetDots (struct cpifaceSessionAPI_t *cpifaceSession, struct notedotsdata *, int);

#endif
