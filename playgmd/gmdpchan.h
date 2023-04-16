#ifndef __GMDPCHAN_H
#define __GMDPCHAN_H

struct gmdmodule;
struct cpifaceSessionAPI_t;
OCP_INTERNAL void gmdChanSetup (struct cpifaceSessionAPI_t *cpifaceSession, const struct gmdmodule *);

#endif
