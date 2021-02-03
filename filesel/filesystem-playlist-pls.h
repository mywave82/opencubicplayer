#ifndef _FILESYSTEM_PLAYLIST_PLS_H
#define _FILESYSTEM_PLAYLIST_PLS_H 1

struct ocpdir_t *pls_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype);

void filesystem_pls_register (void);

#endif
