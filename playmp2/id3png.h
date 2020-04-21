#ifndef _PLAYMP_ID3PNG_H
#define _PLAYMP_ID3PNG_H 1

struct ID3_pic_t;

int __attribute__ ((visibility ("internal"))) try_open_png (struct ID3_pic_t *dst, uint8_t *src, uint_fast32_t srclen);

#endif
