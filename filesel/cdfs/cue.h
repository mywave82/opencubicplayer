#ifndef _CUE_H
#define _CUE_H 1

#include "filesel/filesystem.h"

struct cue_parser_t;
struct cdfs_disc_t;

void cue_parser_free (struct cue_parser_t *cue_parser);

struct cue_parser_t *cue_parser_from_data (const char *input);

struct cdfs_disc_t *cue_parser_to_cdfs_disc (struct ocpfile_t *file, struct cue_parser_t *cue_parser);

#endif
