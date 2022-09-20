#ifndef PLAYYM_YMTYPE_H
#define PLAYYM_YMTYPE_H 1

extern "C"
{
	struct PluginInitAPI_t;
	struct PluginCloseAPI_t;
}

int __attribute__((visibility ("internal"))) ym_type_init (PluginInitAPI_t *API);

void __attribute__((visibility ("internal"))) ym_type_done (PluginCloseAPI_t *API);

extern "C"
{
	extern const struct cpifaceplayerstruct __attribute__((visibility ("internal"))) ymPlayer;
}

#endif
