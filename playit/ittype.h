#ifndef PLAYIT_ITTYPE_H
#define PLAYIT_ITTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int it_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void it_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct itPlayer;

#endif
