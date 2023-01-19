#ifndef _FILESYSTEM_ANCIENT_H
#define _FILESYSTEM_ANCIENT_H 1

struct ocpfilehandle_t;


/* compressionmethod + compressionmethod_len: if provided, the compression method detected will be filled in here
 * s: te source filehandle
 *
 * returns a new filehandle refers to a if a compatiable and intact compressed file
 */
struct ocpfilehandle_t *ancient_filehandle (char *compressionmethod, int compressionmethod_len, struct ocpfilehandle_t *s);

#endif
