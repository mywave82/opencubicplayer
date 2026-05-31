#ifndef PLAYWAVPACK_WAVPACKTYPE_H
#define PLAYWAVPACK_WAVPACKTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int wavpack_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void wavpack_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct wavpackPlayer;


#endif
