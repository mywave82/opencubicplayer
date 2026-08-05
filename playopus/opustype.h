#ifndef PLAYOGG_OPUSTYPE_H
#define PLAYOGG_OPUSTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int opus_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void opus_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct opusPlayer;

#endif
