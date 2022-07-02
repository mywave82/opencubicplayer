#ifndef _CPITIMIDITYSETUP_H
#define _CPITIMIDITYSETUP_H 1

struct cpifaceSessionAPI_t;
void __attribute__ ((visibility ("internal"))) cpiTimiditySetupInit (struct cpifaceSessionAPI_t *cpifaceSession);
void __attribute__ ((visibility ("internal"))) cpiTimiditySetupDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
