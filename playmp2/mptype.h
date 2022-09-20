#ifndef PLAYMP2_MPTYPE_H
#define PLAYMP2_MPTYPE_H 1

struct PluginInitAPI_t;
int __attribute__ ((visibility ("internal"))) ampeg_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
void __attribute__ ((visibility ("internal"))) ampeg_type_done (struct PluginCloseAPI_t *API);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) mpegPlayer;

#endif
