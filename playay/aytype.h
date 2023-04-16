#ifndef PLAYAY_AYTYPE_H
#define PLAYAY_AYTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int ay_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void ay_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct ayPlayer;

#endif
