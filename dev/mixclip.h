#ifndef MIXCLIP_H

extern void mixClipAlt(uint16_t *dst, const uint16_t *src, uint32_t len, const uint16_t *tab);
extern void mixClipAlt2(uint16_t *dst, const uint16_t *src, uint32_t len, const uint16_t *tab);
extern void mixCalcClipTab(uint16_t *ct, int32_t amp);

#endif
