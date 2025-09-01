#ifndef _STUFF_UTF_16_H
#define _STUFF_UTF_16_H 1

#include <stdint.h>

/* returns non-zero if stream is broken... *length tells how many words was consumed, even if stream is broken */
uint32_t utf16_decode (const uint16_t *src, size_t srclen, int *inc);

/* returns number of words needed, excluding zero-termination */
int utf16_encoded_length (uint32_t codepoint);

/* returns number of words used...  up to 2 + NULL terminator */
int utf16_encode (uint16_t *dst, uint32_t code);

char *utf16_to_utf8(const uint16_t *src);

uint16_t *utf8_to_utf16(const char *src);

uint16_t *utf8_to_utf16(const char *src);

#if defined(_WIN32) || defined(TESTING)
uint16_t *utf8_to_utf16_LFN(const char *src, const int slashstar);
#endif


#endif
