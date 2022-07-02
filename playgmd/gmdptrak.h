#ifndef __GMDTRK_H
#define __GMDTRK_H

struct cpifaceSessionAPI_t;
extern void __attribute__ ((visibility ("internal"))) gmdTrkSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct gmdmodule *m);

#endif
