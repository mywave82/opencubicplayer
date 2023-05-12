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

/* toc needs to be string in this format: "1 18 284700 150 15862 33817 51905 71767 88997 102517 118750 137782 154112 158765 175650 182765 202162 217580 239225 248915 268307" */
void *musicbrainz_lookup_discid_init (const char *discid, const char *toc, struct musicbrainz_database_h **result);

int musicbrainz_lookup_discid_iterate (void *token, struct musicbrainz_database_h **result);

void musicbrainz_lookup_discid_cancel (void *token);

void musicbrainz_database_h_free (struct musicbrainz_database_h *);

struct configAPI_t;
int musicbrainz_init (const struct configAPI_t *configAPI);

void musicbrainz_done (void);

#endif
