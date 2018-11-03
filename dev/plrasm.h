#ifndef _PLRASM_H
#define _PLRASM_H

extern void plr16to8(uint8_t *, const uint16_t *, unsigned long);
extern void plrMono16ToStereo16(int16_t *buf, int len);
extern void plrClearBuf(void *buf, int len, int unsign);

#endif
