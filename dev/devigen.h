#ifndef _DEVIGEN_H
#define _DEVIGEN_H

#include "imsdev.h"

struct devinfonode
{
	struct devinfonode *next;
	char handle[9];
	struct deviceinfo devinfo;
	char name[32];
	char ihandle;
	char keep;
	int linkhand;
};

int deviReadDevices(const char *list, struct devinfonode **devs);

#endif
