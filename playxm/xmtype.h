#ifndef PLAYXM_XMTYPE_H
#define PLAYXM_XMTYPE_H 1

struct PluginInitAPI_t;
int __attribute__((visibility ("internal"))) xm_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
void __attribute__((visibility ("internal"))) xm_type_done (struct PluginCloseAPI_t *API);

extern const struct cpifaceplayerstruct __attribute__((visibility ("internal"))) xmpPlayer;

#endif
