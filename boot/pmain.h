#ifndef __PMAIN_H
#define __PMAIN_H

struct mainstruct
{
	int (*main)(int argc, char *argv[]);
};

extern struct mainstruct *ocpmain;

#endif
