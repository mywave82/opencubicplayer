#ifndef __GMDPCHAN_H
#define __GMDPCHAN_H

struct gmdmodule;
struct cpifaceSessionAPI_t;
void __attribute__ ((visibility ("internal"))) gmdChanSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct gmdmodule *);

#endif
