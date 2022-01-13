#ifndef MUSICBRAINZ_H
#define MUSICBRAINZ_H 1

#include "mdb.h"

struct musicbrainz_database_h
{
	char album[MDB_COMPOSER_LEN]; /* common for the entire CD */
	uint32_t date[100];
	char title[100][MDB_TITLE_LEN];
	char artist[100][MDB_ARTIST_LEN];
};

void *musicbrainz_lookup_discid_init (const char *discid, struct musicbrainz_database_h **result);

int musicbrainz_lookup_discid_iterate (void *token, struct musicbrainz_database_h **result);

void musicbrainz_lookup_discid_cancel (void *token);

void musicbrainz_database_h_free (struct musicbrainz_database_h *);

int musicbrainz_init (void);

void musicbrainz_done (void);

#endif
