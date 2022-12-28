#ifndef DEVWMIXF__H
#define DEVWMIXF__H

struct cpifaceSessionAPI_t;

struct mixfpostprocregstruct
{
	void (*Process) (struct cpifaceSessionAPI_t *cpifaceSession, float *buffer, int len, int rate);
	void (*Init) (int rate);
	void (*Close) (void);
	const struct ocpvolregstruct *VolRegs;
	int (*ProcessKey) (uint16_t key);
	struct mixfpostprocregstruct *next;
};

#endif
