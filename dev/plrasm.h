#ifndef _PLRASM_H
#define _PLRASM_H

extern void plrMono16ToStereo16(int16_t *buf, int len);

extern void plrClearBuf(void *buf, int len, int unsign);

/* buflen, oldpos and newpos are per byte, not per sample */
extern void plrConvertBuffer (int16_t *playbuf, void *shadowbuf, unsigned int buflen, unsigned int oldpos, unsigned int newpos, int to16bit, int tosigned, int tostereo, int revstereo);

#endif
