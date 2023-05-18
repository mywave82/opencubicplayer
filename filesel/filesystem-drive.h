#ifndef _FILESYSTEM_DRIVE_H
#define _FILESYSTEM_DRIVE_H 1

struct ocpdir_t;

/* basedir->dirdb_ref is expected to be dirdbFindAndRef(DIRDB_NOPARENT, dmDrive) */
struct dmDrive
{
	char drivename[13];
	struct ocpdir_t *basedir;
	struct ocpdir_t *cwd;
	struct dmDrive *next;
};
extern struct dmDrive *dmDrives;

struct dmDrive *RegisterDrive(const char *dmDrive, struct ocpdir_t *basedir, struct ocpdir_t *cwd);
void UnregisterDrive(struct dmDrive *drive);
struct dmDrive *dmFindDrive(const char *dmDrive); /* to get the correct drive from a given string */

void filesystem_drive_init (void);
void filesystem_drive_done (void);

int filesystem_resolve_dirdb_dir (uint32_t ref, struct dmDrive **drive, struct ocpdir_t **dir);
int filesystem_resolve_dirdb_file (uint32_t ref, struct dmDrive **drive, struct ocpfile_t **file);

struct dmDrive *ocpdir_get_drive (struct ocpdir_t *dir);

#endif
