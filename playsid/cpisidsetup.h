#ifndef _PLAYSID_CPISIDSETUP_H
#define _PLAYSID_CPISIDSETUP_H 1

struct cpifaceSessionAPI_t;
OCP_INTERNAL void cpiSidSetupInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void cpiSidSetupDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
