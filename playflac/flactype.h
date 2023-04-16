#ifndef PLAYFLAC_FLACTYPE_H
#define PLAYFLAC_FLACTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int flac_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void flac_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct flacPlayer;

#endif
