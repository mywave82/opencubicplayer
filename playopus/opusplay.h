#ifndef _OPUSPLAY_H
#define _OPUSPLAY_H

struct cpifaceSessionAPI_t;
struct ocpfilehandle_t;

struct opusinfo
{
	uint64_t filepos;
	uint64_t filelen;
	uint32_t rate;
	int bitrate;
	const char *opt25;
	const char *opt50;
};


struct opus_comment_t
{
	char *title;
	int value_count;
	char *value[];
};

extern OCP_INTERNAL struct opus_comment_t  **opus_comments;
extern OCP_INTERNAL int                      opus_comments_count;

OCP_INTERNAL void opusIdle (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL char opusLooped (void);

OCP_INTERNAL void opusSetLoop (uint8_t s);

OCP_INTERNAL int opusOpenPlayer (struct ocpfilehandle_t *_fh, struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void opusClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void opusInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void opusInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void opusGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct opusinfo *);

OCP_INTERNAL void opusSeekHome (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void opusSeekReverse (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length);
OCP_INTERNAL void opusSeekForward (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int length);

#endif
