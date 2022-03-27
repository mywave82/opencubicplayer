#ifndef _MCHASM_H
#define _MCHASM_H

typedef uint32_t (*mixAddAbsfn)(const void *ch, uint32_t len);

extern uint32_t mixAddAbs16SS(const void *ch, uint32_t len);

typedef void (*mixGetMasterSamplefn)(int16_t *dst, const void *src, uint32_t len, uint32_t step);

extern void mixGetMasterSampleSS16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);

#endif
