#ifndef _PLINKMAN_H
#define _PLINKMAN_H 1

struct mdbreadinforegstruct;
struct moduletype;
struct cpifaceplayerstruct;
struct PluginInitAPI_t
{
	void (*mdbRegisterReadInfo)(struct mdbreadinforegstruct *r);
	void (*fsTypeRegister) (struct moduletype modtype, const char **description, const char *interface, const struct cpifaceplayerstruct *cp);
	void (*fsRegisterExt)(const char *ext);
};

struct PluginCloseAPI_t
{
	void (*mdbUnregisterReadInfo)(struct mdbreadinforegstruct *r);
	void (*fsTypeUnregister) (struct moduletype modtype);
};

struct __attribute__ ((aligned (64))) linkinfostruct
{
	const char *name;
	const char *desc;
	uint32_t ver;
	uint32_t sortindex;

	int (*PreInit)(void); /* high priority init */
	int (*Init)(void);
	int (*LateInit)(void);
	int (*PluginInit)(struct PluginInitAPI_t *API);
	void (*PluginClose)(struct PluginCloseAPI_t *API);
	void (*PreClose)(void);
	void (*Close)(void);
	void (*LateClose)(void); /* low priority Close */
};

struct dll_handle
{
	void *handle;
	char *file;
	int id;
	int refcount;
	uint32_t size;
	const struct linkinfostruct *info;
};
extern int loadlist_n;

extern int lnkLinkDir(const char *dir);
extern int lnkLink(const char *files);
extern void lnkFree(const int id);
extern void lnkInit(void);
#define _lnkGetSymbol(name) lnkGetSymbol(0, name)
extern void *lnkGetSymbol(const int id, const char *name);
extern char *lnkReadInfoReg(const int id, const char *key);
extern int lnkCountLinks(void);
extern int lnkGetLinkInfo(struct linkinfostruct *l, uint32_t *size, int index);

int lnkInitAll (void);
int lnkPluginInitAll (struct PluginInitAPI_t *API);
void lnkPluginCloseAll (struct PluginCloseAPI_t *API);
void lnkCloseAll (void);

#ifdef SUPPORT_STATIC_PLUGINS
# define DLLEXTINFO_BEGIN_PREFIX const __attribute__ ((section ("plugin_list"))) __attribute__ ((used))
# define DLLEXTINFO_END_PREFIX   static const __attribute__ ((section ("plugin_list"))) __attribute__ ((used))
#else
# define DLLEXTINFO_BEGIN_PREFIX
# define DLLEXTINFO_END_PREFIX
#endif

#ifdef STATIC_CORE
# define DLLEXTINFO_CORE_PREFIX  static const __attribute__ ((section ("plugin_list"))) __attribute__ ((used))
#else
# define DLLEXTINFO_CORE_PREFIX const
#endif

#define DLLEXTINFO_DRIVER_PREFIX       const
#define DLLEXTINFO_PLAYBACK_PREFIX     const
#define DLLEXTINFO_PLAYBACK_PREFIX_CPP /* C++, const implies static */

#endif
