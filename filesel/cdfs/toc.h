#ifndef _TOC_H
#define _TOC_H 1

#include "filesel/filesystem.h"

struct toc_parser_t;
struct cdfs_disc_t;

void toc_parser_free (struct toc_parser_t *toc_parser);

struct toc_parser_t *toc_parser_from_data (const char *input /* null terminated */);

struct cdfs_disc_t *toc_parser_to_cdfs_disc (struct ocpfile_t *file, struct toc_parser_t *toc_parser);

#endif
