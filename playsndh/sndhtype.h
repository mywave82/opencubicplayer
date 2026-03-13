#ifndef PLAYSNDH_SNDHTYPE_H
#define PLAYSNDH_SNDHTYPE_H 1

struct PluginInitAPI_t;
OCP_INTERNAL int sndh_type_init (struct PluginInitAPI_t *API);

struct PluginCloseAPI_t;
OCP_INTERNAL void sndh_type_done (struct PluginCloseAPI_t *API);

extern OCP_INTERNAL const struct cpifaceplayerstruct sndhPlayer;

struct sndhMeta_t
{
	int fileversion;
	char *title;
	char **titles; /* subsongs */
	char *composer;
	char *ripper;
	char *converter;
	uint16_t subtunes;
	uint16_t defaultsubtune;
	uint16_t year;
	char *flag; /* global for file */
	char **flags; /* subsongs */
	uint16_t timer_a, timer_b, timer_c, timer_d, vbl;
	uint16_t *times; /* subsongs */
	uint32_t *frames; /* subsongs */
};

void sndhMetaFree (struct sndhMeta_t *meta);

struct sndhMeta_t *sndhReadInfos(const uint8_t *data, size_t len);


#endif
