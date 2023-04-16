#ifndef PLAYMP2_MPTYPE_H
#define PLAYMP2_MPTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int ampeg_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void ampeg_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct mpegPlayer;

#endif
