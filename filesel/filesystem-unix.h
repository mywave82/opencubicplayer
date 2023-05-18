#ifndef _FILESEL_FILESYSTEM_UNIX_H
#define _FILESEL_FILESYSTEM_UNIX_H 1

#ifndef _WIN32

struct ocpdir_t;
struct ocpfile_t;
struct ocpfilehandle_t;

struct dmDrive;

extern struct dmDrive *dmFile;

struct ocpdir_t *file_unix_root (void);

int filesystem_unix_init (void);
void filesystem_unix_done (void);

#endif

#endif
