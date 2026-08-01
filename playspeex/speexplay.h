#ifndef _SPEEXPLAY_H
#define _SPEEXPLAY_H

struct cpifaceSessionAPI_t;
struct ocpfilehandle_t;

struct speexinfo
{
	uint64_t filepos;
	uint64_t filelen;
	uint32_t rate;
	int bitrate;
	const char *opt25;
	const char *opt50;
};


struct speex_comment_t
{
	char *title;
	int value_count;
	char *value[];
};

extern OCP_INTERNAL struct speex_comment_t  **speex_comments;
extern OCP_INTERNAL int                       speex_comments_count;

OCP_INTERNAL void speexIdle (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL char speexLooped (void);

OCP_INTERNAL void speexSetLoop (uint8_t s);

OCP_INTERNAL int speexOpenPlayer (struct ocpfilehandle_t *_fh, struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void speexClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void speexInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void speexInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void speexGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct speexinfo *);

OCP_INTERNAL void speexSeekHome (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void speexSeekReverse (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length);
OCP_INTERNAL void speexSeekForward (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length);

#endif
