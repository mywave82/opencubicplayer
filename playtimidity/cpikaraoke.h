#ifndef _PLAYTIMIDITY_CPIKARAOKE_H
#define _PLAYTIMIDITY_CPIKARAOKE_H 1

struct cpifaceSessionAPI_t;

struct syllable_t
{
	uint32_t timecode;
	unsigned int measuredwidth;
	char text[];
};
struct line_t
{
	unsigned int        is_paragraph;
	unsigned int        syllables;
	unsigned int        measuredwidth;
	struct syllable_t **syllable;
};
struct lyric_t
{
	unsigned int   lines;
	struct line_t *line;
};

OCP_INTERNAL int karaoke_new_line (struct lyric_t *lyric);

OCP_INTERNAL int karaoke_new_paragraph (struct lyric_t *lyric);

OCP_INTERNAL int karaoke_new_syllable (struct cpifaceSessionAPI_t *cpifaceSession, struct lyric_t *lyric, uint32_t timecode, const char *src, int length);

OCP_INTERNAL void karaoke_clear (struct lyric_t *lyric);

OCP_INTERNAL void cpiKaraokeInit (struct cpifaceSessionAPI_t *cpifaceSession, struct lyric_t *lyric);
OCP_INTERNAL void cpiKaraokeDone (struct cpifaceSessionAPI_t *cpifaceSession);

OCP_INTERNAL void cpiKaraokeSetTimeCode (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t timecode);

#endif
