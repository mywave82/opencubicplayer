#ifndef SIDPLAY_SIDTYPE_H
#define SIDPLAY_SIDTYPE_H

struct PluginInitAPI_t;
struct PluginCloseAPI_t;

OCP_INTERNAL int sid_type_init (struct PluginInitAPI_t *API);
OCP_INTERNAL void sid_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct sidPlayer;

#endif
