#ifndef _STUFF_FILE_H
#define _STUFF_FILE_H

/* This is for making a single code-path for data-base read-out of data. It has read-ahead-cache */
typedef struct osfile_t osfile;

struct osfile_t *osfile_open_readwrite (const char *pathname, int dolock, int mustcreate); /* returns NULL on error */

struct osfile_t *osfile_open_readonly (const char *pathname, int dolock); /* returns NULL on error */

void osfile_close (struct osfile_t *f);

uint64_t osfile_getfilesize (struct osfile_t *f);

uint64_t osfile_getpos (struct osfile_t *f);

void osfile_setpos (struct osfile_t *f, uint64_t pos);

void osfile_truncate_at (struct osfile_t *f, uint64_t pos);

void osfile_purge_readahead_cache (struct osfile_t *f);
int64_t osfile_purge_writeback_cache (struct osfile_t *f);

int64_t osfile_write (struct osfile_t *f, const void *data, uint64_t size); /* returns < 0 on error (also for partial writes) */

int64_t osfile_read (struct osfile_t *f, void *data, uint64_t size); /* returns < 0 on error, can return partial data if hitting EOF */

struct osdir_size_t
{
	uint_fast32_t  directories_n;
	uint_fast32_t  files_n;
	uint64_t       files_size;
	void          *internal;
};
int osdir_size_start (struct osdir_size_t *, const char *path); /* returns -1 on error, otherwize 0 */
int osdir_size_iterate (struct osdir_size_t *);                 /* returns 1 if more iterations are needed, otherwize 0 */
void osdir_size_cancel (struct osdir_size_t *);

int osdir_trash_available (const char *path);
int osdir_trash_perform (const char *path); /* -1 on error, 0 on OK */

struct osdir_delete_t
{
	uint_fast32_t  removed_directories_n;
	uint_fast32_t  removed_files_n;
	uint_fast32_t  failed_directories_n;
	uint_fast32_t  failed_files_n;
	void          *internal;
};
int osdir_delete_start   (struct osdir_delete_t *, const char *path); /* returns -1 on error, otherwize 0 */
int osdir_delete_iterate (struct osdir_delete_t *);                   /* returns 1 if more iterations are needed, otherwize 0 */
void osdir_delete_cancel (struct osdir_delete_t *);

#endif
