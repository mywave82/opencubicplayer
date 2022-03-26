#ifndef _MCHASM_H
#define _MCHASM_H

typedef uint32_t (*mixAddAbsfn)(const void *ch, uint32_t len);

extern uint32_t mixAddAbs16M(const void *ch, uint32_t len);
extern uint32_t mixAddAbs16MS(const void *ch, uint32_t len);
extern uint32_t mixAddAbs16S(const void *ch, uint32_t len);
extern uint32_t mixAddAbs16SS(const void *ch, uint32_t len);

extern uint32_t mixAddAbs8M(const void *ch, uint32_t len);
extern uint32_t mixAddAbs8MS(const void *ch, uint32_t len);
extern uint32_t mixAddAbs8S(const void *ch, uint32_t len);
extern uint32_t mixAddAbs8SS(const void *ch, uint32_t len);


typedef void (*mixGetMasterSamplefn)(int16_t *dst, const void *src, uint32_t len, uint32_t step);

extern void mixGetMasterSampleMS8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleMU8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleMS8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleMU8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSS8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSU8M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSS8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSU8S(int16_t *dst, const void *src, uint32_t len, uint32_t step);

extern void mixGetMasterSampleMS16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleMU16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleMS16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleMU16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSS16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSU16M(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSS16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);
extern void mixGetMasterSampleSU16S(int16_t *dst, const void *src, uint32_t len, uint32_t step);

#endif
