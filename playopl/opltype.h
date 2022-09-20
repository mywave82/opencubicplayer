#ifndef PLAYOPL_OPLTYPE_H
#define PLAYOPL_OPLTYPE_H 1

extern "C"
{
	struct PluginInitAPI_t;
	struct PluginCloseAPI_t;
}

int __attribute__ ((visibility ("internal"))) opl_type_init (PluginInitAPI_t *API);
void __attribute__ ((visibility ("internal"))) opl_type_done (PluginCloseAPI_t *API);

extern "C"
{
	extern const struct cpifaceplayerstruct __attribute__ ((visibility ("internal"))) oplPlayer;
}

#endif
