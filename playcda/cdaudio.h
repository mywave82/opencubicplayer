#ifndef _CDAUDIO_H
#define _CDAUDIO_H

struct ocpfilehandle_t;

struct cdStat
{
	int error;  /* boolean */
	int looped; /* boolean */
	uint32_t position;
};

struct cpifaceSessionAPI_t;
OCP_INTERNAL void cdClose (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void cdJump (struct cpifaceSessionAPI_t *cpifaceSession, unsigned long start);
OCP_INTERNAL int cdOpen (unsigned long start, unsigned long len, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void cdGetStatus (struct cdStat *stat);
OCP_INTERNAL void cdSetLoop (int loop);
OCP_INTERNAL void cdIdle (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
