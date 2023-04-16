#ifndef PLAYGMD_GMDTYPE_H
#define PLAYGMD_GMDTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int gmd_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void gmd_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayer669;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerAMS;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerDMF;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerMDL;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerMTM;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerOKT;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerPTM;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerS3M;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerSTM;
extern OCP_INTERNAL const struct cpifaceplayerstruct gmdPlayerULT;

#endif
