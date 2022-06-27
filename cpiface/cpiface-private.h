#ifndef _CPIFACE_PRIVATE_H
#define _CPIFACE_PRIVATE_H 1

struct cpifaceSessionPrivate_t
{
	struct cpifaceSessionAPI_t Public;

	/* instrument visualizer */
	struct insdisplaystruct Inst;
	int   InstScroll;
	int   InstFirstLine;
	int   InstStartCol;
	int   InstLength;
	int   InstHeight;
	int   InstWidth;
	char  InstType;
	char  InstMode;
};

extern __attribute__ ((visibility ("internal"))) struct cpifaceSessionPrivate_t cpifaceSessionAPI;

#endif
