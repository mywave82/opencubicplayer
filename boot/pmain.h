#ifndef __PMAIN_H
#define __PMAIN_H

struct bootupstruct
{
	int (*main)(int argc, char *argv[], const char *ConfigDir, const char *DataDir, const char *ProgramDir);
};
struct mainstruct
{
	int (*main)(int argc, char *argv[]);
};

extern struct mainstruct *ocpmain;

#endif
