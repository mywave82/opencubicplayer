#ifndef _GME_TYPE
#define _GME_TYPE

struct PluginInitAPI_t;
OCP_INTERNAL int gme_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void gme_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct gmePlayer;

#endif
