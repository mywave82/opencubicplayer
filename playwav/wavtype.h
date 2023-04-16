#ifndef PLAYWAV_WAVTYPE_H
#define PLAYWAV_WAVTYPE_H

struct PluginInitAPI_t;
OCP_INTERNAL int wav_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void wav_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct wavPlayer;

#endif
