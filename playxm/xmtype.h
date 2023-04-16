#ifndef PLAYXM_XMTYPE_H
#define PLAYXM_XMTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int xm_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void xm_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct xmpPlayer;

#endif
