#ifndef PLAYIT_ITTYPE_H
#define PLAYIT_ITTYPE_H 1

struct PluginInitAPI_t;
int __attribute__ ((visibility ("internal"))) it_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
void __attribute__ ((visibility ("internal"))) it_type_done (struct PluginCloseAPI_t *API);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) itPlayer;

#endif
