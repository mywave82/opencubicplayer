#ifndef _FILESEL_FILESYSTEM_DEV_H
#define _FILESEL_FILESYSTEM_DEV_H 1

#define IOCTL_DEVINTERFACE "DevInterface"

#include "filesel/pfilesel.h" // interfaceReturnEnum

struct configAPI_t;
struct dirdbAPI_t;
struct dmDrive;
struct ocpdir_t;
struct ocpfile_t;
struct ocpfilehandle_t;
struct moduleinfostruct;
struct PipeProcessAPI_t;

struct DevInterfaceAPI_t
{
	const struct configAPI_t      *configAPI;
	const struct dirdbAPI_t       *dirdb;
	      struct console_t        *console;
	const struct PipeProcessAPI_t *PipeProcess;
#ifdef _WIN32
        const char *           const  dmLastActiveDriveLetter;
              struct dmDrive * const *dmDriveLetters;
#else
	const struct dmDrive         *dmFile;
#endif

	void (*KeyHelp) (uint16_t key, const char *shorthelp); /* Called on ALT-K to issue help about each keyboard shortcut */
	void (*KeyHelpClear) (void); /* Clears the current keyboard shortcut list, only used by keyboard/display loops */
	int  (*KeyHelpDisplay) (void); /* Draws the keyboard shortcut list and polls keyboard. Call for each draw-iteration until it returns zero */

	void (*fsDraw) (void); /* Draws the filesystem browser, great for virtual devices that has a dialog */
	void (*fsForceNextRescan) (void); /* Next time fsFileSelect() is oalled, enforce a rescan */

	int (*filesystem_resolve_dirdb_dir) (uint32_t ref, struct dmDrive **drive, struct ocpdir_t **dir);
	int (*filesystem_resolve_dirdb_file) (uint32_t ref, struct dmDrive **drive, struct ocpfile_t **file);

#ifdef _WIN32
	char * (*utf16_to_utf8) (const uint16_t *src);
#endif
};

struct IOCTL_DevInterface
{
	int                 (*Init)  (struct IOCTL_DevInterface *self, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API);
	interfaceReturnEnum (*Run)   (struct IOCTL_DevInterface *self,                                const struct DevInterfaceAPI_t *API);
	void                (*Close) (struct IOCTL_DevInterface *self,                                const struct DevInterfaceAPI_t *API);
};

struct ocpfile_t *dev_file_create
(
	struct ocpdir_t *parent,
	const char *devname,
	const char *mdbtitle,
	const char *mdbcomposer,
	void *token,
	int  (*Init)       (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API), // Client can change token for instance, it defaults to the provided one
	void (*Run)        (void **token,                                const struct DevInterfaceAPI_t *API), // Client can change token for instance
	void (*Close)      (void **token,                                const struct DevInterfaceAPI_t *API), // Client can change token for instance
	void (*Destructor) (void  *token)
);

#endif
