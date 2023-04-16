#ifndef PLAYOPL_OPLTYPE_H
#define PLAYOPL_OPLTYPE_H 1

extern "C"
{
	struct PluginInitAPI_t;
	struct PluginCloseAPI_t;
}

OCP_INTERNAL int opl_type_init (PluginInitAPI_t *API);
OCP_INTERNAL void opl_type_done (PluginCloseAPI_t *API);

extern "C"
{
	extern OCP_INTERNAL const struct cpifaceplayerstruct oplPlayer;
}

#endif
