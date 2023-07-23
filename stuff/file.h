#ifndef _STUFF_FILE_H
#define _STUFF_FILE_H

/* This is for making a single code-path for data-base read-out of data. It has read-ahead-cache */
typedef struct osfile_t osfile;

struct osfile_t *osfile_open_readwrite (const char *pathname, int dolock, int mustcreate); /* returns NULL on error */

void osfile_close (struct osfile_t *f);

uint64_t osfile_getpos (struct osfile_t *f);

void osfile_setpos (struct osfile_t *f, uint64_t pos);

void osfile_truncate_at (struct osfile_t *f, uint64_t pos);

void osfile_purge_readaheadcache (struct osfile_t *f);

int64_t osfile_write (struct osfile_t *f, const void *data, uint64_t size); /* returns < 0 on error (also for partial writes) */

int64_t osfile_read (struct osfile_t *f, void *data, uint64_t size); /* returns < 0 on error, can return partial data if hitting EOF */

#endif
