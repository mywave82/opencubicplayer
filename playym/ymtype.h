#ifndef PLAYYM_YMTYPE_H
#define PLAYYM_YMTYPE_H 1

extern "C"
{
	struct PluginInitAPI_t;
	struct PluginCloseAPI_t;
}

OCP_INTERNAL int ym_type_init (PluginInitAPI_t *API);

OCP_INTERNAL void ym_type_done (PluginCloseAPI_t *API);

extern "C"
{
	extern OCP_INTERNAL const struct cpifaceplayerstruct ymPlayer;
}

#endif
