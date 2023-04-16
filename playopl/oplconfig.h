#ifndef PLAYOPL_OPLCONFIG_H
#define PLAYOPL_OPLCONFIG_H 1

struct PluginInitAPI_t;
struct PluginCloseAPI_t;

OCP_INTERNAL int opl_config_init (struct PluginInitAPI_t *API);
OCP_INTERNAL void opl_config_done (struct PluginCloseAPI_t *API);

#endif
