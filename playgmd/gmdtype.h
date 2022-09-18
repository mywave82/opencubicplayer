#ifndef PLAYGMD_GMDTYPE_H
#define PLAYGMD_GMDTYPE_H 1

int __attribute__ ((visibility ("internal"))) gmd_type_init (void);
void __attribute__ ((visibility ("internal"))) gmd_type_done (void);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayer669;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerAMS;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerDMF;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerMDL;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerMTM;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerOKT;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerPTM;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerS3M;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerSTM;
extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) gmdPlayerULT;

#endif
