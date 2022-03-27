#ifndef DEVWMIXF__H
#define DEVWMIXF__H

struct mixfpostprocregstruct
{
	void (*Process)(float *buffer, int len, int rate);
	void (*Init)(int rate);
	void (*Close)(void);
	struct mixfpostprocregstruct *next;
};

extern void mixfRegisterPostProc(struct mixfpostprocregstruct *);

struct mixfpostprocaddregstruct
{
	int (*ProcessKey)(uint16_t key);
	struct mixfpostprocaddregstruct *next;
};

#endif
