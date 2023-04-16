#ifndef PLAYTIMIDITY_TIMIDITYTYPE_H
#define PLAYTIMIDITY_TIMIDITYTYPE_H 1

struct PluginInitAPI_t;
struct PluginCloseAPI_t;

OCP_INTERNAL int timidity_type_init (struct PluginInitAPI_t *API);
OCP_INTERNAL void timidity_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct timidityPlayer;

#endif
