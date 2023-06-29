#ifndef PLAYOPL_OPLCONFIG_H

#define PLAYOPL_OPLCONFIG_H 1

struct PluginInitAPI_t;
struct PluginCloseAPI_t;
struct configAPI_t;

OCP_INTERNAL char *opl_config_retrowave_device (const struct PipeProcessAPI_t *PipeProcess, const struct configAPI_t *configAPI);

OCP_INTERNAL int opl_config_init (struct PluginInitAPI_t *API);
OCP_INTERNAL void opl_config_done (struct PluginCloseAPI_t *API);

#endif
