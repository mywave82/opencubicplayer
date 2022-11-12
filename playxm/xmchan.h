#ifndef PLAYXM_XMPCHAN_H
#define PLAYXM_XMPCHAN_H 1

struct cpifaceSessionAPI_t;
struct xmpinstrument;
struct xmpsample;

void __attribute__ ((visibility ("internal"))) xmChanSetup(struct cpifaceSessionAPI_t *cpifaceSession, struct xmpinstrument *insts, struct xmpsample *samps);

#endif
