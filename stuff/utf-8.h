#ifndef _STUFF_UTF_8_H
#define _STUFF_UTF_8_H 1

#include <stdint.h>

/* returns non-zero if stream is broken... *length tells how many bytes was consumed, even if stream is broken */
uint32_t utf8_decode (const char *_src, size_t srclen, int *inc);

/* returns number of characters used...  up to 6 + NULL terminator */
int utf8_encode (char *dst, uint32_t code);

void displaystr_utf8_overflowleft (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);

int EditStringUTF8(unsigned int y, unsigned int x, unsigned int w, char **s);

#endif
