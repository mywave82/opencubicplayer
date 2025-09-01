#ifndef _STUFF_UTF_8_H
#define _STUFF_UTF_8_H 1

#include <stdint.h>

/* returns non-zero if stream is broken... *length tells how many bytes was consumed, even if stream is broken */
uint32_t utf8_decode (const char *_src, size_t srclen, int *inc);

/* returns number of characters needed, excluding zero-termination */
int utf8_encoded_length (uint32_t codepoint);

/* returns number of characters used...  up to 6 + NULL terminator */
int utf8_encode (char *dst, uint32_t code);

void displaystr_utf8_overflowleft (uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len);

/* zero-terminated fixed buffer */
/* return values:
 *  -1 - cancelled
 *   0 - finished
 *   1 - call again
 */
int EditStringUTF8z(unsigned int y, unsigned int x, unsigned int w, int l, char *s);

/* zero-terminated, forever long dynamic buffer */
int EditStringUTF8(unsigned int y, unsigned int x, unsigned int w, char **s);

/* zero-terminated, forever long dynamic buffer */
int EditStringASCII(unsigned int y, unsigned int x, unsigned int w, char **s);

char *utf8_casefold (const char *src);

#endif
