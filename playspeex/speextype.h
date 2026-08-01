#ifndef PLAYOGG_SPEEXTYPE_H
#define PLAYOGG_SPEEXTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int speex_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void speex_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct speexPlayer;

#endif
