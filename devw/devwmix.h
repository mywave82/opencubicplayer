#ifndef DEVWMIX__H
#define DEVWMIX__H

struct mixqpostprocregstruct
{
	void (*Process)(int32_t *buffer, int len, int rate, int stereo);
	void (*Init)(int rate, int stereo);
	void (*Close)(void );
	struct mixqpostprocregstruct *next;
};

void mixqRegisterPostProc(struct mixqpostprocregstruct *);

struct mixqpostprocaddregstruct
{
	int (*ProcessKey)(unsigned short key);
	struct mixqpostprocaddregstruct *next;
};

#endif
