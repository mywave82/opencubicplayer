#ifndef _DIRLIST_H
#define _DIRLIST_H

#include "config.h"
#if 0
#include <dirent.h>
#include <sys/types.h>
#endif
#include <stdio.h> /* size_t */

struct dmDrive;
struct modlistentry
{
	char shortname[12]; /* the name that we present in the filelists.. 8:3 format looks so nice... yuck... but i works*/
	const struct dmDrive *drive;
	uint32_t dirdbfullpath; /* full path */

#define MODLIST_FLAG_DIR      1
#define MODLIST_FLAG_ARC      2 /* mutual exlusive flags, but we still let them have bit values.. looks nice */
#define MODLIST_FLAG_FILE     4
#define MODLIST_FLAG_VIRTUAL  8
#define MODLIST_FLAG_DRV      16
	int flags;
	uint32_t mdb_ref;
	/* uint32_t dirref; */
	uint32_t adb_ref; /* new, until dirref is re-created later perhaps */

	int (*Read)(struct modlistentry *entry, char **mem, size_t *size);
	int (*ReadHeader)(struct modlistentry *entry, char *mem, size_t *size); /* size is prefilled with max data, and mem is preset*/
	FILE *(*ReadHandle)(struct modlistentry *entry);
};

struct modlist
{
	struct modlistentry **files;
	struct modlistentry **realfiles;

	/* these are used by external */
	unsigned int pos; /* position */

	unsigned int max; /* current array size */
	unsigned int num; /* entries used */
};

extern struct modlist *modlist_create(void);
extern void modlist_free(struct modlist *modlist);
extern void modlist_sort(struct modlist *modlist);
extern void modlist_append(struct modlist *modlist, struct modlistentry *entry);
extern void modlist_swap(struct modlist *modlist, unsigned int index1, unsigned int index2);
extern void modlist_remove(struct modlist *modlist, unsigned int index, unsigned int count);
extern void modlist_remove_all_by_path(struct modlist *modlist, uint32_t ref);
extern int modlist_find(struct modlist *modlist, const uint32_t path);
extern int modlist_fuzzyfind(struct modlist *modlist, const char *filename);
extern struct modlistentry *modlist_get(const struct modlist *modlist, unsigned int index);
extern struct modlistentry *modlist_getcur(const struct modlist *modlist); /* does not Ref() */
extern void modlist_append_modlist(struct modlist *target, struct modlist *source);

extern void fs12name(char *dst13bytes, const char *source);

#endif
