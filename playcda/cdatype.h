#ifndef PLAYCDA_CDATYPE_H
#define PLAYCDA_CDATYPE_H 1

struct PluginInitAPI_t;
int __attribute__ ((visibility ("internal"))) cda_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
void __attribute__ ((visibility ("internal"))) cda_type_done (struct PluginCloseAPI_t *API);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) cdaPlayer;

#endif
