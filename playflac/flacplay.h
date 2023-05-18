#ifndef _FLACPLAY_H
#define _FLACPLAY_H

struct flacinfo
{
	uint64_t pos; /* in source samples */
	uint64_t len; /* in source samples */
	uint32_t timelen; /* in seconds ETA */
	uint32_t rate; /* output rate */
	int	 stereo;
	int      bits;
	uint32_t bitrate;
	char     opt25[26];
	char     opt50[51];
};

struct flac_comment_t
{
	char *title;
	int value_count;
	char *value[];
};

struct flac_picture_t
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

struct ocpfilehandle_t;

OCP_INTERNAL void flacMetaDataLock (void);
extern OCP_INTERNAL struct flac_comment_t **flac_comments;
extern OCP_INTERNAL int                     flac_comments_count;
extern OCP_INTERNAL struct flac_picture_t  *flac_pictures;
extern OCP_INTERNAL int                     flac_pictures_count;
OCP_INTERNAL void flacMetaDataUnlock (void);

struct cpifaceSessionAPI_t;
OCP_INTERNAL int flacOpenPlayer (struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void flacClosePlayer (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void flacIdle (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void flacSetLoop (uint8_t s);
OCP_INTERNAL int flacIsLooped (void);
OCP_INTERNAL void flacGetInfo (struct flacinfo *);
OCP_INTERNAL uint64_t flacGetPos (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void flacSetPos (uint64_t pos);

OCP_INTERNAL void FlacInfoInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void FlacInfoDone (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void FlacPicInit (struct cpifaceSessionAPI_t *cpifaceSession);
OCP_INTERNAL void FlacPicDone (struct cpifaceSessionAPI_t *cpifaceSession);

#endif
