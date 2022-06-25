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

extern void __attribute__ ((visibility ("internal"))) flacMetaDataLock(void);
extern struct flac_comment_t __attribute__ ((visibility ("internal"))) **flac_comments;
extern int                   __attribute__ ((visibility ("internal")))   flac_comments_count;
extern struct flac_picture_t __attribute__ ((visibility ("internal")))  *flac_pictures;
extern int                   __attribute__ ((visibility ("internal")))   flac_pictures_count;
extern void __attribute__ ((visibility ("internal"))) flacMetaDataUnlock(void);

struct cpifaceSessionAPI_t;
extern int __attribute__ ((visibility ("internal"))) flacOpenPlayer(struct ocpfilehandle_t *, struct cpifaceSessionAPI_t *cpiSessionAPI);
extern void __attribute__ ((visibility ("internal"))) flacClosePlayer(void);
extern void __attribute__ ((visibility ("internal"))) flacIdle(void);
extern void __attribute__ ((visibility ("internal"))) flacSetLoop(uint8_t s);
extern int __attribute__ ((visibility ("internal"))) flacIsLooped(void);
extern void __attribute__ ((visibility ("internal"))) flacPause(int p);
extern void __attribute__ ((visibility ("internal"))) flacGetInfo(struct flacinfo *);
extern uint64_t __attribute__ ((visibility ("internal"))) flacGetPos(void);
extern void __attribute__ ((visibility ("internal"))) flacSetPos(uint64_t pos);

extern void __attribute__ ((visibility ("internal"))) FlacInfoInit (void);
extern void __attribute__ ((visibility ("internal"))) FlacInfoDone (void);

extern void __attribute__ ((visibility ("internal"))) FlacPicInit (void);
extern void __attribute__ ((visibility ("internal"))) FlacPicDone (void);

#endif
