#ifndef __OGG_H
#define __OGG_H

/* ogg_int64_t */
#include <vorbis/vorbisfile.h>

struct ogginfo
{
	ogg_int64_t pos;
	ogg_int64_t len;
	uint32_t rate;
	uint8_t stereo;
	uint8_t bit16;
	int bitrate;
	const char *opt25;
	const char *opt50;
};

struct ogg_comment_t
{
	char *title;
	int value_count;
	char *value[];
};

struct ogg_picture_t
{
	uint32_t picture_type;
	char *description;

	uint16_t width;
	uint16_t height;
	uint8_t *data_bgra;

	uint16_t  scaled_width;
	uint16_t  scaled_height;
	uint8_t  *scaled_data_bgra;
};

extern OCP_INTERNAL struct ogg_comment_t  **ogg_comments;
extern OCP_INTERNAL int                     ogg_comments_count;
extern OCP_INTERNAL struct ogg_picture_t   *ogg_pictures;
extern OCP_INTERNAL int                     ogg_pictures_count;

struct ocpfilehandle_t;
struct cpifaceSessionAPI_t;
OCP_INTERNAL int oggOpenPlayer (struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void oggClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void oggIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void oggSetLoop (uint8_t s);
OCP_INTERNAL char oggLooped (void);
OCP_INTERNAL void oggPause (uint8_t p);
OCP_INTERNAL void oggGetInfo (struct cpifaceSessionAPI_t *cpifaceSession, struct ogginfo *);
OCP_INTERNAL ogg_int64_t oggGetPos (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void oggSetPos (struct cpifaceSessionAPI_t *cpifaceSession, ogg_int64_t pos);

OCP_INTERNAL void OggInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void OggInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void OggPicInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void OggPicDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
