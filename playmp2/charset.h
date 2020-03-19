#ifndef _CHARSET_H
#define _CHARSET_H 1

struct charset_t
{
	uint_fast32_t (*strlen_with_term)(uint8_t *src, uint_fast32_t available_space, int require_term); /* return (uint_fast32_t)(-1) if termination is missing */
	void (*readstring)(const uint8_t *source, uint_fast32_t sourcelength, char *target, int targetlength);
	const char *name;
};
#define MAX_CHARSET_N 4
extern struct charset_t __attribute__ ((visibility ("internal"))) id3v2_charsets[MAX_CHARSET_N];

#endif
