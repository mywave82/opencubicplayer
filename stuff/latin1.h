#ifndef _STUFF_LATIN1_H

/* table to convert latin1 into OCP style cp437 */
extern const uint8_t latin1_table[256];

/* table to convert latin1 into Unicode (UTF-8) */
extern const uint16_t latin1_to_unicode[256];

void latin1_f_to_utf8_z (const char *src, size_t srclen, char *dst, size_t dstlen);

#endif
