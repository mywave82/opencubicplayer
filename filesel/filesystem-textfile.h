#ifndef _FILESEL_FILESYSTEM_TEXTFILE_H
#define _FILESEL_FILESYSTEM_TEXTFILE_H

struct textfile_t;
struct ocpfilehandle_t;

struct textfile_t *textfile_start (struct ocpfilehandle_t *filehandle);
void textfile_stop (struct textfile_t *self);
const char *textfile_fgets (struct textfile_t *self);

#endif
