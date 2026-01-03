/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * JPEG decoder, using libjpeg/libturbojpeg
 */

#ifndef _CPIFACE_JPEG_H
#define _CPIFACE_JPEG_H 1

int try_open_jpeg (uint16_t *width, uint16_t *height, uint8_t **data_bgra, const uint8_t *src, uint_fast32_t srclen);

#endif
