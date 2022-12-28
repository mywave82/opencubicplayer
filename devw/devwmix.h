#ifndef DEVWMIX__H
#define DEVWMIX__H

struct cpifaceSessionAPI_t;

struct mixqpostprocregstruct
{
	void (*Process) (struct cpifaceSessionAPI_t *cpifaceSession, int32_t *buffer, int len, int rate);
	void (*Init) (int rate);
	void (*Close) (void);
	const struct ocpvolregstruct *VolRegs;
	int (*ProcessKey) (uint16_t key);
	struct mixqpostprocregstruct *next;
};

#endif
