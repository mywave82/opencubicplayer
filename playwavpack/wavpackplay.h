#ifndef _PLAYWAVPACK_WAVPACKPLAY_H
#define _PLAYWAVPACK_WAVPACKPLAY_H

#include <stdint.h>

struct wavpackinfo
{
	uint64_t pos;
	uint64_t len;
	uint32_t rate;
	uint8_t stereo;
	uint8_t bit16;
	int bitrate;
	const char *opt25;
	const char *opt50;
};

struct wavpack_comment_t
{
	char *title;
	int value_count;
	char **value;
};

struct wavpack_picture_t
{
	char *title;
	char *filename;

	uint16_t width;
	uint16_t height;
	const uint8_t *data_bgra; /* follows filename in the data-stream */

	uint16_t  scaled_width;
	uint16_t  scaled_height;
	uint8_t  *scaled_data_bgra;
};


extern OCP_INTERNAL struct wavpack_comment_t *wavpack_comments;
extern OCP_INTERNAL int                       wavpack_comments_count;
extern OCP_INTERNAL struct wavpack_picture_t *wavpack_pictures;
extern OCP_INTERNAL int                       wavpack_pictures_count;

OCP_INTERNAL int wavpackOpenPlayer (struct ocpfilehandle_t *fh, struct ocpfilehandle_t *fh_wvc, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wavpackClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wavpackIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wavpackSetLoop (int s);
OCP_INTERNAL int wavpackLooped (void);
OCP_INTERNAL void wavpackGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct wavpackinfo *);
OCP_INTERNAL void wavpackSetPos (struct cpifaceSessionAPI_t *cpifaceSession, uint64_t newpos);

OCP_INTERNAL void wavpackInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wavpackInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void wavpackPicInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void wavpackPicDone (struct cpifaceSessionAPI_t *cpifaceSession);


#endif
