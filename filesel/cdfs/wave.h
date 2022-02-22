#ifndef _WAVE_H
#define _WAVE_H 1

#include <stdint.h>

int wave_filename(const char *filename);

#warning this operation should be redesigned for iterations
int wave_openfile (struct ocpdir_t *dir, const char *filename, struct ocpfile_t **file, struct ocpfilehandle_t **handle, uint64_t *offset, uint64_t *length);

#warning this operation should be redesigned for iterations
int data_openfile (struct ocpdir_t *dir, const char *filename, struct ocpfile_t **file, struct ocpfilehandle_t **handle, uint64_t *length);

#endif
