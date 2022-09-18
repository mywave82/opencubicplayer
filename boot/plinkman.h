#ifndef _PLINKMAN_H
#define _PLINKMAN_H 1

struct __attribute__ ((aligned (64))) linkinfostruct
{
	const char *name;
	const char *desc;
	uint32_t ver;

	int (*PreInit)(void); /* high priority init */
	int (*Init)(void);
	int (*LateInit)(void);
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
	off_t size;
	const struct linkinfostruct *info;
};
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
extern int lnkGetLinkInfo(struct linkinfostruct *l, off_t *size, int index);

int lnkInitAll (void);
void lnkCloseAll (void);

#ifdef SUPPORT_STATIC_PLUGINS
#define DLLEXTINFO_PREFIX __attribute__ ((section ("plugin_list"))) __attribute__ ((used)) static
#else
#define DLLEXTINFO_PREFIX
#endif

#endif
