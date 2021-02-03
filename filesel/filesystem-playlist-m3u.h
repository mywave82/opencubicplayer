#ifndef _FILESYSTEM_PLAYLIST_M3U_H
#define _FILESYSTEM_PLAYLIST_M3U_H 1

struct ocpdir_t *m3u_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype);

void filesystem_m3u_register (void);

#endif
