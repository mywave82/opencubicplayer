#ifndef SIDPLAY_SIDCONFIG_H
#define SIDPLAY_SIDCONFIG_H 1

struct PluginInitAPI_t;
struct PluginCloseAPI_t;

OCP_INTERNAL int sid_config_init (struct PluginInitAPI_t *API);
OCP_INTERNAL void sid_config_done (struct PluginCloseAPI_t *API);

#endif
