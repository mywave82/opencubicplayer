#ifndef _WAVE_H
#define _WAVE_H 1

#include <stdint.h>

int wave_filename(const char *filename);

#warning this operation should be redesigned for iterations
struct cdfs_datasource_handle_t *wave_openfile (struct ocpdir_t *dir, const char *filename);

#warning this operation should be redesigned for iterations
struct cdfs_datasource_handle_t *data_openfile (struct ocpdir_t *dir, const char *filename);

struct cdfs_datasource_handle_t *spawn_audiofile_handle_plain (struct ocpfilehandle_t *fh, uint64_t offset, uint64_t length);

#endif
