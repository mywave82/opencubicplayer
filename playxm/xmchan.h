#ifndef PLAYXM_XMPCHAN_H
#define PLAYXM_XMPCHAN_H 1

struct cpifaceSessionAPI_t;
struct xmpinstrument;
struct xmpsample;

OCP_INTERNAL void xmChanSetup (struct cpifaceSessionAPI_t *cpifaceSession, struct xmpinstrument *insts, struct xmpsample *samps);

#endif
