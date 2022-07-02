#ifndef _PLAYSID_CPIINFO_H
#define _PLAYSID_CPIINFO_H 1

extern void __attribute__ ((visibility ("internal"))) SidInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
extern void __attribute__ ((visibility ("internal"))) SidInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
