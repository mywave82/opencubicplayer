#ifndef PLAYQOA_WAVTYPE_H
#define PLAYQOA_WAVTYPE_H

struct PluginInitAPI_t;
OCP_INTERNAL int qoa_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void qoa_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct qoaPlayer;

#endif
