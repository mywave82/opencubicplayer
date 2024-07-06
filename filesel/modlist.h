#ifndef _DIRLIST_H
#define _DIRLIST_H

struct ocpdir_t;
struct ocpfile_t;

struct modlistentry
{
	char utf8_8_dot_3  [12*4+1];  /* UTF-8 ready */
	char utf8_16_dot_3 [20*4+1]; /* UTF-8 ready */

#define MODLIST_FLAG_DRV     1
#define MODLIST_FLAG_DOTDOT  2
#define MODLIST_FLAG_SCANNED 4
#define MODLIST_FLAG_ISMOD   8
	int flags;

	uint32_t mdb_ref;
	struct ocpdir_t *dir;
	struct ocpfile_t *file;
};

struct modlist
{
	int                 *sortindex; /* presented order, and most API works on this list */
	struct modlistentry *files;

	/* these are used by external */
	unsigned int pos; /* position - will be zero if num is zero */

	unsigned int max; /* current array size */
	unsigned int num; /* entries used */
};

struct dmDrive;

struct modlist *modlist_create(void);
void modlist_free(struct modlist *modlist);
void modlist_sort(struct modlist *modlist);
void modlist_subsort_filesonly_groupdir (struct modlist *modlist, unsigned int pos, unsigned int length); /* sorts a slice of the list */
void modlist_append(struct modlist *modlist, struct modlistentry *entry);
void modlist_append_dir (struct modlist *modlist, struct ocpdir_t *dir);
void modlist_append_dotdot (struct modlist *modlist, struct ocpdir_t *dir);
void modlist_append_drive (struct modlist *modlist, struct dmDrive *drive);
void modlist_append_file (struct modlist *modlist, struct ocpfile_t *file, int ismod, int prescanhint); /* if file is stored SOLID, trigger a mdb scan if needed */

void modlist_swap(struct modlist *modlist, unsigned int index1, unsigned int index2);
void modlist_clear(struct modlist *modlist);
void modlist_remove(struct modlist *modlist, unsigned int index); /* by sortindex */
void modlist_remove_all_by_path(struct modlist *modlist, uint32_t ref);
int modlist_find(struct modlist *modlist, const uint32_t path);
int modlist_fuzzyfind(struct modlist *modlist, const char *filename);
struct modlistentry *modlist_get(const struct modlist *modlist, unsigned int index);
struct modlistentry *modlist_getcur(const struct modlist *modlist); /* does not Ref() */
void modlist_append_modlist(struct modlist *target, struct modlist *source);

#endif
