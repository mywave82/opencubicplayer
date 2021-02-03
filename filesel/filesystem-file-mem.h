#ifndef _FILESEL_FILESYSTEM_FILE_MEM_H
#define _FILESEL_FILESYSTEM_FILE_MEM_H 1

struct ocpdir_t;
struct ocpfile_t;
struct ocpfilehandle_t;

/* Memory-mapped ocpfilehandle_t */

/* takes ownership of memory */
struct ocpfilehandle_t *mem_filehandle_open (int dirdb_ref, char *ptr, uint32_t len);

/* takes ownership of memory */
struct ocpfile_t *mem_file_open (struct ocpdir_t *parent, int dirdb_ref, char *ptr, uint32_t len);


#endif
