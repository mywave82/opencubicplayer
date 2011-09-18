#ifndef _GENDIR_HELPERS_H
#define _GENDIR_HELPERS_H

extern void gendir(const char *orgdir, const char *fixdir, char *targetdir); /* ALL dirs must be PATH_MAX+1 */
extern void genreldir(const char *orgdir, const char *fixdir, char *targetdir); /* ALL dirs must be PATH_MAX+1 */

#endif
