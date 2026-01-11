#ifndef __PLAYQOA_H
#define __PLAYQOA_H

struct qoainfo
{
	unsigned long pos;
	unsigned long len;
	uint32_t rate;
	int channels;
	int bitrate;
	const char *opt25;
	const char *opt50;	
};

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;

OCP_INTERNAL int qoaOpenPlayer (struct ocpfilehandle_t *fp, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void qoaClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void qoaIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void qoaSetLoop (unsigned char s);
OCP_INTERNAL char qoaIsLooped (void);
OCP_INTERNAL void qoaGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct qoainfo *);
#if 0
OCP_INTERNAL uint32_t qoaGetPos (struct cpifaceSessionAPI_t *cpifaceSession);
#endif
OCP_INTERNAL void qoaSetPos (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t pos);

#endif
