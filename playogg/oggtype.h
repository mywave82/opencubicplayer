#ifndef PLAYOGG_OGGTYPE_H
#define PLAYOGG_OGGTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int ogg_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void ogg_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct oggPlayer;

#endif
