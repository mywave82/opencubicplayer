#ifndef PLAYHVL_HVLTYPE_H
#define PLAYHVL_HVLTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int hvl_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void hvl_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct hvlPlayer;

#endif
