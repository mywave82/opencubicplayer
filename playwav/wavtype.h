#ifndef PLAYWAV_WAVTYPE_H
#define PLAYWAV_WAVTYPE_H

struct PluginInitAPI_t;
int __attribute__ ((visibility ("internal"))) wav_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
void __attribute__ ((visibility ("internal"))) wav_type_done (struct PluginCloseAPI_t *API);

extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) wavPlayer;

#endif
