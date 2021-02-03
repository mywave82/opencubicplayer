#ifndef _FILESEL_FILESYSTEM_DIR_MEM_H
#define _FILESEL_FILESYSTEM_DIR_MEM_H 1

/* Memory-mapped "static" dir_t */

struct ocpdir_mem_t; /* private */
struct ocpdir_t;
struct ocpfile_t;

/* parent will be ref'ed if non-null */
struct ocpdir_mem_t *ocpdir_mem_alloc (struct ocpdir_t *parent, const char *name);

void ocpdir_mem_add_dir (struct ocpdir_mem_t *, struct ocpdir_t *child);
void ocpdir_mem_remove_dir (struct ocpdir_mem_t *, struct ocpdir_t *child);

void ocpdir_mem_add_file (struct ocpdir_mem_t *, struct ocpfile_t *child);
void ocpdir_mem_remove_file (struct ocpdir_mem_t *, struct ocpfile_t *child);

struct ocpdir_t *ocpdir_mem_getdir_t (struct ocpdir_mem_t *);

#endif
