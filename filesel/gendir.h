#ifndef _GENDIR_HELPERS_H
#define _GENDIR_HELPERS_H

int gendir_malloc (const char *basepath, const char *relpath, char **resultpath);

int genreldir_malloc(const char *basepath, const char *targetpath, char **relpath);

#endif
