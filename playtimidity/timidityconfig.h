#ifndef PLAYTIMIDITY_TIMIDITYCONFIG_H
#define PLAYTIMIDITY_TIMIDITYCONFIG_H 1

struct PluginInitAPI_t;
struct PluginCloseAPI_t;

int __attribute__ ((visibility ("internal"))) timidity_config_init (struct PluginInitAPI_t *API);
void __attribute__ ((visibility ("internal"))) timidity_config_done (struct PluginCloseAPI_t *API);

#endif
