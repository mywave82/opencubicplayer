#ifndef MIXASM_H
#define MIXASM_H

extern void mixAddAbs(const struct mixchannel *ch, uint32_t len, uint32_t *l, uint32_t *r);
extern void mixPlayChannel(int32_t *buf, uint32_t len, struct mixchannel *ch, int st);
extern void mixClip(int16_t *dst, const int32_t *src, uint32_t len, int16_t (*tab)[256], int32_t max);

extern int8_t (*mixIntrpolTab)[256][2];
extern int16_t (*mixIntrpolTab2)[256][2];

#endif
