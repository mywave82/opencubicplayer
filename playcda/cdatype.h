#ifndef PLAYCDA_CDATYPE_H
#define PLAYCDA_CDATYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int cda_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void cda_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct cdaPlayer;

#endif
