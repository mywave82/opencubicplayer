#ifndef _STUFF_UTF_8_H
#define _STUFF_UTF_8_H 1

#include <stdint.h>

/* returns non-zero if stream is broken... *length tells how many bytes was consumed, even if stream is broken */
int utf8_decode (const char *_src, size_t srclen, int *inc);

/* returns number of characters used...  up to 6 + NULL terminator */
int utf8_encode (char *dst, uint32_t code);

#endif
