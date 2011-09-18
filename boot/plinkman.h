#ifndef _PLINKMAN_H
#define _PLINKMAN_H 1

struct __attribute__ ((aligned (64))) linkinfostruct
{
	const char *name;
	const char *desc;
	uint32_t ver;
	uint32_t size;

	int (*PreInit)(void); /* high priority init */
	int (*Init)(void);
	int (*LateInit)(void);
	void (*PreClose)(void);
	void (*Close)(void);
	void (*LateClose)(void); /* low priority Close */
};
#define LINKINFOSTRUCT_NOEVENTS ,0,0,0,0,0,0

struct dll_handle
{
	void *handle;
	int id;
	struct linkinfostruct *info;
};
extern struct dll_handle loadlist[MAXDLLLIST];
extern int loadlist_n;

extern int lnkLinkDir(const char *dir);
extern int lnkLink(const char *files);
extern void lnkFree(const int id);
extern void lnkInit(void);
#define _lnkGetSymbol(name) lnkGetSymbol(0, name)
extern void *lnkGetSymbol(const int id, const char *name);
extern char *_lnkReadInfoReg(const char *key);
extern char *lnkReadInfoReg(const int id, const char *key);
extern int lnkCountLinks(void);
extern int lnkGetLinkInfo(struct linkinfostruct *l, int index);

#ifdef SUPPORT_STATIC_PLUGINS
#define DLLEXTINFO_PREFIX __attribute__ ((section ("plugin_list"))) __attribute__ ((used)) static
#else
#define DLLEXTINFO_PREFIX
#endif

#endif
