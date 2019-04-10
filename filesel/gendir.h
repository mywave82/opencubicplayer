#ifndef _GENDIR_HELPERS_H
#define _GENDIR_HELPERS_H

/* TO BE REMOVED */ void gendir(const char *orgdir, const char *fixdir, char *targetdir);
/* TO BE REMOVED */ void genreldir(const char *orgdir, const char *fixdir, char *targetdir);


int gendir_malloc (const char *basepath, const char *relpath, char **resultpath);

int genreldir_malloc(const char *basepath, const char *targetpath, char **relpath);

#endif
