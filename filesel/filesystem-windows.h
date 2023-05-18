#ifndef _FILESEL_FILESYSTEM_WINDOWS_H
#define _FILESEL_FILESYSTEM_WINDOWS_H 1

# ifdef _WIN32

extern struct dmDrive *dmDriveLetters[26];
extern char            dmLastActiveDriveLetter; /* A-Z, or NULL - last active drive, updated by pfilesel.c */

struct dmDrive *filesystem_windows_init (void); /* returns CWD-drive */
void filesystem_windows_done (void);

# endif

#endif
