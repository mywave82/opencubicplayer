#ifndef FILESYSTEM_PLAYLIST_H
#define FILESYSTEM_PLAYLIST_H 1

struct ocpdir_t;
struct ocpfile_t;
struct playlist_instance_t;

struct playlist_string_entry_t
{
	char *string;
	int flags; /* can contain DIRDB_RESOLVE_DRIVE DIRDB_RESOLVE_TILDE_HOME */
};

struct playlist_instance_t
{
	struct ocpdir_t head;
	struct playlist_instance_t *next;

	struct playlist_string_entry_t *string_data;
	int string_count;
	int string_size;
	int string_pos; /* increased by iterate until it matches count, then it resets and clears */

	struct ocpfile_t **ocpfile_data;
	int ocpfile_count;
	int ocpfile_size;
};

extern struct playlist_instance_t *playlist_root;

struct playlist_instance_t *playlist_instance_allocate (struct ocpdir_t *parent, uint32_t dirdb_ref);

/* steals the string */
void playlist_add_string (struct playlist_instance_t *self, char *string, const int flags);


#endif
