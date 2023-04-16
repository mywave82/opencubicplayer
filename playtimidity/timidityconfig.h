#ifndef PLAYTIMIDITY_TIMIDITYCONFIG_H
#define PLAYTIMIDITY_TIMIDITYCONFIG_H 1

struct PluginInitAPI_t;
struct PluginCloseAPI_t;

OCP_INTERNAL int timidity_config_init (struct PluginInitAPI_t *API);
OCP_INTERNAL void timidity_config_done (struct PluginCloseAPI_t *API);

#endif
