#ifndef _FILESEL_FILESYSTEM_FILEHANDLE_CACHE_H
#define _FILESEL_FILESYSTEM_FILEHANDLE_CACHE_H 1

struct ocpdir_t;
struct ocpfile_t;
struct ocpfilehandle_t;

/* Common caching code for ocpfilehandle_t */

/* owner is so we can open the real file if pre-section is not enough, we take ownership of the headptr and tailptr... head and/or tail can be NULL if their len is 0 */
struct ocpfilehandle_t *cache_filehandle_open_pre (struct ocpfile_t *owner, char *headptr, uint32_t headlen, char *tailptr, uint32_t taillen);

/* for general cached version, we go directly for an open handle */
struct ocpfilehandle_t *cache_filehandle_open (struct ocpfilehandle_t *parent);

#endif
