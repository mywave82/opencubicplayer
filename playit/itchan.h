#ifndef PLAYIT_ITPCHAN_H
#define PLAYIT_ITPCHAN_H 1

struct cpifaceSessionAPI_t;
struct it_instrument;
struct it_sample;

OCP_INTERNAL void itChanSetup (struct cpifaceSessionAPI_t *cpifaceSession, struct it_instrument *insts, struct it_sample *samps);

#endif
