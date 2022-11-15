#ifndef PLAYTIMIDITY_TIMIDITYTYPE_H
#define PLAYTIMIDITY_TIMIDITYTYPE_H 1

struct PluginInitAPI_t;
struct PluginCloseAPI_t;

int __attribute__ ((visibility ("internal"))) timidity_type_init (struct PluginInitAPI_t *API);
void __attribute__ ((visibility ("internal"))) timidity_type_done (struct PluginCloseAPI_t *API);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) timidityPlayer;

#endif
