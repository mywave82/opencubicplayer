#ifndef _CPIFACE_PRIVATE_H
#define _CPIFACE_PRIVATE_H 1

struct cpifaceSessionPrivate_t
{
	struct cpifaceSessionAPI_t Public;
	struct insdisplaystruct    InsDisplay;
};

extern __attribute__ ((visibility ("internal"))) struct cpifaceSessionPrivate_t cpifaceSessionAPI;

#endif
