#ifndef _HVLPINST_H
#define _HVLPINST_H 1

struct cpifaceSessionAPI_t;

void __attribute__ ((visibility ("internal"))) hvlInstSetup (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
