/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * PNG decoder using libpng
 */

#ifndef _CPIFACE_PNG_H
#define _CPIFACE_PNG_H 1

int try_open_png (uint16_t *width, uint16_t *height, uint8_t **data_bgra, const uint8_t *src, uint_fast32_t srclen);

#endif
