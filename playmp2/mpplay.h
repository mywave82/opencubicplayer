#ifndef _MPPLAY_H
#define _MPPLAY_H

struct mpeginfo
{
	uint32_t pos;
	uint32_t len;
	uint32_t timelen;
	uint32_t rate;
	int	 stereo;
	int      bit16;
	const char *opt25;
	const char *opt50;
};

struct ID3_t;
struct ocpfilehandle_t;

struct cpifaceSessionAPI_t;
extern OCP_INTERNAL int mpeg_Bitrate; /* bitrate of the last decoded frame */
OCP_INTERNAL int mpegOpenPlayer (struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void mpegClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void mpegIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void mpegSetLoop (uint8_t s);
OCP_INTERNAL char mpegIsLooped (void);
OCP_INTERNAL void mpegPause (uint8_t p);
OCP_INTERNAL void mpegGetInfo (struct mpeginfo *);
OCP_INTERNAL uint32_t mpegGetPos (void);
OCP_INTERNAL void mpegSetPos (uint32_t pos);
OCP_INTERNAL void mpegGetID3 (struct ID3_t **ID3);
OCP_INTERNAL void ID3InfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void ID3InfoDone (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void ID3PicInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void ID3PicDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
