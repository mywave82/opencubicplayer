#ifndef _PLAYSID_CPIINFO_H
#define _PLAYSID_CPIINFO_H 1

struct cpifaceSessionAPI_t;
OCP_INTERNAL void SidInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void SidInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
