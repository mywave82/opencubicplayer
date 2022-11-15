#ifndef _PLINKMAN_H
#define _PLINKMAN_H 1

struct mdbreadinforegstruct;
struct moduletype;
struct cpifaceplayerstruct;
struct configAPI_t;
struct ocpfile_t;
struct ocpdir_t;
struct ocpfilehandle_t;
struct moduleinfostruct;
struct DevInterfaceAPI_t;
struct dmDrive;

struct PluginInitAPI_t
{
	void (*mdbRegisterReadInfo)(struct mdbreadinforegstruct *r);
	void (*fsTypeRegister) (struct moduletype modtype, const char **description, const char *interface, const struct cpifaceplayerstruct *cp);
	void (*fsRegisterExt)(const char *ext);
	const struct configAPI_t *configAPI;

	void (*filesystem_setup_register_file) (struct ocpfile_t *file);
	struct ocpfile_t *(*dev_file_create)
	(
		struct ocpdir_t *parent,
		const char *devname,
		const char *mdbtitle,
		const char *mdbcomposer,
		void *token,
		int  (*Init)       (void **token, struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct DevInterfaceAPI_t *API), // Client can change token for instance, it defaults to the provided one
		void (*Run)        (void **token,                                                           const struct DevInterfaceAPI_t *API), // Client can change token for instance
		void (*Close)      (void **token,                                                           const struct DevInterfaceAPI_t *API), // Client can change token for instance
		void (*Destructor) (void  *token)
	);
	struct dmDrive *dmSetup;
};

struct PluginCloseAPI_t
{
	void (*mdbUnregisterReadInfo)(struct mdbreadinforegstruct *r);
	void (*fsTypeUnregister) (struct moduletype modtype);

	void (*filesystem_setup_unregister_file) (struct ocpfile_t *file);
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
# ifdef __APPLE__
#  define DLLEXTINFO_BEGIN_PREFIX const __attribute__ ((section ("__DATA,plugin_list"))) __attribute__ ((used))
#  define DLLEXTINFO_END_PREFIX   static const __attribute__ ((section ("__DATA,plugin_list"))) __attribute__ ((used))
# else
#  define DLLEXTINFO_BEGIN_PREFIX const __attribute__ ((section ("plugin_list"))) __attribute__ ((used))
#  define DLLEXTINFO_END_PREFIX   static const __attribute__ ((section ("plugin_list"))) __attribute__ ((used))
# endif
#else
# define DLLEXTINFO_BEGIN_PREFIX
# define DLLEXTINFO_END_PREFIX
#endif

#ifdef STATIC_CORE
# ifdef __APPLE__
#  define DLLEXTINFO_CORE_PREFIX  static const __attribute__ ((section ("__DATA,plugin_list"))) __attribute__ ((used))
# else
#  define DLLEXTINFO_CORE_PREFIX  static const __attribute__ ((section ("plugin_list"))) __attribute__ ((used))
# endif
#else
# define DLLEXTINFO_CORE_PREFIX const
#endif

#define DLLEXTINFO_DRIVER_PREFIX       const
#define DLLEXTINFO_PLAYBACK_PREFIX     const
#define DLLEXTINFO_PLAYBACK_PREFIX_CPP /* C++, const implies static */

#endif
