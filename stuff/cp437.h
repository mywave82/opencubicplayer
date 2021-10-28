#ifndef _STUFF_CP437_H

/* Tables to convert codepage 437 into Unicode (UTF-8).
 *
 * OpenCubicPlayer on real "hardware" - VSCA, and all 8x8 and 4x4 charsets
 * at the moment uses a modified version, to fit volume-bars for the analyzer.
 */

extern const uint32_t ocp_cp437_to_unicode[256];
extern const uint16_t cp437_to_unicode[256];

/* destination is not zero-terminated if full */
void utf8_to_cp437(const char *src, size_t srclen, char *dst, size_t dstlen);

/* destination is not zero-terminated if full */
/* fixed buffer to zero-terminated buffer */
void cp437_f_to_utf8_z(const char *src, size_t srclen, char *dst, size_t dstlen);

#endif
