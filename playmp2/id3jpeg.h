#ifndef _PLAYMP_ID3JPEG_H
#define _PLAYMP_ID3JPEG_H 1

struct ID3_pic_t;

int __attribute__ ((visibility ("internal"))) try_open_jpeg (struct ID3_pic_t *dst, const uint8_t *src, uint_fast32_t srclen);

#endif
