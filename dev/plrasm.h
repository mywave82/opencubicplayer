#ifndef _PLRASM_H
#define _PLRASM_H

extern void plrMono16ToStereo16(int16_t *buf, int len);

extern void plrClearBuf(void *buf, int len, int unsign);

extern void plrConvertBuffer (void *dstbuf, int16_t *srcbuf, int samples, int to16bit, int tosigned, int tostereo, int revstereo);

#endif
