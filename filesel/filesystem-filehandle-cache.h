#ifndef _FILESEL_FILESYSTEM_FILEHANDLE_CACHE_H
#define _FILESEL_FILESYSTEM_FILEHANDLE_CACHE_H 1

struct ocpdir_t;
struct ocpfile_t;
struct ocpfilehandle_t;

/* Common caching code for ocpfilehandle_t */

/* for general cached version, we go directly for an open handle */
struct ocpfilehandle_t *cache_filehandle_open (struct ocpfilehandle_t *parent);

#endif
