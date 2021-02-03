#ifndef _FILESYSTEM_SETUP_H
#define _FILESYSTEM_SETUP_H 1

extern struct dmDrive *dmSetup;

void filesystem_setup_register (void);

void filesystem_setup_register_dir (struct ocpdir_t *dir);
void filesystem_setup_unregister_dir (struct ocpdir_t *dir);
void filesystem_setup_register_file (struct ocpfile_t *file);
void filesystem_setup_unregister_file (struct ocpfile_t *file);

#endif
